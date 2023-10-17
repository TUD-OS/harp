#include "bruteforce.h"

#include "util/debug_util.h"

namespace tetris {

/**
 * Create a schedule object with the current mapping list
 */
std::unique_ptr<Schedule> BruteforceMapper::ToSchedule(const MappingList &ops) {
  auto schedule = std::make_unique<Schedule>(_clients, _start_time, false);
  schedule->AddSegment();
  for (int i = 0; i < _clients.size(); ++i) {
    if (ops[i].has_value()) {
      schedule->SetOperatingPoint(0, _clients[i], *ops[i]);
    }
  }
  return schedule;
}

/**
 * Updates the best solution found so far.
 *
 * \param current_mappings The current mappings.
 * \param best_mappings The best mappings found so far.
 * \param best_value The value of the best mappings found so far.
 */
void BruteforceMapper::UpdateBestSolution() {
  auto schedule = ToSchedule(_cur_ops);
  auto cur_value = _objective->EvaluateSchedule(*schedule.get());
  const auto [num_apps, value] = cur_value;
  const auto [best_num_apps, best_value] = _best_value;
  if ((num_apps > best_num_apps) ||
      (num_apps == best_num_apps && value < best_value)) {
    _best_value = cur_value;
    _best_schedule = std::move(schedule);
    LOGGER->debug("New best mapping: num_apps=%d, value=%f\n", num_apps, value);
  }
}

/**
 * Recursively iterates over all clients and tries all possible mappings.
 *
 * \param n The index of the current client.
 * \param busy_cores The list of busy CPUs.
 */
void BruteforceMapper::IterateClient(int n, CPUCoreSet busy_cores) {
  if (n >= _clients.size()) {
    UpdateBestSolution();
    return;
  }

  auto &op_allocator = _platform.GetEquivResAllocator();
  auto &client = *_clients[n];

  for (auto &op : client.ops) {
    // Find an equivalent mapping that is not ovelapping with busy cpus
    auto opt_opa = op_allocator.FindEquivOP(op, busy_cores);
    if (opt_opa.has_value()) {
      auto opa = *opt_opa;
      _cur_ops[n] = opa;
      auto opa_cores = _platform.ToCPUCoreSet(opa.GetThreadSet());
      IterateClient(n + 1, busy_cores | opa_cores);
    }
  }

  // Assign no mapping
  {
    _cur_ops[n] = std::nullopt;
    IterateClient(n + 1, busy_cores);
  }
}

/**
 * Selects client mappings considering a list of blocked CPUs.
 *
 * \param clients The list of clients for which mappings need to be selected.
 * \param start_time The start time of the schedule
 * \param blocked_cpus The list of blocked CPUs.
 * \return The selected mappings.
 */
std::unique_ptr<Schedule>
BruteforceMapper::GenerateSchedule(std::vector<Client *> clients,
                                   double start_time,
                                   CPUCoreSet blocked_cores) {
  // Initialize internal data structures
  _clients = clients;
  _start_time = start_time;
  _blocked = blocked_cores;
  _best_schedule = std::unique_ptr<Schedule>();
  _best_value = std::make_tuple(0, 0.0);
  _cur_ops.clear();
  _cur_ops.resize(clients.size());

  // Start bruteforce
  IterateClient(0, _blocked);

  return std::move(_best_schedule);
}

} // namespace tetris
