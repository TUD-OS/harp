#include "mock_client.h"
#include "util/platform/perf.h"
#include "util/protobuf_util.h"
#include "util/platform/reader.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>

#include <unistd.h>


namespace tetris {

MockClient::MockClient(const std::string &server_socket_path, const std::string &platform_desc_path)
        : Client(), _mock_message_listener(get_push_listener_socket_path(), this),
          _managed(false)
{
    _logger = debug::Logger::get();
    try {
        YamlPlatformReader platform_reader;
        _platform = std::move(platform_reader.ReadFromFile(platform_desc_path));
        _energy_measure = std::move(_platform->GetEnergyMeasureMethod());
        _perf_measure = std::make_unique<perf::PerfManager>();
        if (auto tmp = _perf_measure->open(getpid())) {
            _perf_handle = std::move(tmp.value());
        } else {
            _logger->warning("No perf measurements available!\n");
        }

        _tetris_server_connection.connect(server_socket_path);
        _managed = register_client();

        /* Take energy and performance measurements */
        take_measurement();
    } catch (std::exception &e) {
        _logger->info("No TETRiS server, TETRiS is unused.\n");
        _managed = false;
    }
}

MockClient::~MockClient() {
    take_measurement();

    if (_energy_perf_measurments.size() > 1) {
        auto [f_time, f_energy, f_perf] = _energy_perf_measurments.front();
        auto [b_time, b_energy, b_perf] = _energy_perf_measurments.back();

        _logger->info("time;energy;ips\n%lu;%llu;%lf\n",
                std::chrono::duration<double, std::milli>(b_time - f_time).count(),
                b_energy - f_energy, 
                (b_perf["Instructions"] - f_perf["Instructions"]) / std::chrono::duration<double>(b_time - f_time).count());
    }
}

void MockClient::bind(Feature *feature)
{
    /* Features are not supported by the MockClient */
}

void MockClient::bind(MappingFeature *feature)
{
    if (!_managed)
        return;

    feature->accept(this);

    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();

        _mock_message_listener.add_subscriber(feature_id, feature);
    }

    _mapping_features.push_back(feature);

    /* If we already have an active mapping, let the feature know about this! */
if (_active_mapping)
        feature->mapping_update(*_active_mapping, {});
}

ServerResponse MockClient::send(const ClientMessage &message)
{
    ServerResponse response{};
    protobuf_util::Send(_tetris_server_connection.locked(), message);
    protobuf_util::Receive(_tetris_server_connection.locked(), response);
    return response;
}

std::string MockClient::get_push_listener_socket_path()
{
    std::stringstream string_stream{};
    string_stream << "/tmp/tetris_push_listener_" << getpid();

    return string_stream.str();
}

ClientResponse MockClient::handle(const ServerMessage &msg)
{
    /* We normally don't handle this messages so just ACK them */
    ClientResponse response{};
    response.set_type(tetris::ClientResponse::ACKNOWLEDGE);

    return response;
}

ClientResponse MockClient::handle(const MockServerMessage &msg)
{
    _logger->debug("Got MockMapping from TETRiS server\n");

    if (msg.type() == MockServerMessage::ACTIVATE_OP && msg.has_activated_op_info()) {
        /* Construct the mapping */
        std::vector<std::pair<std::string, std::string>> thread_map;
        auto op_info = msg.activated_op_info();
        for (int i = 0; i < op_info.cpus_size(); ++i) {
            auto cpu = op_info.cpus(i);
            thread_map.push_back({"T_"+cpu, cpu});
        }
        RegionAffinities<std::string> regions{};
        std::vector<std::pair<std::string, std::string>> characteristics{};

        _active_mapping = std::make_unique<Mapping>(*_platform, "test-mapping", thread_map, regions, characteristics);

        for (const auto f : _mapping_features)
            f->mapping_update(*_active_mapping, {});
    }

    ClientResponse response{};
    response.set_type(tetris::ClientResponse::ACKNOWLEDGE);

    return response;
}

bool MockClient::register_client()
{
    RegistrationRequest request{};

    /* Send the new-client message to the server. */
    request.set_pid(getpid());
    char exec[512];
    memset(exec, 0, sizeof(exec));
    readlink("/proc/self/exe", exec, sizeof(exec));
    request.set_exec(exec);

    // Send the command.
    try {
        RegistrationResponse response{};
        protobuf_util::Send(_tetris_server_connection.locked(), request);
        protobuf_util::Receive(_tetris_server_connection.locked(), response);

        // Process the TETRiS server response.
        _logger->info("TETRIS-ID: %d\n", response.id());
        return true;
    } catch (std::exception &e) {
        return false;
    }
}

void MockClient::take_measurement()
{
    if (_perf_handle) {
        auto tp = std::chrono::high_resolution_clock::now();
        auto energy = _energy_measure->read();
        auto perf = _perf_handle->read();

        _energy_perf_measurments.emplace_back(tp, energy, perf);
    }
}

} /* namespace tetris */
