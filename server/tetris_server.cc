#include "algorithm.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "filter.h"
#include "mapping.h"
#include "util/path_util.h"
#include "util/socket.h"
#include "util/string_util.h"
#include "util/tetris.h"
#include "proto/tetris.pb.h"
#include "util/protobuf_util.h"
#include "json.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>



/***
 * Global variables
 ***/

using ConnectionPtr = std::shared_ptr<Connection>;

const static int MAXEVENTS = 100;
debug::LoggerPtr logger;


/***
 * Failure handling for no mapping found
 ***/

class NoMappingError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};


/***
 * Client Program
 ***/

class Client
{
public:
    struct Thread
    {
        std::string name;
        int tid;
        CPUList cpus;

        Thread(const std::string &name, int tid, CPUList cpus) :
                name{name}, tid{tid}, cpus{cpus}
        {}
    };

    class Comp
    {
    private:
        std::string _criteria;
        bool _more_is_better;
        std::function<bool(const double, const double)> _comp;

    public:
        Comp(const std::string compare_criteria, bool compare_more_is_better) :
                _criteria{compare_criteria}, _more_is_better{compare_more_is_better}
        {
            if (_more_is_better)
                _comp = std::greater<double>{};
            else
                _comp = std::less<double>{};
        }

        Comp() :
                _criteria{}, _comp{std::less<double>()}
        {}

        bool operator()(const Mapping &other, const Mapping &best)
        {
            return _comp(other.characteristic(_criteria), best.characteristic(_criteria));
        }

        std::string criteria() const
        {
            return _criteria;
        }

        std::string repr() const
        {
            std::stringstream ss;
            ss << _criteria << "(" << (_more_is_better ? ">" : "<") << ")";

            return ss.str();
        }
    };

public:
    ConnectionPtr connection;
    std::string exec;
    int pid;
    bool dynamic_client;
    std::vector<Thread> threads;
    std::vector<Mapping> mappings;
    Mapping active_mapping;
    bool using_dppm;

    Filter filter;
    Comp comp;

public:
    Client(const Client &) = delete;

    Client(const ConnectionPtr &conn) :
            connection{conn}, exec{}, pid{-1}, dynamic_client{false}, threads{}, mappings{}, active_mapping{},
            using_dppm{false}, filter{}, comp{}
    {}

    ~Client()
    {
        if (pid != -1)
            logger->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
    }

    CPUList cpus() const
    {
        return active_mapping.cpus;
    }

    void update_mapping(const Mapping &new_mapping)
    {
        if (new_mapping.name == active_mapping.name)
            return;

        logger->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid, new_mapping.name.c_str());
        active_mapping = new_mapping;

        for (auto &t : threads) {
            CPUList cpus;
            if (dynamic_client)
                cpus = active_mapping.cpus;
            else
                cpus = active_mapping.cpu(t.name);

            logger->info(" * remap thread '%s' [%i] from cpu(s) %s to cpu(s) %s\n", t.name.c_str(), t.tid,
                         string_util::join(t.cpus.cpulist(num_cpus), ",").c_str(),
                         string_util::join(cpus.cpulist(num_cpus), ",").c_str());

            t.cpus = cpus;

            cpu_set_t mask = cpus.cpu_set();
            if (sched_setaffinity(t.tid, sizeof(cpu_set_t), &mask) != 0)
                logger->warning("Failed to set cpu affinity for thread '%s': %s\n", t.name.c_str(), strerror(errno));
        }

        // If the client is using DPM, update potential regions.
        if (using_dppm)
            update_mapping_regions();

