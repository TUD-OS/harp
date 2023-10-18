#include "manager.h"

#include "util/operating_point.h"
#include "util/platform/platform.h"
#include <memory>
#include <chrono>

using namespace tetris;

using ClientPtr = std::unique_ptr<Client>;

/**
 * \brief Handles the incoming message from a client.
 *
 * This function processes incoming messages from a client. Based on the type of
 * the message, the following actions are performed:
 *   - TETRIS_NEW_CLIENT: Registering a new client.
 *   - TETRIS_NEW_THREAD: Registering a new thread for an existing client.
 *   - DPM_SUBSCRIBE: Subscribing a client to Dynamic Process Mananger (DPM).
 *   - DPM_SEND_APPLICATION_THREAD_ID: Registering application thread IDs from a
 * client. In all cases, a message is sent back to the client to acknowledge the
 * received request.
 *
 * If any error occurs during the processing of the message, the function will
 * log an error message and close the connection to the client.
 *
 * \param fd File descriptor of the client's connection.
 * \return A boolean indicating if the connection to the client should be
 * closed. Returns true if an error occurs or if the client is not being
 * managed.
 */
bool Manager::client_message(int fd) try {
  ClientPtr &c = _clients.at(fd);
  ConnectionPtr conn = c->connection;

  bool done = false;
  bool close = false;

  while (!done) {
    if (c->pid == -1) {
      /* The client is not yet fully registered with the server. Until now we only accept
       * registration requests. */
      RegistrationRequest request{};
      auto res = protobuf_util::Receive(conn->locked(), request);

      if (res == Connection::InState::DONE) {
        /* We are done processing. So return. */
        done = true;
      } else if (res == Connection::InState::CLOSED) {
        /* We are done processing and the remote site closed the connection */
        close = true;
        done = true;
      } else {
        c->pid = request.pid();
        c->exec = request.exec();

        LOGGER->info(" -> The client registered! '%s' [%d]\n", c->exec.c_str(), c->pid);

        /* Construct and send the server's registration response */
        RegistrationResponse response{};
        response.set_id(fd);

        if (protobuf_util::Send(conn->locked(), response) != Connection::OutState::DONE)
          LOGGER->error("Failed to acknowledge the new-thread message\n");
      }
    } else {
      /* The client is fully registered, receive the message and let
       * the client handle it properly */
      ClientMessage msg{};
      auto res = protobuf_util::Receive(conn->locked(), msg);

      if (res == Connection::InState::DONE) {
        /* We are done processing. So return. */
        done = true;
      } else if (res == Connection::InState::CLOSED) {
        /* We are done processing and the remote site closed the connection */
        close = true;
        done = true;
      } else {
        LOGGER->debug(" -> Forward message to client [%d]\n", c->pid);
        auto response = c->handle_message(msg);
        if (protobuf_util::Send(conn->locked(), response) != Connection::OutState::DONE)
          LOGGER->error("Failed to acknowledge the new-thread message\n");
      }
    }
  }
  return close;
} catch (std::out_of_range) {
  LOGGER->warning("Received message for unknown client %i\n", fd);
  return true;
} catch (std::runtime_error &e) {
  LOGGER->warning("Error working with message for client %i: %s", fd, e.what());
  return true;
}

void Manager::print_mappings() {
  auto &allocator = _platform->GetEquivResAllocator();
  std::cout << "Currently active mappings:" << std::endl
            << "==========================" << std::endl;
  for (const auto &[name, client] : _clients) {
    std::cout << "Client '" << client->exec << "' [" << client->pid
              << "] (ID: " << name << ")" << std::endl;
    if (client->active_op.has_value()) {
      std::cout << "-> mapping: " << client->active_op->base.name << " ["
                << allocator.GetEquivClassName(*(client->active_op)) << "]"
                << std::endl;
    } else {
      std::cout << "-> mapping: none" << std::endl;
    }
  }
  std::cout << "======= END OF LIST =======" << std::endl;
}

void Manager::run_scheduler() {
  _needs_reschedule = false;
  auto start = std::chrono::high_resolution_clock::now();

  // Update the current progress of each client
  update_client_progresses(start);

  // Run the scheduler
  std::vector<Client*> clients;
  for (auto& [cid, c]: _clients) {
    if (c->type == Client::ACTIVE)
      clients.push_back(c.get());
  }

  // Measure separately the call to GenerateSchedule()
  auto before  = std::chrono::high_resolution_clock::now();

  // 0.0 is a dummy start time
  auto schedule = _scheduler->GenerateSchedule(clients, 0.0, _blocked_cores);

  // The scheduling might be too too long, update progresses again
  auto after = std::chrono::high_resolution_clock::now();
  update_client_progresses(after);

  // updates the mappings 
  for (auto& c: clients) {
    auto op = schedule->GetOperatingPoint(0, c);
    if (op.has_value())
      c->activate_op(*op);
    else {
      LOGGER->error("No mapping generated for client '%s' [%d]."
          "Handling of such cases is not yet implemented.",
          c->exec.c_str(), c->pid);
      throw std::runtime_error("Not yet implemented");
    }
  }
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> full_dur = end - start;
  auto full_dur_s = full_dur.count();
  std::chrono::duration<double> sched_dur = after - before;
  auto sched_dur_s = sched_dur.count();
  LOGGER->info("Activated the cheduler: duration = %lfs [scheduling time: %lfs ]\n",
      full_dur_s, sched_dur_s);
}
