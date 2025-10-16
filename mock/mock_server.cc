#include <exception>
#include <filesystem>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <fstream>

#include <signal.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "util/platform/reader.h"
#include "util/platform/platform.h"
#include "util/socket.h"
#include "util/string_util.h"
#include "util/protobuf_util.h"
#include "util/connection.h"
#include "util/platform/perf.h"
#include "util/platform/energy.h"


using namespace tetris;

/***
 * Global variables
 ***/

const constexpr int UTILITY_FEATURE_ID = 1;

const static int MAXEVENTS = 100;
debug::LoggerPtr logger;

using ConnectionPtr = std::shared_ptr<Connection>;

/***
 * Support Classes
 ***/
struct CpuTimes {
  uint64_t all;
  std::vector<uint64_t> cores;
};

struct ProcessTimes {
  uint64_t all;
  std::map<pid_t, uint64_t> threads;
};

struct CpuEnergy {
  uint64_t all;
  std::vector<uint64_t> cores;
};

struct ProcessEnergy {
  uint64_t all;
  uint64_t power;
  std::map<pid_t, uint64_t> threads;
};

struct EnergyData {
  std::chrono::high_resolution_clock::time_point time;
  uint64_t total_energy_uj;
  CpuTimes raw_ctimes;

  CpuTimes ctimes;
  CpuEnergy energy;
};

struct ProcessEnergyData {
  std::chrono::high_resolution_clock::time_point time;
  std::chrono::high_resolution_clock::duration update_interval;
  ProcessTimes raw_ctimes;

  std::map<pid_t, int> thread_core_assignment;

  ProcessTimes ctimes;
  ProcessEnergy energy;
};


class Client {
   private:
    int _pid;
    std::string _exec;
    ConnectionPtr _connection;
    bool _has_utility;
    std::vector<double> _utility;
    tetris::perf::HandlePtr _perf;
    std::chrono::high_resolution_clock::time_point _start_tp;
    uint64_t _start_energy;
    std::vector<ProcessEnergyData> _energy_data;

    friend class Manager;

    std::string push_listener_path()
    {
        if (_pid == -1) {
            logger->error("Push Listener Path not fully configured!\n");
            throw std::runtime_error("Can't send messages to not fully registered client!\n");
        }

        std::stringstream ss;
        ss << "/tmp/tetris_push_listener_" << _pid;
        return ss.str();
    }

   public:
    Client(ConnectionPtr connection) :
        _pid{-1}, _exec{}, _connection{connection}, _has_utility{false}, _utility{}, _perf{},
        _start_tp{}, _start_energy{}
    {}

    void send_mapping(const std::vector<int> &cpus)
    {
        ServerMessage msg;
        Connection conn{push_listener_path()};
        logger->info("Sending client '%s' [%d] mapping info\n", _exec.c_str(), _pid);

        msg.set_feature_id(0);
        msg.set_type(ServerMessage::ACTIVATE_CPUS);
        auto cpus_info = msg.mutable_activated_cpus();

        for (const auto &c : cpus) {
            cpus_info->add_cpu_ids(c);
        }

        protobuf_util::Send(conn.locked(), msg);

        ClientResponse response{};
        protobuf_util::Receive(conn.locked(), response);

        if (response.type() != ClientResponse::ACKNOWLEDGE) {
            logger->warning("Client failed to acknowledge message!\n");
        }
    }

    void update_metrics()
    {
        if (!_has_utility)
            return;

        try {
            /* Get utility metrics from the client if the client registered the feature */
            ServerMessage msg;
            Connection conn{push_listener_path()};

            msg.set_feature_id(UTILITY_FEATURE_ID);
            msg.set_type(ServerMessage::UTILITY_UPDATE);

            protobuf_util::Send(conn.locked(), msg);

            ClientResponse response{};
            protobuf_util::Receive(conn.locked(), response);

            if (response.type() == ClientResponse::UTILITY_UPDATE && response.has_utility()) {
                _utility.push_back(response.utility());
            }
        } catch (std::exception &e) {
            logger->warning("Updating utility failed\n");
        }
    }