        logger->info(" * done\n");
    }

    void update_mapping_regions() {
        // Update parallel regions configuration.
        if (!active_mapping.region_map.empty()) {
            auto response_with_tids = set_parallel_regions_number_num_replicas();
            set_parallel_regions_cpu_affinities(response_with_tids);
        }
    }

    tetris::PushResponse set_parallel_regions_number_num_replicas() const
    {
        // Connect to the push listener.
        std::stringstream path{};
        path << "/tmp/tetris_push_listener_" << pid;
        Connection conn(path.str());
        tetris::PushRequest request{};
        tetris::PushResponse response{};
        request.set_type(tetris::PushRequest::DPM_UPDATE_CONFIGURATION);
        request.set_feature_id(0);
        // Set the number of replicas in all parallel regions according to the mapping.
        for (const auto &[region_name, replicas] : active_mapping.region_map) {
            auto configuration = request.add_region_configurations();
            configuration->set_name(region_name);
            configuration->set_num_replicas(replicas.size());
            logger->info(" * set %d replicas in parallel region '%s'\n", replicas.size(), region_name.c_str());
        }
        // Send the push request.
        protobuf_util::Send(conn.locked(), request);
        // Wait for the response.
        protobuf_util::Receive(conn.locked(), response);
        if (response.type() != tetris::PushResponse::DPM_REGION_INFO)
            logger->error("Failed to set the number of replicas for parallel region\n");
        return response;
    }

    void set_parallel_regions_cpu_affinities(const tetris::PushResponse& response) const
    {
        // Set CPU affinity for each process in each replica of each parallel region.
        auto regions_info = response.region_infos();
        for (auto &region_info : regions_info) {
            auto region_name = region_info.name();
            auto replica_affinities = active_mapping.region_map.at(region_name);
            int replica_number = 0;
            for (auto &replica : region_info.replicas()) {
                auto process_affinities = replica_affinities.back();
                replica_affinities.pop_back();
                ++replica_number;
                for (auto &process : replica.process_thread()) {
                    CPUList cpus;
                    auto process_name = process.process_name();
                    auto tid = process.thread_id();
                    if (dynamic_client)
                        cpus = active_mapping.cpus;
                    else
                        cpus = CPUList({process_affinities.at(process_name)});
                    logger->info(" * map thread '%s::%s@%d' [%i] to cpu(s) %s\n", region_name.c_str(),
                                 process_name.c_str(), replica_number, tid,
                                 string_util::join(cpus.cpulist(num_cpus), ",").c_str());
                    cpu_set_t mask = cpus.cpu_set();
                    if (sched_setaffinity(tid, sizeof(cpu_set_t), &mask) != 0)
                        logger->warning("Failed to set cpu affinity for thread '%s::%s': %s\n", region_name.c_str(),
                                        process_name.c_str(), strerror(errno));
                }
            }
        }
    }

    void new_thread(const std::string &name, int tid)
    {
        logger->info("New thread '%s' [%i] registered for client '%s' [%d]\n", name.c_str(), tid, exec.c_str(), pid);

        CPUList cpus;
        if (dynamic_client) {
            cpus = active_mapping.cpus;
            logger->info(" * enabled cpu(s) %s (dynamic client)\n",
                         string_util::join(cpus.cpulist(num_cpus), ",").c_str());
        } else {
            cpus = active_mapping.cpu(name);
            logger->info(" * enabled cpu(s) %s\n", string_util::join(cpus.cpulist(num_cpus), ",").c_str());
        }

        auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.name == name; });
        if (it == threads.end()) {
            threads.emplace_back(name, tid, cpus);

            cpu_set_t mask = cpus.cpu_set();
            if (sched_setaffinity(tid, sizeof(cpu_set_t), &mask) != 0)
                logger->warning("Failed to set cpu affinity for thread '%s': %s\n", name.c_str(), strerror(errno));
        } else
            logger->warning("Duplicate thread '%s'\n", name.c_str());
    }
};


/***
 * Client Manager
 ***/

class Manager
{
private:
    std::map<int, Client> _clients;
    std::string _mappings_path;
    std::map<std::string, std::vector<Mapping>> _mappings;

    CPUList _blocked_cpus;

    const std::string knob_description_filename = "__confdefs__.json";

    KnobDescription parse_knob_description(const std::string &dir) const
    {
        std::stringstream confdefs_filepath_stream{};
        confdefs_filepath_stream << dir << "/" << knob_description_filename;
        std::ifstream json_knob_file{confdefs_filepath_stream.str()};
        nlohmann::json json_knob;
        json_knob_file >> json_knob;
        return KnobDescription(json_knob);
    }

