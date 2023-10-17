#include "client.h"
#include "manager.h"

#include "proto/tetris.pb.h"
#include "util/operating_point.h"

using namespace tetris;

Client::Client(const ConnectionPtr &conn)
    : connection{conn}, exec{}, pid{-1}, ops{},
      active_op{}, type{Type::PASSIV}, filter{},
      comp{} {
  std::stringstream path{};
  path << "/tmp/tetris_push_listener_" << pid;

  push_listener_path = path.str();
}

bool Client::receive_ops(
    const ClientMessage::OperatingPointsInfo &ops_info) {
  ops.clear();

  /* Convert the protobuf mapping representation into our internal format */
  for (int i = 0; i < ops_info.operating_points_size(); i++) {
    auto cur = ops_info.operating_points(i);
    ops.emplace_back(cur);
  }

  return true;
}

void Client::activate_op(const OperatingPointAllocation &new_op) {
  LOGGER->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid,
               new_op.base.name.c_str());
  active_op = new_op;

  /* Send the new mapping information to the client so that client library knows
   * about the change and can react accordingly. */
  ServerMessage msg{};
  msg.set_type(ServerMessage::ACTIVATE_OP);
  auto op_info = msg.mutable_activated_op_info();
  op_info->set_identifier(new_op.base.name);

  LOGGER->info(" -> sending mapping info to client\n");

  Connection conn{push_listener_path};
  auto response = protobuf_util::Send(conn.locked(), msg);

  LOGGER->info(" * done\n");
}

ServerResponse Client::handle_message(const ClientMessage &msg)
{
  ServerResponse response{};
  response.set_type(ServerResponse::ERROR);

  switch(msg.type()) {
    case ClientMessage::OPERATING_POINTS:
      /* Parse the mapping information from the client */
      if (msg.has_ops_info() && receive_ops(msg.ops_info())) {
        response.set_type(ServerResponse::ACKNOWLEDGE);
      }
      break;
    case ClientMessage::OPTIMIZATION_TARGET:
      break;
    case ClientMessage::FEATURE_SUBSCRIBE:
      break;
  }

  return response;
}