    void update_energy_data(EnergyData &sw_energy, uint64_t duration_ms) {
      if (_pid == -1) {
          logger->debug("Client not properly initialized for energy update!\n");
          return;
      }

      ProcessEnergyData proc_energy;
      proc_energy.time = sw_energy.time;

      /* In order to partially account the energy to the current client we need to
       * get utime and stime as well as core assignments for the threads of the
       * corresponding process */

      /* 1. Get the total utime and stime of the process */
      {
        std::stringstream path;
        path << "/proc/" << _pid << "/stat";
        std::ifstream stat(path.str());
        if (!stat.is_open()) {
          logger->warning("Can't open /proc/%d/stat for per process cputime statistics\n", _pid);
          return;
        }

        /* The first line in this file contains the interesting information for us */
        std::string stat_line;
        std::getline(stat, stat_line);
        stat.close();

        /* The format of this file is defined in the Linux kernel documentation. Since
         * we are interested in user-time and system-time, we need the elements 13 and
         * 14.
         */
        auto elements = string_util::split(stat_line, ' ');
        proc_energy.raw_ctimes.all =  std::stoull(elements[13]) + std::stoull(elements[14]);
      }
      /* 2. Get the per thread utime and stime as well as core assignments */
      {
        std::stringstream path;
        path << "/proc/" << _pid << "/task/";
        for (const auto & entry : std::filesystem::directory_iterator(path.str())) {
          std::ifstream thread_stat(entry.path() / "stat");
          if (!thread_stat.is_open()) {
            logger->warning("Failed to open stat file in %s\n", entry.path().c_str());
            continue;
          }

          std::string stat_line;
          std::getline(thread_stat, stat_line);
          thread_stat.close();

          /* The format of the stat file is the same as before, hence again we are
           * interested in element 13 and 14. The last core assingment of the thread
           * is saved in element 38 of this list */
          auto elements = string_util::split(stat_line, ' ');
          auto tid = std::stod(elements[0]);
          proc_energy.raw_ctimes.threads[tid] = std::stoull(elements[13]) + std::stoull(elements[14]);
          proc_energy.thread_core_assignment[tid] = std::stod(elements[38]);
        }
      }

      /* 3. Calculate the amount of time the client executed since the last update.
       *    If there is no previous update, assume that all the time was executed 
       *    in this period. */
      if (_energy_data.size() > 0) {
        auto last = _energy_data.back();
        proc_energy.update_interval = proc_energy.time - last.time;

        proc_energy.ctimes.all = util::ctime_to_ms(proc_energy.raw_ctimes.all - last.raw_ctimes.all);
        for (auto &[tid, raw_time] : proc_energy.raw_ctimes.threads) {
          if (last.raw_ctimes.threads.contains(tid))
            proc_energy.ctimes.threads[tid] = util::ctime_to_ms(raw_time - last.raw_ctimes.threads[tid]);
          else
            proc_energy.ctimes.threads[tid] = util::ctime_to_ms(proc_energy.raw_ctimes.threads[tid]);
        }
      } else {
        proc_energy.ctimes.all = util::ctime_to_ms(proc_energy.raw_ctimes.all);
        for (auto &[tid, raw_time] : proc_energy.raw_ctimes.threads)
          proc_energy.ctimes.threads[tid] = util::ctime_to_ms(raw_time);
      }

      /* 4. Now attribute the energy proportional to the time the thread executed on
       * the individual cores */
      uint64_t sum_threads = 0;
      for (auto &[tid, core] : proc_energy.thread_core_assignment) {
        auto thread_energy = sw_energy.ctimes.cores[core] != 0 ? (sw_energy.energy.cores[core] * proc_energy.ctimes.threads[tid]) / sw_energy.ctimes.cores[core] : 0;
        sum_threads += thread_energy;
        proc_energy.energy.threads[tid] = thread_energy;
      }
      proc_energy.energy.all = sum_threads;
      proc_energy.energy.power = sum_threads / duration_ms;

      logger->debug("Client %s [%d] has the following energy data: %llu uJ with %llu ms active --> %llu mW\n",
              _exec.c_str(), _pid, proc_energy.energy.all, proc_energy.ctimes.all, proc_energy.energy.power);
      for (auto &[tid, thread_energy] : proc_energy.energy.threads) {
        logger->debug(" => Thread %d (Core %d): %llu uJ with %llu ms active --> %llu mW\n",
                tid, proc_energy.thread_core_assignment[tid], thread_energy, proc_energy.ctimes.threads[tid],
                proc_energy.ctimes.threads[tid] != 0 ? thread_energy / proc_energy.ctimes.threads[tid] : 0);
      }

      _energy_data.push_back(proc_energy);
    }
};