    std::vector<Mapping> parse_mappings(const std::string &dir)
    {
        std::vector<Mapping> mappings;
        auto knob_description = parse_knob_description(dir);
        // If the knob description is empty, return zero mapping.
        if (!knob_description.is_valid()) {
            logger->warning("Knob description file '%s' is using an incorrect format.\n", dir.c_str());
            return mappings;
        }

        try {
            path_util::for_each_file(dir, [&](const std::string &file) -> void {
                if ((path_util::extension(file) == ".json") && (path_util::basename(file) != knob_description_filename)) {
                    // Parse the JSON mapping file.
                    std::ifstream json_mapping_file{file};
                    nlohmann::json json_mapping;
                    json_mapping_file >> json_mapping;
                    auto parsed_mapping = parse_mapping(json_mapping);
                    // Check if the mapping is valid, otherwise discard it.
                    if (parsed_mapping.is_valid(knob_description))
                        mappings.emplace_back(parsed_mapping);
                    else
                        logger->warning("Mapping file '%s' does not comply to the knob description of %s.\n",
                                        path_util::basename(file).c_str(), path_util::basename(dir).c_str());
                }
            });
        } catch (std::exception &e) {
            logger->error("Reading mappings failed with: %s\n", e.what());
        }

        if (!mappings.empty()) {
            std::vector<std::string> thread_names;
            std::vector<std::string> characteristic_names;
            Mapping mapping = mappings.back();
            // Add regular processes names.
            for (const auto &key_val : mapping.thread_map)
                thread_names.push_back(key_val.first);
            // Add parallel regions and inside processes names.
            for (const auto & [region_name, replicas] : mapping.region_map) {
                for (const auto &key_val : replicas.back()) {
                    auto process_name = key_val.first;
                    thread_names.push_back(region_name + "::" + key_val.first);
                }
            }
            for (const auto &key_val : mapping.characteristics_map)
                characteristic_names.push_back(key_val.first);

            logger->debug("  * Found %i mapping(s)\n", mappings.size());
            logger->debug("  |-> %i thread(s): %s\n", thread_names.size(),
                          string_util::join(thread_names, ",").c_str());
            logger->debug("  |-> %i characteristic(s): %s\n", characteristic_names.size(),
                          string_util::join(characteristic_names, ",").c_str());

            for (const auto &m : mappings) {
                std::vector<std::string> mapping_characteristics;

                for (const auto &c : characteristic_names) {
                    std::stringstream ss;

                    ss << std::setprecision(0) << std::fixed << c << ":" << m.characteristic(c);
                    mapping_characteristics.push_back(ss.str());
                }

                logger->debug("  |=> %s [%s] %s\n", m.name.c_str(),
                              m.equivalence_class().name().c_str(),
                              string_util::join(mapping_characteristics, ",").c_str());
            }
        }

        return mappings;
    }

    Mapping parse_mapping(const nlohmann::json &json_mapping)
    {
        std::vector<std::pair<std::string, std::string>> threads;
        RegionAffinities<std::string> regions{};
        std::vector<std::pair<std::string, std::string>> characteristics;
        /* Get all processes mapping information. */
        for (const auto &mapping : json_mapping["mapping"]) {
            if (mapping["type"] == "process") {
                /* Regular process. */
                std::string thread_name = mapping["name"];
                std::string cpu_name = mapping["core"];
                threads.emplace_back(thread_name, cpu_name);
            } else if (mapping["type"] == "DLP") {
                /* Parallel region. */
                ReplicaAffinities<std::string> replica_affinities{};
                for (const auto& replica : mapping["replicas"]) {
                    ProcessAffinities<std::string> process_affinities{};
                    for (const auto &process : replica) {
                        std::string process_name = process["name"];
                        std::string cpu_name = process["core"];
                        process_affinities.emplace(process_name, cpu_name);
                    }
                    replica_affinities.push_back(process_affinities);
                }
                regions.emplace(mapping["name"], replica_affinities);
            }
        }
        /* Get name of the mapping. */
        auto name = json_mapping["name"];
        /* All the other attributes are characteristics of the mapping */
        for (const auto &item : json_mapping.items()) {
            const auto attribute = item.key();
            if (attribute != "name" && attribute != "mapping") {
                characteristics.emplace_back(attribute, json_mapping[attribute]);
            }
        }
        return Mapping{name, threads, regions, characteristics};
    }

