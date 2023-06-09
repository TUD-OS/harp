#include "manager.h"

#include "algorithm.h"
#include "mapping_reader.h"

/**
 * \brief Selects the best mapping for a given client.
 *
 * This function first identifies all mappings that satisfy the client's filter
 * criteria and are compatible with the currently non-occupied CPUs. Out of the
 * remaining possible mappings, it selects the best one based on the client's
 * comparison criteria. If no mapping is found that satisfies the filter, it
 * throws a NoMappingError.
 *
 * \param c The client for whom the best mapping is sought.
 * \return The best mapping for the client.
 * \throw NoMappingError If no suitable mapping is found.
 */

Mapping Manager::select_best_mapping(Client &c) {
  LOGGER->info("Search for best mapping for '%s' [%d] using criteria %s\n",
               c.exec.c_str(), c.pid, c.comp.repr().c_str());

  /* First go through all mappings and take those that satisfy our filter
   * criteria */
  auto filter = [&c](const Mapping &m) -> bool { return c.filter(m); };

  std::vector<Mapping> possible_mappings;
  for (const auto &m : c.mappings) {
    if (filter(m))
      possible_mappings.push_back(m);
    else
      LOGGER->debug(
          " * Mapping %s (%.0f@%s) [%s] doesn't satisfy filter criteria %s: "
          "%s=%f\n",
          m.name.c_str(), m.characteristic(c.comp.criteria()),
          c.comp.criteria().c_str(), m.equivalence_class().name().c_str(),
          c.filter.repr().c_str(), c.filter.criteria().c_str(),
          m.characteristic(c.filter.criteria()));
  }

  if (possible_mappings.empty()) {
    LOGGER->debug(
        "No mappings are available for client '%s' [%i] that satisfy the "
        "filter\n",
        c.exec.c_str(), c.pid);
    throw NoMappingError("Can't find mapping that satisfies the filter.");
  } else
    LOGGER->debug(
        " * There are %i mapping(s) for this client that satisfy the filter\n",
        possible_mappings.size());

  /* Now get all the mappings (containing equivalent ones) from the possible
   * ones, that still fit on the non-occupied CPUs. */
  CPUList occupied_cpus = _blocked_cpus;
  for (const auto &[name, cl] : _clients) {
    if (cl.pid == c.pid) continue;

    occupied_cpus |= cl.cpus();
  }

  if (occupied_cpus.nr_cpus() == 0)
    LOGGER->debug(" * Already taken cpu(s): none\n");
  else
    LOGGER->debug(
        " * Already taken cpu(s): %s\n",
        string_util::join(occupied_cpus.cpulist(num_cpus), ",").c_str());

  /* Get all the TETRiS mappings for this client */
  auto possible_tetris_mappings =
      tetris_mappings(possible_mappings, occupied_cpus);
  if (possible_tetris_mappings.empty()) {
    LOGGER->debug(
        "No TETRiS mappings are available for client '%s' [%i] that fit the "
        "available cpu(s)\n",
        c.exec.c_str(), c.pid);
    throw NoMappingError("Can't find a proper TETRiS mapping for the client.");
  } else
    LOGGER->debug(
        " * There are %i TETRiS mapping(s) for this client that fit the "
        "available cpu(s)\n",
        possible_tetris_mappings.size());

  /* Now select the best one out of the remaining ones. */
  auto comp = [&c](const Mapping &other, const Mapping &best) -> bool {
    return c.comp(other, best);
  };

  auto best = possible_tetris_mappings.begin();
  LOGGER->debug(" * Start search with mapping: %s (%.0f@%s) [%s]\n",
                best->name.c_str(), best->characteristic(c.comp.criteria()),
                c.comp.repr().c_str(),
                best->equivalence_class().name().c_str());

  for (auto m = best; m != possible_tetris_mappings.end(); ++m) {
    if (filter(*m) && comp(*m, *best)) {
      LOGGER->debug(
          " * Found better mapping: %s (%.0f@%s) [%s] vs %s (%.0f@%s) [%s]\n",
          m->name.c_str(), m->characteristic(c.comp.criteria()),
          c.comp.repr().c_str(), m->equivalence_class().name().c_str(),
          best->name.c_str(), best->characteristic(c.comp.criteria()),
          c.comp.repr().c_str(), best->equivalence_class().name().c_str());

      /* Remember this one as best one */
      best = m;
    }
  }

  LOGGER->info("The best mapping: %s (%.0f@%s) [%s]\n", best->name.c_str(),
               best->characteristic(c.comp.criteria()), c.comp.repr().c_str(),
               best->equivalence_class().name().c_str());

  return *best;
}

/**
 * \brief Uses the client's preferred mapping if available, otherwise selects
 * the best one.
 */
Mapping Manager::use_preferred_mapping(
    Client &c, const std::string &preferred_mapping_name) {
  LOGGER->info("Use preferred mapping '%s' for '%s' [%d]\n",
               preferred_mapping_name.c_str(), c.exec.c_str(), c.pid);

  auto it = std::find_if(
      c.mappings.begin(), c.mappings.end(),
      [&](const auto &m) { return m.name == preferred_mapping_name; });
  if (it != c.mappings.end())
    return *it;
  else {
    LOGGER->info("Couldn't find preferred mapping\n");
    return select_best_mapping(c);
  }
}

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
  Client &c = _clients.at(fd);
  ConnectionPtr conn = c.connection;

  bool done = false;
  bool close = false;

  while (!done) {
    if (c.pid == -1) {
      /* The client is not yet fully registered with the server. Until now we only accept
       * registration requests. */
      tetris::RegistrationRequest request{};
      auto res = protobuf_util::Receive(conn->locked(), request);

      if (res == Connection::InState::DONE) {
        /* We are done processing. So return. */
        done = true;
      } else if (res == Connection::InState::CLOSED) {
        /* We are done processing and the remote site closed the connection */
        close = true;
        done = true;
      } else {
        c.pid = request.pid();
        c.exec = request.exec();

        LOGGER->info(" -> The client registered! '%s' [%d]\n", c.exec.c_str(), c.pid);

        /* Construct and send the server's registration response */
        tetris::RegistrationResponse response{};
        response.set_id(fd);

        if (protobuf_util::Send(conn->locked(), response) != Connection::OutState::DONE)
          LOGGER->error("Failed to acknowledge the new-thread message\n");
      }
    } else {
      /* The client is fully registered, receive the message and let
       * the client handle it properly */
      tetris::ClientMessage msg{};
      auto res = protobuf_util::Receive(conn->locked(), msg);

      if (res == Connection::InState::DONE) {
        /* We are done processing. So return. */
        done = true;
      } else if (res == Connection::InState::CLOSED) {
        /* We are done processing and the remote site closed the connection */
        close = true;
        done = true;
      } else {
        auto response = c.handle_message(msg);
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

void Manager::update_mappings() {
    /* TODO: Initiate update of the mappings */
}