using ClientPtr = std::shared_ptr<Client>;
using PlatformPtr = std::unique_ptr<Platform>;

class AppAssignments {
   private:
    struct Assignment{
        std::string app;
        std::vector<std::string> cpus;
    };

    std::vector<Assignment> _assignments;
    std::unique_ptr<Assignment> _default;

    std::vector<int> convert_cpus(const std::vector<std::string> &cpulist, Platform *p)
    {
        std::vector<int> result;

        if (cpulist.empty()) {
            for (const auto &c : p->GetCPUThreads()) {
                result.push_back(c.second->GetID());
            }
        } else {
            for (const auto &c : cpulist) {
                auto cptr = p->FindCPUThread(c);
                if (cptr) {
                    result.push_back(cptr->GetID());
                    logger->debug("Enabling CPU %s (%d)\n", c.c_str(), cptr->GetID());
                } else {
                    logger->warning("Unknown CPU %s specified -- ignoring\n", c.c_str());
                }
            }
        }
        return result;
    }

   public:
    AppAssignments() = default;
    AppAssignments(const AppAssignments& o) = delete;
    AppAssignments(AppAssignments &&o) : _assignments{std::move(o._assignments)}, _default{std::move(o._default)}
    {}

    void add_one(const std::string &name, const std::vector<std::string> &cpulist)
    {
        _assignments.emplace_back(name, cpulist);
    }

    void add_all(const std::vector<std::string> &cpulist)
    {
        _default = std::make_unique<Assignment>("", cpulist);
    }

    bool empty() const
    {
        return !_default && _assignments.empty();
    }

    std::vector<int> get_cpus(const std::string &name, Platform *p)
    {
        /* Find the exact app assignment */
        for (auto &a : _assignments) {
            /* assume that the given name is findable somewhere inside the real application name */
            if (name.find(a.app) != std::string::npos) {
                logger->debug("Found assignment for %s\n", name.c_str());
                return convert_cpus(a.cpus, p);
            }
        }

        /* We have no specific app assignment found, check for an all assignment */
        if (_default) {
            logger->debug("Using 'all' assignment for %s\n", name.c_str());
            return convert_cpus(_default->cpus, p);
        }

        /* We did not find any assignment for this APP, log an ERROR and return an ALL CPU assignment */
        logger->error("Could not find assignment for %s!\n", name.c_str());
        return convert_cpus({}, p);
    }
};

class Manager {
   private:
    std::map<int, ClientPtr> _clients;
    PlatformPtr _platform;
    perf::PerfManager _perf_manager;
    std::unique_ptr<Measure> _energy_measure;
    AppAssignments _assignments;
    std::vector<EnergyData> _energy_data;

   public:
    Manager(std::unique_ptr<Platform>&& platform, AppAssignments &&assignments)
        : _clients{}, _assignments{std::move(assignments)}, _platform(std::move(platform)), _perf_manager{}, _energy_measure{}
    {
        _energy_measure = std::move(_platform->GetEnergyMeasureMethod());
    }