    Mapping select_best_mapping(Client &c)
    {
        logger->info("Search for best mapping for '%s' [%d] using criteria %s\n", c.exec.c_str(), c.pid,
                     c.comp.repr().c_str());

        /* First go through all mappings and take those that satisfy our filter criteria */
        auto filter = [&c](const Mapping &m) -> bool {
            return c.filter(m);
        };

        std::vector<Mapping> possible_mappings;
        for (const auto &m : c.mappings) {
            if (filter(m))
                possible_mappings.push_back(m);
            else
                logger->debug(" * Mapping %s (%.0f@%s) [%s] doesn't satisfy filter criteria %s: %s=%f\n",
                              m.name.c_str(), m.characteristic(c.comp.criteria()), c.comp.criteria().c_str(),
                              m.equivalence_class().name().c_str(), c.filter.repr().c_str(),
                              c.filter.criteria().c_str(), m.characteristic(c.filter.criteria()));
        }


        if (possible_mappings.empty()) {
            logger->debug("No mappings are available for client '%s' [%i] that satisfy the filter\n", c.exec.c_str(),
                          c.pid);
            throw NoMappingError("Can't find mapping that satisfies the filter.");
        } else
            logger->debug(" * There are %i mapping(s) for this client that satisfy the filter\n",
                          possible_mappings.size());

        /* Now get all the mappings (containing equivalent ones) from the possible ones,
         * that still fit on the non-occupied CPUs. */
        CPUList occupied_cpus = _blocked_cpus;
        for (const auto&[name, cl] : _clients) {
            if (cl.pid == c.pid)
                continue;

            occupied_cpus |= cl.cpus();
        }

        if (occupied_cpus.nr_cpus() == 0)
            logger->debug(" * Already taken cpu(s): none\n");
        else
            logger->debug(" * Already taken cpu(s): %s\n",
                          string_util::join(occupied_cpus.cpulist(num_cpus), ",").c_str());

        /* Get all the TETRiS mappings for this client */
        auto possible_tetris_mappings = tetris_mappings(possible_mappings, occupied_cpus);
        if (possible_tetris_mappings.empty()) {
            logger->debug("No TETRiS mappings are available for client '%s' [%i] that fit the available cpu(s)\n",
                          c.exec.c_str(), c.pid);
            throw NoMappingError("Can't find a proper TETRiS mapping for the client.");
        } else
            logger->debug(" * There are %i TETRiS mapping(s) for this client that fit the available cpu(s)\n",
                          possible_tetris_mappings.size());

        /* Now select the best one out of the remaining ones. */
        auto comp = [&c](const Mapping &other, const Mapping &best) -> bool {
            return c.comp(other, best);
        };

        auto best = possible_tetris_mappings.begin();
        logger->debug(" * Start search with mapping: %s (%.0f@%s) [%s]\n", best->name.c_str(),
                      best->characteristic(c.comp.criteria()), c.comp.repr().c_str(),
                      best->equivalence_class().name().c_str());

        for (auto m = best; m != possible_tetris_mappings.end(); ++m) {
            if (filter(*m) && comp(*m, *best)) {
                logger->debug(" * Found better mapping: %s (%.0f@%s) [%s] vs %s (%.0f@%s) [%s]\n",
                              m->name.c_str(), m->characteristic(c.comp.criteria()),
                              c.comp.repr().c_str(), m->equivalence_class().name().c_str(),
                              best->name.c_str(), best->characteristic(c.comp.criteria()),
                              c.comp.repr().c_str(), best->equivalence_class().name().c_str());

                /* Remember this one as best one */
                best = m;
            }
        }

        logger->info("The best mapping: %s (%.0f@%s) [%s]\n", best->name.c_str(),
                     best->characteristic(c.comp.criteria()), c.comp.repr().c_str(),
                     best->equivalence_class().name().c_str());

        return *best;
    }

    Mapping use_preferred_mapping(Client &c, const std::string &preferred_mapping_name)
    {
        logger->info("Use preferred mapping '%s' for '%s' [%d]\n", preferred_mapping_name.c_str(), c.exec.c_str(),
                     c.pid);

        auto it = std::find_if(c.mappings.begin(), c.mappings.end(),
                               [&](const auto &m) { return m.name == preferred_mapping_name; });
        if (it != c.mappings.end())
            return *it;
        else {
            logger->info("Couldn't find preferred mapping\n");
            return select_best_mapping(c);
        }
    }

public:
    explicit Manager(const std::string &mappings_path) :
            _clients{}, _mappings_path{mappings_path}, _mappings{}
    {
        update_mappings();
    }

    void client_connect(int fd, const ConnectionPtr &conn)
    {
        _clients.emplace(fd, conn);
    }

    void client_disconnect(int fd)
    {
        _clients.erase(fd);
    }

