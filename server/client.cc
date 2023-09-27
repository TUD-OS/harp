#include "client.h"
#include "manager.h"

#include "proto/tetris.pb.h"

Client::Client(const Manager &manager, const ConnectionPtr &conn)
    : manager{manager}, connection{conn}, exec{}, pid{-1}, mappings{},
      active_mapping{manager.GetPlatform()}, type{Type::PASSIV}, filter{},
      comp{} {
  std::stringstream path{};
  path << "/tmp/tetris_push_listener_" << pid;

  push_listener_path = path.str();
}

bool Client::receive_mappings(
    const tetris::ClientMessage::MappingsInfo &mapping_info) {
  mappings.clear();

  const auto &platform = manager.GetPlatform();

  /* Convert the protobuf mapping representation into our internal format */
  for (int i = 0; i < mapping_info.mappings_size(); i++) {
    auto cur = mapping_info.mappings(i);
    Mapping new_mapping{platform, cur};
    mappings.push_back(new_mapping);
  }

  return true;
}

void Client::update_mapping(const Mapping &new_mapping) {
  if (new_mapping.name == active_mapping.name) return;

  LOGGER->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid,
               new_mapping.name.c_str());
  active_mapping = new_mapping;

  /* Send the new mapping information to the client so that client library knows
   * about the change and can react accordingly. */
  tetris::ServerMessage msg{};
  msg.set_type(tetris::ServerMessage::UPDATE_MAPPING);
  auto map_info = msg.mutable_update_mapping();

  for (auto &[name, cpu_nr] : active_mapping.thread_map) {
    auto thread = map_info->add_threads();
    thread->set_name(name);
    thread->set_cpu_nr(cpu_nr);
  }

  LOGGER->info(" -> sending mapping info to client\n");

  Connection conn{push_listener_path};
  auto response = protobuf_util::Send(conn.locked(), msg);

  LOGGER->info(" * done\n");
}

tetris::ServerResponse Client::handle_message(const tetris::ClientMessage &msg)
{
  tetris::ServerResponse response{};
  response.set_type(tetris::ServerResponse::ERROR);

  switch(msg.type()) {
    case tetris::ClientMessage::MAPPINGS:
      /* Parse the mapping information from the client */
      if (msg.has_mappings_info() && receive_mappings(msg.mappings_info())) {
        response.set_type(tetris::ServerResponse::ACKNOWLEDGE);
      }
      break;
    case tetris::ClientMessage::OPTIMIZATION_TARGET:
      break;
    case tetris::ClientMessage::FEATURE_SUBSCRIBE:
      break;
  }

  return response;
}