    void client_connect(int fd, ConnectionPtr conn)
    {
        auto c = _clients.find(fd);
        if (c != _clients.end()) {
            logger->warning("Reconnection from client %d\n", fd);
        } else {
            _clients[fd] = std::make_shared<Client>(conn);
            logger->debug("Connection from client %d\n", fd);
        }
    }

    bool client_message(int fd)
    {
        auto c = _clients.find(fd);
        if (c == _clients.end()) {
            logger->warning("Message from unknown client %d\n", fd);
            return true;
        }

        logger->debug("Message for client %d\n", fd);
        bool done = false;
        bool close = false;
        ClientPtr cl = c->second;

        while (!done) {
            if (cl->_pid == -1) {
                  /* The client is not yet fully registered with the server. Until now we only accept
                   * registration requests. */
                  RegistrationRequest request{};
                  auto res = protobuf_util::Receive(cl->_connection->locked(), request);

                if (res == Connection::InState::DONE) {
                    /* We are done processing. So return. */
                    done = true;
                } else if (res == Connection::InState::CLOSED) {
                    /* We are done processing and the remote site closed the connection */
                    close = true;
                    done = true;
                } else {
                    cl->_pid = request.pid();
                    cl->_exec = request.exec();
                    if (auto handle = _perf_manager.open(cl->_pid)) {
                        cl->_perf = std::move(handle.value());
                    }
                    cl->_start_tp = std::chrono::high_resolution_clock::now();
                    cl->_start_energy = _energy_measure->read();

                    logger->info(" -> The client registered! '%s' [%d]\n", cl->_exec.c_str(), cl->_pid);

                    /* Construct and send the server's registration response */
                    RegistrationResponse response{};
                    response.set_id(fd);

                    if (protobuf_util::Send(cl->_connection->locked(), response) != Connection::OutState::DONE)
                        logger->error("Failed to acknowledge the registration message\n");

                    /* Get the CPU assignment for this client */
                    auto cpus = _assignments.get_cpus(cl->_exec, _platform.get());

                    /* Directly inform the client about the preferred mapping */
                    cl->send_mapping(cpus);
                }
            } else {
                  /* The client is fully registered, however, we ignore messages */
                ClientMessage msg{};
                auto res = protobuf_util::Receive(cl->_connection->locked(), msg);

                if (res == Connection::InState::DONE) {
                    /* We are done processing. So return. */
                    done = true;
                } else if (res == Connection::InState::CLOSED) {
                    /* We are done processing and the remote site closed the connection */
                    close = true;
                    done = true;
                } else {
                    ServerResponse response{};
                    response.set_type(ServerResponse::ACKNOWLEDGE);

                    switch (msg.type()) {
                    case ClientMessage::OPERATING_POINTS:
                      break;
                    case ClientMessage::OPTIMIZATION_TARGET:
                      break;
                    case ClientMessage::FEATURE_SUBSCRIBE:
                      if (msg.has_feature_info() && msg.feature_info().type() == ClientMessage::FeatureInfo::UTILITY_MEASURE) {
                        cl->_has_utility = true;
  
                        response.set_type(ServerResponse::FEATURE_ACKNOWLEDGE);
                        auto ack_info = response.mutable_feature_ack_info();
                        ack_info->set_id(UTILITY_FEATURE_ID);
                      }
                    }

                    protobuf_util::Send(cl->_connection->locked(), response);
                }
            }
        }

        return close;
    }