    void remap(int fd, const std::string &preferred_mapping_name)
    try
    {
        Client &c = _clients.at(fd);

        logger->info("Change mapping for client '%s' [%d] to mapping %s\n",
                     c.exec.c_str(), c.pid, preferred_mapping_name.c_str());

        auto it = std::find_if(c.mappings.begin(), c.mappings.end(),
                               [&](const auto &m) { return m.name == preferred_mapping_name; });
        if (it == c.mappings.end()) {
            logger->info("Unknown mapping %s for client %i\n", preferred_mapping_name.c_str(), fd);
            return;
        } else {
            logger->info("Changing mapping for client '%s' [%d] to mapping %s\n",
                         c.exec.c_str(), c.pid, preferred_mapping_name.c_str());
            c.update_mapping(*it);
        }
    } catch (std::out_of_range &) {
        logger->error("Unknown client %i\n", fd);
    }

    bool client_message(int fd)
    try
    {
        Client &c = _clients.at(fd);
        ConnectionPtr conn = c.connection;

        bool done = false;
        bool close = false;

        while (!done) {
            tetris::PullRequest request{};
            auto res = protobuf_util::Receive(conn->locked(), request);
            if (res == Connection::InState::DONE) {
                /* We are done processing. So return. */
                done = true;
            } else if (res == Connection::InState::CLOSED) {
                /* We are done processing and the remote site closed the
                 * connection. */
                close = true;
                done = true;
            } else {
                /* There is some data to process. Handle it. */
                switch (request.type()) {
                    case tetris::PullRequest::TETRIS_NEW_CLIENT: {
                        int pid = request.new_client().pid();
                        std::string exec = string_util::strip(path_util::basename(request.new_client().exec()));
                        bool managed;
                        try {
                            logger->always("New client registered: '%s' [%d] (ID: %d)\n", exec.c_str(), pid, fd);

                            /* Update the client data. */
                            c.pid = pid;
                            c.exec = exec;
                            c.dynamic_client = (request.new_client().mapping_type() == tetris::NewClient::DYNAMIC);
                            c.mappings = _mappings.at(exec);

                            c.comp = Client::Comp(string_util::strip(request.new_client().compare_criteria()),
                                                  request.new_client().compare_more_is_better());

                            logger->info(" * criteria: %s\n", c.comp.repr().c_str());

                            if (request.new_client().has_filter_criteria())
                                c.filter = Filter(request.new_client().filter_criteria());

                            logger->info(" * filter: %s\n", c.filter.repr().c_str());

                            if (request.new_client().has_preferred_mapping()) {
                                std::string preferred_mapping = string_util::strip(
                                        request.new_client().preferred_mapping());
                                c.update_mapping(use_preferred_mapping(c, preferred_mapping));
                            } else {
                                c.update_mapping(select_best_mapping(c));
                            }

                            logger->info(" * mapping: %s (%.0f@%s) [%s]\n", c.active_mapping.name.c_str(),
                                         c.active_mapping.characteristic(c.comp.criteria()), c.comp.repr().c_str(),
                                         c.active_mapping.equivalence_class().name().c_str());
                            logger->info(" * thread placement: %s\n", c.dynamic_client ? "CFS" : "static");

                            /* Add the main thread to the client */
                            c.new_thread("@main", c.pid);

                            /* We will manage this client. */
                            managed = true;
                        } catch (std::out_of_range &) {
                            logger->error("Unknown client: '%s' [%i]\n", exec.c_str(), pid);
                            managed = false;
                        } catch (NoMappingError &) {
                            logger->warning("Couldn't find a proper mapping for client: '%s' [%i]\n", exec.c_str(),
                                            pid);
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        tetris::PullResponse ack{};
                        ack.set_type(tetris::PullResponse::TETRIS_NEW_CLIENT_ACK);
                        ack.mutable_new_client_ack()->set_id(fd);
                        ack.mutable_new_client_ack()->set_managed(managed);

                        if (protobuf_util::Send(conn->locked(), ack) != Connection::OutState::DONE) {
                            logger->error("Failed to acknowledge the new-client message\n");
                            managed = false;
                        }

                        /* If we don't manage this client we can close its connection. */
                        close = !managed;
                        break;
                    }
                    case tetris::PullRequest::TETRIS_NEW_THREAD: {
                        int tid = request.new_thread().tid();
                        std::string name = request.new_thread().name();
                        bool managed;
                        try {
                            /* Update the client data. */
                            c.new_thread(name, tid);
                            managed = true;
                        } catch (std::out_of_range) {
                            logger->error("Unknown thread: '%s' [%i] for client '%s'\n", name.c_str(), tid,
                                          c.exec.c_str());
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        tetris::PullResponse response{};
                        response.set_type(tetris::PullResponse::TETRIS_NEW_THREAD_ACK);
                        response.mutable_new_thread_ack()->set_managed(managed);

                        if (protobuf_util::Send(conn->locked(), response) != Connection::OutState::DONE)
                            logger->error("Failed to acknowledge the new-thread message\n");

                        break;
                    }
                    case tetris::PullRequest::DPM_SUBSCRIBE: {
                        logger->always("Client '%s' [%d] use DPM\n", c.exec.c_str(), c.pid);
                        c.using_dppm = true;

                        /* We need to acknowledge this message. */
                        tetris::PullResponse ack{};
                        ack.set_type(tetris::PullResponse::ACKNOWLEDGE);
                        ack.set_feature_id(0);
                        if (protobuf_util::Send(conn->locked(), ack) != Connection::OutState::DONE) {
                            logger->error("Failed to acknowledge the DPM registration message\n");
                            c.using_dppm = false;
                        }
                        break;
                    }
                    case tetris::PullRequest::DPM_SEND_APPLICATION_THREAD_ID: {
                        logger->always("Client '%s' [%d] send it threads ID\n", c.exec.c_str(), c.pid);

                        auto &application_threads_id = request.application_threads_id();
                        auto &regular_process_info = application_threads_id.process_info();

                        bool managed = true;
                        std::string process_name;
                        pthread_t process_tid;
                        // Register all regular processes into the TETRiS manager.
                        try {
                            for (auto &process: regular_process_info) {
                                process_name = process.process_name();
                                process_tid = process.thread_id();
                                c.new_thread(process_name, process_tid);
                            }
                        } catch (std::out_of_range) {
                            logger->error("Unknown thread: '%s' [%i] for client '%s'\n", process_name.c_str(), process_tid,
                                          c.exec.c_str());
                            managed = false;
                        }

                        c.update_mapping_regions();

                        /* We need to acknowledge this message. */
                        tetris::PullResponse ack{};
                        // If all regular processes are managed by TETRiS, set type to ACKNOWLEDGE, otherwise set it to
                        // ERROR.
                        ack.set_type(managed ? tetris::PullResponse::ACKNOWLEDGE : tetris::PullResponse::ERROR);
                        ack.set_feature_id(0);
                        if (protobuf_util::Send(conn->locked(), ack) != Connection::OutState::DONE)
                            logger->error("Failed to acknowledge the DPM registration message\n");

                        break;
                    }
                    default:
                        logger->warning("Other message received\n");
                }
            }
        }
        return close;
    } catch (std::out_of_range) {
        logger->warning("Received message for unknown client %i\n", fd);
        return true;
    } catch (std::runtime_error &e) {
        logger->warning("Error working with message for client %i: %s", fd, e.what());
        return true;
    }

    void control_message(ControlData &data)
    try
    {
        switch (data.op) {
            case ControlData::Operations::UPDATE_CLIENT: {
                Client &c = _clients.at(data.update_data.client_fd);

                logger->info("Update client: '%s' [%d]\n", c.exec.c_str(), c.pid);

                /* Update the client's options according to the given new
                 * values and select a new mapping based on the new criteria. */
                if (data.update_data.has_dynamic_client) {
                    c.dynamic_client = data.update_data.dynamic_client;

                    logger->info(" * change thread placement: %s\n", c.dynamic_client ? "CFS" : "static");
                }

                if (data.update_data.has_compare_criteria) {
                    c.comp = Client::Comp(string_util::strip(data.update_data.compare_criteria),
                                          data.update_data.compare_more_is_better);

                    logger->info(" * change criteria: %s\n", c.comp.repr().c_str());
                }

                if (data.update_data.has_filter_criteria) {
                    c.filter = Filter(data.update_data.filter_criteria);

                    logger->info(" * change filter: %s\n", c.filter.repr().c_str());
                }

                if (data.update_data.has_preferred_mapping) {
                    std::string preferred_mapping = string_util::strip(data.update_data.preferred_mapping);
                    c.update_mapping(use_preferred_mapping(c, preferred_mapping));
                } else {
                    c.update_mapping(select_best_mapping(c));
                }

                logger->info(" * mapping: %s (%.0f@%s) [%s]\n", c.active_mapping.name.c_str(),
                             c.active_mapping.characteristic(c.comp.criteria()), c.comp.repr().c_str(),
                             c.active_mapping.equivalence_class().name().c_str());

                break;
            }
            case ControlData::Operations::BLOCK_CPUS:
                logger->info("Update blocked cpus\n");

                _blocked_cpus = data.block_cpus_data.cpus;

                if (_blocked_cpus.nr_cpus() == 0)
                    logger->info(" * blocked: none\n");
                else
                    logger->info(" * blocked: %s\n", string_util::join(_blocked_cpus.cpulist(num_cpus), ",").c_str());
                break;
            default:
                logger->warning("Other control message received\n");
        }
    } catch (std::out_of_range) {
        logger->warning("Received control message for unknown client\n");
    }

    void print_mappings()
    {
        std::cout << "Currently active mappings:" << std::endl
                  << "==========================" << std::endl;
        for (const auto&[name, client] : _clients) {
            std::cout << "Client '" << client.exec << "' [" << client.pid << "] (ID: " << name << ")" << std::endl;
            std::cout << "-> mapping: " << client.active_mapping.name << " ["
                      << client.active_mapping.equivalence_class().name() << "]" << std::endl;

            std::cout << "-> threads:" << std::endl;
            for (const auto &t : client.threads)
                std::cout << "--> " << t.name << "(" << t.tid << "): "
                          << string_util::join(t.cpus.cpulist(num_cpus), ",") << std::endl;
        }
        std::cout << "======= END OF LIST =======" << std::endl;
    }

    void update_mappings()
    {
        logger->info("Update mapping database (%s).\n", _mappings_path.c_str());
        _mappings.clear();

        try {
            path_util::for_each_folder(_mappings_path, [&](const std::string &dir) -> void {
                auto parsed_mappings = parse_mappings(dir);
                if (!parsed_mappings.empty()) {
                    logger->info(" -> found mapping for '%s'\n", path_util::basename(dir).c_str());
                    _mappings.emplace(path_util::basename(dir), parsed_mappings);
                }
            });
        } catch (std::exception &e) {
            logger->error("Reading mappings failed with: %s\n", e.what());
        }
    }
};


void usage()
{
    std::cout << "usage: tetrisserver [-h] [MAPPINGS]" << std::endl
              << std::endl
              << "Options:" << std::endl
              << "   -h, --help           show this help message." << std::endl
              << std::endl
              << "Positionals:" << std::endl
              << " MAPPINGS               path the folder with the per-app mappings." << std::endl;
}

int main(int argc, char *argv[])
{
    /* Parsing command line arguments. */
    std::string mappings_path;

    if (argc > 2) {
        usage();
        return 1;
    } else if (argc == 1) {
        mappings_path = path_util::getcwd();
    } else {
        std::string arg{argv[1]};
        if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else {
            mappings_path = path_util::abspath(path_util::expanduser(arg));
        }
    }

    std::cout << "Welcome to TETRiS" << std::endl;

    /* Setup logging */
    logger = debug::Logger::get();

    /* Setting up the manager */
    Manager manager{mappings_path};

    /* Setting up the server socket */
    Socket server_sock;
    int sock_fd = -1;
    try {
        server_sock.open(SERVER_SOCKET);
        server_sock.non_blocking();
        server_sock.listening();
        sock_fd = server_sock.fd();
    } catch (std::runtime_error &e) {
        std::cerr << "Failed to open socket" << std::endl
                  << e.what() << std::endl;
        return 1;
    }

    /* Setting up control socket */
    Socket ctl_sock;
    int ctl_fd = -1;
    try {
        ctl_sock.open(CONTROL_SOCKET);
        ctl_sock.non_blocking();
        ctl_sock.listening();
        ctl_fd = ctl_sock.fd();
    } catch (std::runtime_error &e) {
        std::cerr << "Failed to open control socket" << std::endl
                  << e.what() << std::endl;
        return 1;
    }

    logger->info(" * Server socket: %s (%i)\n", server_sock.path(), sock_fd);
    logger->info(" * Control socket: %s (%i)\n", ctl_sock.path(), ctl_fd);

    /* Setup signal handling */
    int sig_fd = -1;
    {
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGABRT);
        sigaddset(&sigmask, SIGHUP);
        sigaddset(&sigmask, SIGINT);
        sigaddset(&sigmask, SIGQUIT);
        sigaddset(&sigmask, SIGTERM);
        sigaddset(&sigmask, SIGUSR1);
        sigaddset(&sigmask, SIGUSR2);

        /* First block the signals. */
        sigprocmask(SIG_BLOCK, &sigmask, nullptr);

        /* And create a signal fd where these signals are managed. */
        sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
        if (sig_fd == -1) {
            std::cerr << "Failed to create signal fd." << std::endl
                      << strerror(errno) << std::endl;
            return 1;
        }
    }

    /* Setup the epoll event loop. */
    int epoll_fd = -1;
    {
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            std::cerr << "Failed to initialize epoll." << std::endl
                      << strerror(errno) << std::endl;
            return 1;
        }

        epoll_event e;
        for (auto fd : {sock_fd, ctl_fd, sig_fd}) {
            e.data.fd = fd;
            e.events = EPOLLIN;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
                std::cerr << "Failed to add socket " << fd << " to epoll." << std::endl
                          << strerror(errno) << std::endl;
                return 1;
            }
        }
    }

    /* The event loop */
    epoll_event events[MAXEVENTS];
    bool done = false;

    while (!done) {
        int n;

        n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

        for (int i = 0; i < n; ++i) {
            epoll_event *cur = &events[i];

            if (cur->data.fd == sock_fd) {
                /* There are a new connections at the server socket.
                 * Connect with all of them. */
                while (1) {
                    sockaddr_un in_sock;
                    socklen_t in_sock_size = sizeof(in_sock);
                    int infd = ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We connected to all possible connections already.
                             * Continue with the main loop. */
                            break;
                        } else {
                            logger->error("An error happened while accepting a connection: %s", strerror(errno));
                            break;
                        }
                    }

                    /* Make the new socket non-blocking and add it to epoll. */
                    ConnectionPtr in_conn = std::make_shared<Connection>(infd, in_sock);
                    in_conn->non_blocking();

                    epoll_event e;
                    e.data.fd = infd;
                    e.events = EPOLLIN;

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &e) == -1) {
                        logger->error("Failed to add new connection to epoll: %s", strerror(errno));
                        ::close(infd);
                    } else {
                        logger->info("A new client connected (%i)\n", infd);

                        manager.client_connect(infd, in_conn);
                    }
                }
            } else if (cur->data.fd == ctl_fd) {
                /* There are a new connections at the control socket.
                 * Connect with all of them. */
                while (1) {
                    sockaddr_un in_sock;
                    socklen_t in_sock_size = sizeof(in_sock);
                    int infd = ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We connected to all possible connections already.
                             * Continue with the main loop. */
                            break;
                        } else {
                            logger->error("An error happened while accepting a connection: %s", strerror(errno));
                            break;
                        }
                    }

                    /* Control connection are usually single shot. So just open this connection
                     * and directly read out the data */
                    ControlData cd;
                    Connection(infd, in_sock).read(cd);

                    switch (cd.op) {
                        case ControlData::Operations::UPDATE_MAPPINGS:
                            manager.update_mappings();
                            break;
                        default:
                            /* All the other control messages are directly handled in the manager */
                            manager.control_message(cd);
                    }
                }
            } else if (cur->data.fd == sig_fd) {
                /* There was a signal delivered to this process. */
                while (1) {
                    signalfd_siginfo siginfo;
                    ssize_t count;

                    count = read(sig_fd, &siginfo, sizeof(siginfo));
                    if (count == -1) {
                        if (errno != EAGAIN)
                            logger->error("An error happened while reading data from signal fd: %s", strerror(errno));

                        break;
                    }

                    logger->info("Received a signal (%i)\n", siginfo.ssi_signo);

                    switch (siginfo.ssi_signo) {
                        case SIGUSR1:
                            manager.update_mappings();
                            break;
                        case SIGUSR2:
                            manager.print_mappings();
                            break;
                        default:
                            done = 1;
                    }
                }
            } else if (cur->events & EPOLLIN) {
                /* Some client tried to send us data. */
                logger->debug("The client sent a message\n");

                if (manager.client_message(cur->data.fd)) {
                    manager.client_disconnect(cur->data.fd);
                }
            } else if (cur->events & EPOLLHUP) {
                /* Some client disconnected. */
                manager.client_disconnect(cur->data.fd);
            } else {
                logger->warning("Strange event at %i\n", cur->data.fd);
                ::close(cur->data.fd);
            }
        }
    }

    std::cout << "Exiting" << std::endl;
    ::close(sig_fd);

    return 0;
}
