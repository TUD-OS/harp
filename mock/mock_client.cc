#include "mock_client.h"
#include "util/protobuf_util.h"
#include "util/platform/reader.h"

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

        _tetris_server_connection.connect(server_socket_path);
        _managed = register_client();
    } catch (std::exception &e) {
        _logger->info("No TETRiS server, TETRiS is unused.\n");
        _managed = false;
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

} /* namespace tetris */