    void client_disconnect(int fd)
    {
        auto c = _clients.find(fd);
        if (c == _clients.end()) {
            logger->debug("Unknown client disconnected\n");
        } else {
            logger->info("Client %s [%d] disconnected\n", c->second->_exec.c_str(), c->second->_pid);

            /* Get all the interesting statistics from the client and output it to the log */
            auto cl = c->second;

            auto total_ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - cl->_start_tp);
            auto energy = _energy_measure->read() - cl->_start_energy;

            uint64_t instructions = 0;
            if (cl->_perf) {
                auto perf_data = cl->_perf->read();
                instructions = perf_data["Instructions"];
            }

            if (cl->_has_utility && cl->_utility.size() != 0) {
                double sum = 0;
                for (auto &val : cl->_utility)
                    sum += val;

                auto utility = sum / cl->_utility.size();

                logger->info("time;energy;instruction;ips;utility\n%lf;%llu;%llu;%lf;%lf\n",
                    total_ms.count(), energy, instructions, instructions/total_ms.count()*1000, utility);
            } else {
                logger->info("time;energy;instruction;ips\n%lf;%llu;%llu;%lf\n",
                    total_ms.count(), energy, instructions, instructions/total_ms.count()*1000);
            }

            /* Get the average power from the client and output as well */
            double avg_power = 0.0;
            int n = 0;
            for (const auto &edata : cl->_energy_data) {
                avg_power += (edata.energy.power - avg_power) / ++n;
            }
            logger->info("EnergAt power (mW):\n%s;%lf\n", cl->_exec.c_str(), avg_power);

            /* Now erase the client */
            _clients.erase(c);
        }
    }

    void update_client_metrics()
    {
        /* Call the client to update its metrics (only applies for clients with UTILITY_FEEDBACK) */
        for (auto &[_, c] : _clients) {
            c->update_metrics();
        }

        /* Make energy measurements per client */
        logger->debug("Updating energy data based on timer update\n");
        auto now = std::chrono::high_resolution_clock::now();

        EnergyData energy;
        energy.time = now;
        energy.total_energy_uj= _energy_measure->read();

        /* Ok we got the total energy consumption. Now it is time to attribute it to the individual tasks.
         * In order to achieve this, we follow the approach given by the EnergAt paper. We basically calculate
         * the individual influence of the tasks at the overall system energy by comparing their cputime with
         * the overall system wide cputime */

        /* 1: Read the overall cputime statistics from /proc/stat for all CPUs as well as total*/
        std::ifstream stat("/proc/stat");
        if (!stat.is_open()) {
          logger->warning("Can't open /proc/stat for global cputime statistics");
        }

        /* The first line contains the total CPU time */
        std::string stat_line;
        std::getline(stat, stat_line);

        /* Since the line looks as follows:
         * cpu  <user> <niced> <system> …
         * Hence we are interested in element 2 and 4 (when split at every ' ').
         */
        {
          auto elements = string_util::split(stat_line, ' ');
          energy.raw_ctimes.all = std::stoull(elements[2]) + std::stoull(elements[4]);
        }
  
        /* Now read the remaining lines to get the CPU time per cores */
        while (true) {
          std::getline(stat, stat_line);
          if (string_util::starts_with(stat_line, "cpu")) {
            /* Since the line looks as follows:
             * cpuN <user> <niced> <system> …
             * Hence we are interested in element 1 and 3 (when split at every ' ').
             */
            auto elements = string_util::split(stat_line, ' ');
            energy.raw_ctimes.cores.push_back(std::stoull(elements[1]) + std::stoull(elements[3]));
          } else {
            break;
          }
        }

        if (_energy_data.size() == 0) {
          /* If we don't have any prior data, the remaining measurements are not meaningful. Bail early in this case. */
          _energy_data.push_back(energy);
          return;
        }

        auto &last = _energy_data.back();
        /* 2a: Calculate how much the cores were active over the last period */
        energy.ctimes.all = util::ctime_to_ms(energy.raw_ctimes.all - last.raw_ctimes.all);
        for (int i = 0; i < energy.raw_ctimes.cores.size(); ++i) {
          energy.ctimes.cores.push_back(util::ctime_to_ms(energy.raw_ctimes.cores[i] - last.raw_ctimes.cores[i]));
        }

        /* 2b: Attribute the measured energy to the individual CPUs respecting their power coefficient */
        auto all_energy_uj = energy.total_energy_uj - last.total_energy_uj;
        auto duration_ms =  std::chrono::duration_cast<std::chrono::milliseconds>(energy.time - last.time).count();
        if (duration_ms == 0) {
          logger->warning("No time has passed since last update - Ignoring! (%llu ms)\n", duration_ms);
          return;
        }

        if (last.total_energy_uj > energy.total_energy_uj) {
          logger->warning("Energy counters overflowed: %llu (LAST) vs %llu (CURRENT)\n",
                  last.total_energy_uj, energy.total_energy_uj);
          all_energy_uj = 0;
        }

        auto static_energy_uj = _platform->GetStaticPower() * duration_ms;
        energy.energy.all = all_energy_uj - static_energy_uj;

        if (all_energy_uj < static_energy_uj) {
          logger->warning("Reported energy is lower than estimated static energy consumption: %llu (ALL) vs %llu (STATIC)\n",
                  all_energy_uj, static_energy_uj);
          energy.energy.all = 0;
        }

        double time_coefficient_sum = 0.0;
        for (int i = 0; i < energy.ctimes.cores.size(); ++i) {
            bool smt_core = false;

            if (energy.ctimes.cores[i] != 0) {
              for (auto &t : _platform->FindCPUThread(i)->GetSiblings()) {
                if (energy.ctimes.cores[t->GetID()] != 0)
                  smt_core = true;
              }
            }
            if (smt_core)
              time_coefficient_sum += energy.ctimes.cores[i] * static_cast<double>(_platform->FindCPUThread(i)->GetPowerCoefficient())/2;
            else
              time_coefficient_sum += energy.ctimes.cores[i] * _platform->FindCPUThread(i)->GetPowerCoefficient();
        }
        for (int i = 0; i < energy.ctimes.cores.size(); ++i) {
            energy.energy.cores.push_back((energy.energy.all * energy.ctimes.cores[i] * _platform->FindCPUThread(i)->GetPowerCoefficient()) / time_coefficient_sum);
        }

        logger->debug("Current energy consumption: Total: %llu uJ --> %llu uJ (%llu mW) since last update\n",
                energy.total_energy_uj, energy.energy.all, energy.energy.all / duration_ms);

        /* 2: Now do local attribution at the individual clients */
        for (auto& [cid, c]: _clients) {
            c->update_energy_data(energy, duration_ms);
        }

        _energy_data.push_back(energy);
    }
};


