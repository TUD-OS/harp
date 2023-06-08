#include "client.h"

void Client::update_mapping(const Mapping &new_mapping) {
  if (new_mapping.name == active_mapping.name) return;

  LOGGER->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid,
               new_mapping.name.c_str());
  active_mapping = new_mapping;

  for (auto &t : threads) {
    CPUList cpus;
    if (type & Type:PASSIV)
      cpus = active_mapping.cpus;
    else
      cpus = active_mapping.cpu(t.name);

    LOGGER->info(" * remap thread '%s' [%i] from cpu(s) %s to cpu(s) %s\n",
                 t.name.c_str(), t.tid,
                 string_util::join(t.cpus.cpulist(num_cpus), ",").c_str(),
                 string_util::join(cpus.cpulist(num_cpus), ",").c_str());

    t.cpus = cpus;

    cpu_set_t mask = cpus.cpu_set();
    if (sched_setaffinity(t.tid, sizeof(cpu_set_t), &mask) != 0)
      LOGGER->warning("Failed to set cpu affinity for thread '%s': %s\n",
                      t.name.c_str(), strerror(errno));
  }

  LOGGER->info(" * done\n");
}

tetris::ServerResponse Client::handle_message(const tetris::ClientMessage &msg)
{
  tetris::ServerResponse response{};
  response.set_type(tetris::ServerResponse::ERROR);

  switch(msg.type()) {
    case tetris::ClientMessage::MAPPINGS: 
      break;
    case tetris::ClientMessage::OPTIMIZATION_TARGET:
      break;
    case tetris::ClientMessage::FEATURE_SUBSCRIBE:
      break;
    case tetris::ClientMessage::REGISTER_THREAD:
      if (msg.has_thread_info()) {
        if (msg.thread_info().has_name())
          new_thread(msg.thread_info().tid(), msg.thread_info().name());
        else
          new_thread(msg.thread_info().tid());

        response.set_type(tetris::ServerResponse::ACKNOWLEDGE);
      }
      break;
    case tetris::ClientMessage::DELETE_THREAD:
      if (msg.has_thread_info()) {
        delete_thread(msg.thread_info().tid());
        response.set_type(tetris::ServerResponse::ACKNOWLEDGE);
      }
      break;
  }

  return response;
}

void Client::new_thread(int tid)
{
  LOGGER->info("New thread 'unnamed' [%i] registered for client '%s' [%d]\n", tid, exec.c_str(), pid);

  auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.tid == tid; });
  if (it == threads.end()) {
    /* With unnamed threads we don't support per-thread CPU assignments */
    threads.emplace_back(tid, active_mapping.cpus);

    /* Instead we assign the thread the CPUs of the mapping and let it migrate around within the assigned CPUs */
    LOGGER->info(" * enabled cpu(s) %s\n", string_util::join(active_mapping.cpus.cpulist(num_cpus), ",").c_str());

    cpu_set_t cset = active_mapping.cpus.cpu_set();
    sched_setaffinity(tid, sizeof(cset), &cset);
  } else {
    LOGGER->warning("Duplicate thread 'unnamed' [%i]\n", tid);
  }
}

void Client::new_thread(int tid, const std::string &name)
{
  LOGGER->info("New thread '%s' [%i] registered for client '%s' [%d]\n", name.c_str(), tid, exec.c_str(), pid);

  auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.tid == tid; });
  if (it == threads.end()) {
    CPUList cpus;
    if (type & Type::PER_THREAD) {
      /* If we have per-thread CPU assignments, get the available CPUs for this thread
       * from the mapping */
      cpus = active_mapping.cpu(name);
    } else {
      /* Otherwise we allow all CPUs that are used by the mapping */
      cpus = active_mapping.cpus;
    }

    threads.emplace_back(tid, name, cpus);

    /* Instead we assign the thread the CPUs of the mapping and let it migrate around their. */
    LOGGER->info(" * enabled cpu(s) %s\n", string_util::join(cpus.cpulist(num_cpus), ",").c_str());

    if (type & Type::PER_THREAD) {
      /* Inform the client, that it has to move this thread to a specific CPU */
      tetris::ServerMessage msg;

      msg.set_type(tetris::ServerMessage::MOVE_THREADS);

      Connection conn(push_path());
      protobuf_util::Send(conn.locked(), msg);

      tetris::ClientResponse resp;
      protobuf_util::Receive(conn.locked(), resp);
      if (resp.type() != tetris::ClientResponse::ACKNOWLEDGE) {
          LOGGER->warning(" --> Client responded with an error\n");
      }
    } else {
      cpu_set_t cset = cpus.cpu_set();
      sched_setaffinity(tid, sizeof(cset), &cset);
    }
  } else {
      LOGGER->warning("Duplicate thread '%s' [%i]\n", name.c_str(), tid);
  }
}

void Client::delete_thread(int tid)
{
  auto it = std::find_if(threads.begin(), threads.end(), [&](const auto &t) { return t.tid == tid; });
  if (it != threads.end()) {
    LOGGER->info("Deleting thread '%s' [%i] from client '%s' [%d]\n", it->name.c_str(), it->tid, exec.c_str(), pid);
    threads.erase(it);
  } else {
    LOGGER->warning("No thread known with ID %i, can't delete\n", tid);
  }
}

