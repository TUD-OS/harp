#include "client.h"
#include "manager.h"

#include "proto/tetris.pb.h"

#include <sstream>


using namespace tetris;

Client::Client(const ConnectionPtr &conn, Manager& manager)
    : connection{conn}, exec{}, pid{-1}, op_table{},
      active_op{}, progress{0.0}, type{Type::PASSIV}, _manager{manager}
{
  progress_tp = std::chrono::high_resolution_clock::now();

  // TODO: choose the type of operating point table based on the client info
  // TODO: Pass the objective function
  op_table = std::make_unique<CustomOperatingPointTable>(_manager.GetPlatform());
}

std::string Client::push_path() const
{
    std::stringstream path{};
    path << "/tmp/tetris_push_listener_" << pid;

    return path.str();
}

bool Client::receive_ops(
    const ClientMessage::OperatingPointsInfo &ops_info) {
  /* Mark the client active since we now have operating points */
  type = Type::ACTIVE;

  op_table->Clear();

  /* Convert the protobuf mapping representation into our internal format */
  int op_size = ops_info.operating_points_size();
  for (int i = 0; i < ops_info.operating_points_size(); i++) {
    auto cur = ops_info.operating_points(i);
    throw std::runtime_error("NYI");
    //ops.emplace_back(_manager->GetPlatform(), cur);
  }

  LOGGER->info(" -> Received %d operating points from client %d\n", op_size, pid);
  _manager.reschedule();

  return true;
}

void Client::activate_op(const OperatingPointAllocation &new_op) {
  LOGGER->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid,
               new_op.name().c_str());
  active_op = new_op;

  /* Send the new mapping information to the client so that client library knows
   * about the change and can react accordingly. */
  ServerMessage msg{};
  msg.set_feature_id(0);
  msg.set_type(ServerMessage::ACTIVATE_OP);
  auto op_info = msg.mutable_activated_op_info();
  op_info->set_identifier(new_op.name());

  for (const auto& [fc, tc] : new_op.permutation) {
    auto conv = op_info->add_cpu_convs();
    conv->set_cpu_id_from(fc);
    conv->set_cpu_id_to(tc);
  }

  LOGGER->info(" -> sending mapping info to client\n");

  try {
    Connection conn{push_path()};
    ClientResponse response;

    protobuf_util::Send(conn.locked(), msg);
    protobuf_util::Receive(conn.locked(), response);

    if (response.type() != ClientResponse::ACKNOWLEDGE) {
        LOGGER->warning(" -! Client didn't acknowledge the message!\n");
    }
  } catch (std::exception &e) {
    LOGGER->error(" -! Sending failed with an error: %s\n", e.what());
  }

  LOGGER->info(" * done\n");
}

ServerResponse Client::handle_message(const ClientMessage &msg)
{
  ServerResponse response{};
  response.set_type(ServerResponse::ERROR);

  switch(msg.type()) {
    case ClientMessage::OPERATING_POINTS:
      LOGGER->debug(" -> Received operating point message from client\n");
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