void usage() {
  std::cout << "usage: tetris_mock [-h] [-p platform] -c CPUS | -a APPFILE\n"
            << "\n"
            << "Options:\n"
            << "   -h, --help                show this help message.\n"
            << "   -p, --platform <name>     specify the platform.\n"
            << "MOCK Data:\n"
            << "   -c CPUS                   one general CPU list for all applications (',' separated)\n"
            << "   -a APPFILE                file with CPU assignments per managed application (appname:cpulist)"
            << "\n";
}

/**
 * \brief Setup a socket with a given path and return its file descriptor.
 *
 * If any internal operation fails, the function writes an error message to the
 * standard error output and terminates the program.
 *
 * \param socket_path The path where to open the socket.
 * \param fd Reference to an integer where to store the file descriptor of the
 *           opened socket.
 * \return The created and setup Socket object.
 */
Socket setup_socket(const std::string &socket_path, int &fd) {
  Socket sock;
  try {
    sock.open(socket_path);
    sock.non_blocking();
    sock.listening();
    fd = sock.fd();
  } catch (const std::runtime_error &e) {
    std::cerr << "Failed to open socket " << socket_path << "\n"
              << e.what() << "\n";
    exit(1);
  }
  return sock;
}

/**
 * \brief Setup signal handling for the program.
 *
 * If creating the signal file descriptor fails, the function returns -1.
 *
 * \return The file descriptor of the created signal file descriptor, or -1 on
 * failure.
 */
