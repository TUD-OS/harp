//
// Created by dylan on 10/08/2020.
//

#include "concrete_client.h"
#include "client.h"
#include "util/protobuf_util.h"
#include "util/platform/reader.h"
#include "util/mapping_reader.h"

#include <memory>
#include <sstream>

namespace tetris {

ConcreteClient::ConcreteClient(const std::string &server_socket_path,
        const std::string &platform_desc_path, const std::string &mapping_path)
        : Client(), _push_message_listener(get_push_listener_socket_path(), this),
          _managed(false), _communication_mutex()
{
    _logger = debug::Logger::get();
    try {
        /* Read the platform file */
        YamlPlatformReader platform_reader;
        _platform = std::move(platform_reader.ReadFromFile(platform_desc_path));

        /* Read the mappings */
        YamlMappingReader mapping_reader;
        _mappings = std::move(mapping_reader.read_mappings(*_platform, mapping_path));
        _logger->debug(" -> Loaded %d mappings for this client\n", _mappings.size());

        _tetris_server_connection.connect(server_socket_path);
        _managed = register_client();

        if (_managed) {
            /* Send over the mappings to the server, so that we can get scheduled */
            ClientMessage msg;
            msg.set_type(ClientMessage::OPERATING_POINTS);
            auto ops_info = msg.mutable_ops_info();

            for (const auto& m : _mappings) {
                auto op_data = ops_info->add_operating_points();

                op_data->set_identifier(m.name);
                for (const auto& [cn, cv] : m.characteristics_map) {
                    auto c = op_data->add_characteristics();
                    c->set_name(cn);
                    c->set_value(cv);
                }

                for (const auto& c : m.cpus) {
                    op_data->add_cpu_ids(c);
                }
            }

            ServerResponse response{};
            _communication_mutex.lock();
            protobuf_util::Send(_tetris_server_connection.locked(), msg);
            protobuf_util::Receive(_tetris_server_connection.locked(), response);
            _communication_mutex.unlock();

            if (response.type() != ServerResponse::ACKNOWLEDGE) {
                _logger->warning("The server failed to parse our mappings!\n");
            }
        }
    } catch (std::exception &e) {
        _logger->info("No TETRiS server, TETRiS is unused.\n");
        _managed = false;
    }
}

void ConcreteClient::bind(Feature *feature)
{
    // If the client is not connected to the server, do not bind the feature.
    if (!_managed)
        return;
    // Bind the client to the feature.
    feature->accept(this);
    // If it needs a handshake, perform it.
    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();
        // Add the feature to the subscriber lists of the push listener.
        _push_message_listener.add_subscriber(feature_id, feature);
    }
}

void ConcreteClient::bind(MappingFeature *feature)
{
    if (!_managed)
        return;

    feature->accept(this);

    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();

        _push_message_listener.add_subscriber(feature_id, feature);
    }

    _mapping_features.push_back(feature);

    if (_active_mapping)
        feature->mapping_update(*_active_mapping, {});
}

ServerResponse ConcreteClient::send(const ClientMessage &message)
{
    ServerResponse response{};
    _communication_mutex.lock();
    protobuf_util::Send(_tetris_server_connection.locked(), message);
    protobuf_util::Receive(_tetris_server_connection.locked(), response);
    _communication_mutex.unlock();
    return response;
}

std::string ConcreteClient::get_push_listener_socket_path()
{
    std::stringstream string_stream{};
    string_stream << "/tmp/tetris_push_listener_" << getpid();
    return string_stream.str();
}

ClientResponse ConcreteClient::handle(const ServerMessage &msg)
{
    ClientResponse response{};
    response.set_type(tetris::ClientResponse::ERROR);

    if (msg.has_activated_op_info()) {
        /* Call all mapping_features with the new mapping, so that they can adapt */
        auto active_op = msg.activated_op_info();

        auto map_id = active_op.identifier();
        _logger->info(" * Got mapping update from server: %s\n", map_id.c_str());

        std::map<int, int> conv_map;
        for (int i = 0; i < active_op.cpu_convs_size(); ++i) {
            auto conv = active_op.cpu_convs(i);
            conv_map.emplace(conv.cpu_id_from(), conv.cpu_id_to());
        }

        auto it = std::find_if(_mappings.begin(), _mappings.end(), [&map_id](const Mapping& m) { return m.name == map_id; });
        if (it != _mappings.end()) {
            _active_mapping = std::make_unique<Mapping>(*it, conv_map);
            _logger->debug(" -> Active mapping %s\n", _active_mapping->name.c_str());

            /* Tell the features to react to the new mapping */
            for (const auto& feature : _mapping_features) {
                feature->mapping_update(*_active_mapping, conv_map);
            }

            response.set_type(ClientResponse::ACKNOWLEDGE);
        }
    }

    return response;
}

bool ConcreteClient::register_client()
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
        _communication_mutex.lock();
        protobuf_util::Send(_tetris_server_connection.locked(), request);
        protobuf_util::Receive(_tetris_server_connection.locked(), response);
        _communication_mutex.unlock();

        // Process the TETRiS server response.
        _logger->info("TETRIS-ID: %d\n", response.id());
        return true;
    } catch (std::exception &e) {
        return false;
    }
}

}