int setup_signal_handling() {
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGABRT);
  sigaddset(&sigmask, SIGHUP);
  sigaddset(&sigmask, SIGINT);
  sigaddset(&sigmask, SIGQUIT);
  sigaddset(&sigmask, SIGTERM);
  sigaddset(&sigmask, SIGUSR1);
  sigaddset(&sigmask, SIGUSR2);
  sigaddset(&sigmask, SIGALRM);

  /* First block the signals. */
  sigprocmask(SIG_BLOCK, &sigmask, nullptr);

  /* And create a signal fd where these signals are managed. */
  int sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
  return sig_fd;
}

/**
 * \brief Setup the repeating timer for reading out the perf updates for the clients
 **/
bool setup_perf_timer() {
  timer_t timerid;
  struct sigevent sev;
  struct itimerspec its;

  /* Prepare and create the timer signal */
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  sev.sigev_value.sival_ptr = &timerid;

  int ret = timer_create(CLOCK_REALTIME, &sev, &timerid);
  if (ret == -1) {
      std::cerr << "Failed to create timer" << std::endl
                << strerror(errno) << std::endl;
      return false;
  }

  /* Arm the timer */
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = 100000000; /* 100 ms */
  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;

  ret = timer_settime(timerid, 0, &its, NULL);
  if (ret == -1) {
      std::cerr << "Failed to arm timer" << std::endl
                << strerror(errno) << std::endl;

      return false;
  }

  return true;
}

/**
 * \brief Sets up the epoll event loop.
 *
 * \param fds A vector containing file descriptors to be added to the epoll
 * loop.
 *
 * \return On success, it returns the file descriptor for the new epoll
 * instance. On failure, it returns -1 and prints an error message.
 */
int setup_epoll(const std::vector<int> &fds) {
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    std::cerr << "Failed to initialize epoll." << std::endl
              << strerror(errno) << std::endl;
    return -1;
  }

  epoll_event e;
  for (auto &fd : fds) {
    e.data.fd = fd;
    e.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
      std::cerr << "Failed to add socket " << fd << " to epoll." << std::endl
                << strerror(errno) << std::endl;
      return -1;
    }
  }
  return epoll_fd;
}

void event_loop(Manager &manager, int epoll_fd, int server_fd, int control_fd, int sig_fd) {
  // Code for managing event loop
  epoll_event events[MAXEVENTS];
  bool done = false;

  while (!done) {
    int n;

    n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

    for (int i = 0; i < n; ++i) {
      epoll_event *cur = &events[i];

      if (cur->data.fd == server_fd) {
        /* There are a new connections at the server socket.
         * Connect with all of them. */
        while (1) {
          sockaddr_un in_sock;
          socklen_t in_sock_size = sizeof(in_sock);
          int infd =
              ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock),
                       &in_sock_size);
          if (infd == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
              /* We connected to all possible connections already.
               * Continue with the main loop. */
              break;
            } else {
              logger->error(
                  "An error happened while accepting a connection: %s",
                  strerror(errno));
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
            logger->error("Failed to add new connection to epoll: %s",
                          strerror(errno));
            ::close(infd);
          } else {
            logger->info("A new client connected (%i)\n", infd);

            manager.client_connect(infd, in_conn);
          }
        }
      } else if (cur->data.fd == control_fd) {
        /* There are a new connections at the control socket.
         * Connect with all of them. */
        while (1) {
          sockaddr_un in_sock;
          socklen_t in_sock_size = sizeof(in_sock);
          int infd =
              ::accept(cur->data.fd, reinterpret_cast<sockaddr *>(&in_sock),
                       &in_sock_size);
          if (infd == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
              /* We connected to all possible connections already.
               * Continue with the main loop. */
              break;
            } else {
              logger->error(
                  "An error happened while accepting a connection: %s",
                  strerror(errno));
              break;
            }
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
              logger->error(
                  "An error happened while reading data from signal fd: %s",
                  strerror(errno));

            break;
          }

          logger->info("Received a signal (%i)\n", siginfo.ssi_signo);

          switch (siginfo.ssi_signo) {
          case SIGALRM:
            manager.update_client_metrics();
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
}

int main(int argc, char *argv[]) {
  /* Parsing command line arguments. */
  std::string platform_path;
  AppAssignments assignments;

  for (int i = 1; i < argc; ++i) {
    std::string arg{argv[i]};

    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "--platform" || arg == "-p") {
      if (i + 1 >= argc) {
        std::cerr << "Expected platform name or path after " << arg << ".\n";
        usage();
        return 1;
      }

      i++;
      std::string platform = argv[i];

      if (string_util::ends_with(platform, ".yaml")) {
        if (!std::filesystem::exists(platform)) {
          std::cerr << "Specified .yaml platform file does not exist.\n";
          return 1;
        } else {
          platform_path = platform;
        }
      } else {
        // Check if the platform file exists in the default location
        std::string default_path =
            "../examples/platforms/platform_" + platform + ".yaml";
        if (std::filesystem::exists(default_path)) {
          platform_path = default_path;
        } else {
          std::cerr << "Specified platform is not available and it's not a "
                       "valid .yaml file path.\n";
          return 1;
        }
      }
    } else if (arg == "-c") {
        if (i + 1 >= argc) {
          std::cerr << "Expected cpulist after " << arg << ".\n";
          usage();
          return 1;
        }
  
        i++;
        auto cpulist = string_util::split(argv[i], ',');
        assignments.add_all(cpulist);
    } else if (arg == "-a") {
        if (i + 1 >= argc) {
            std::cerr << "Expected appfile after " << arg << ".\n";
            usage();
            return 1;
        }

        i++;
        std::ifstream appfile(argv[i]);
        if (!appfile.is_open()) {
            std::cerr << "Specified appfile did not exist: " << argv[i] << "\n";
            usage();
            return 1;
        }

        std::string line;
        while (std::getline(appfile, line)) {
            auto parts = string_util::split(line, ":");
            if (parts.size() != 2) {
                std::cerr << "Appfile is malformed. Can't parse following line: " << line << "\n";
                return 1;
            }

            auto name = parts[0];
            auto cpulist = string_util::split(parts[1], ",");
            if (name == "*")
                assignments.add_all(cpulist);
            else
                assignments.add_one(name, cpulist);
        }
    } else {
        std::cout << "Unknown command line option\n";
        usage();
        return 1;
    }
  }

  if (platform_path.empty()) {
    std::cerr << "Platform not specified.\n";
    usage();
    return 1;
  }

  if (assignments.empty()) {
    std::cerr << "No app assignments specified!" << std::endl;
    usage();
    return 1;
  }

  std::cout << "Welcome the TETRiS MOCK Server" << std::endl;

  /* Setup logging */
  logger = debug::Logger::get();

  /* Create a platform */
  auto reader = YamlPlatformReader();
  auto platform = reader.ReadFromFile(platform_path);

  /* Setting up the manager */
  Manager manager{std::move(platform), std::move(assignments)};

  // Setting up the server and control sockets
  int server_fd = -1;
  Socket server_sock = setup_socket(SERVER_SOCKET, server_fd);

  int control_fd = -1;
  Socket control_sock = setup_socket(CONTROL_SOCKET, control_fd);

  logger->info(" * Server socket: %s (%i)\n", server_sock.path(), server_fd);
  logger->info(" * Control socket: %s (%i)\n", control_sock.path(), control_fd);

  /* Setup signal handling */
  int sig_fd = setup_signal_handling();
  if (sig_fd == -1) {
    std::cerr << "Failed to create signal fd." << std::endl
              << strerror(errno) << std::endl;
    return 1;
  }

  /* Setup the epoll event loop. */
  int epoll_fd = setup_epoll({server_fd, control_fd, sig_fd});
  if (epoll_fd == -1) {
    return 1;
  }

  if (!setup_perf_timer()) {
      return 1;
  }

  /* The event loop */
  event_loop(manager, epoll_fd, server_fd, control_fd, sig_fd);

  std::cout << "Exiting" << std::endl;
  ::close(sig_fd);

  return 0;
}
