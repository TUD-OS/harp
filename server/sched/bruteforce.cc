#include "bruteforce.h"

#include "util/debug_util.h"

namespace tetris {

/**
 * Create a schedule object with the current mapping list
 */
Schedule BruteforceMapper::ToSchedule(const MappingList &ops) {
  Schedule schedule(_clients, _start_time, false);
  schedule.AddSegment();
  for (int i = 0; i < _clients.size(); ++i) {
    if (ops[i].has_value()) {
      schedule.SetOperatingPoint(0, _clients[i], *ops[i]);
    }
  }
  return schedule;
}

/**
 * Evaluates the quality of a given list of mappings.
 *
 * \param ops The list of mappings to be evaluated.
 * \return The number of applications and the total energy consumption.
 */
BruteforceMapper::MappingListValue
BruteforceMapper::Evaluate(const MappingList &ops) {
  int num_apps = 0;
  double energy = 0.0;

  for (const auto &m_opt : ops) {
    if (m_opt.has_value()) {
      num_apps++;
      energy += m_opt->characteristic("energy");
    }
  }

  return std::make_tuple(num_apps, energy);
}

/**
 * Updates the best solution found so far.
 *
 * \param current_mappings The current mappings.
 * \param best_mappings The best mappings found so far.
 * \param best_value The value of the best mappings found so far.
 */
void BruteforceMapper::UpdateBestSolution() {
  auto cur_value = Evaluate(_cur_ops);
  const auto [num_apps, energy] = cur_value;
  const auto [best_num_apps, best_energy] = _best_value;
  if ((num_apps > best_num_apps) ||
      (num_apps == best_num_apps && energy < best_energy)) {
    _best_value = cur_value;
    _best_ops = _cur_ops;
    LOGGER->debug("New best mapping: num_apps=%d, energy=%f\n", num_apps,
                  energy);
  }
}

/**
 * Recursively iterates over all clients and tries all possible mappings.
 *
 * \param n The index of the current client.
 * \param busy_cpus The list of busy CPUs.
 */
void BruteforceMapper::IterateClient(int n, CPUThreadSet busy_cpus) {
  if (n >= _clients.size()) {
    UpdateBestSolution();
    return;
  }

  CPUCoreSet busy_cores = _platform.ToCPUCoreSet(busy_cpus);
  auto &op_allocator = _platform.GetEquivResAllocator();
  auto &client = *_clients[n];

  for (auto &op : client.ops) {
    // Find an equivalent mapping that is not ovelapping with busy cpus
    auto opt_opa = op_allocator.FindEquivOP(op, busy_cores);
    if (opt_opa.has_value()) {
      auto opa = *opt_opa;
      _cur_ops[n] = opa;
      auto opa_cpus = opa.GetThreadSet();
      IterateClient(n + 1, busy_cpus | opa_cpus);
    }
  }

  // Assign no mapping
  {
    _cur_ops[n] = std::nullopt;
    IterateClient(n + 1, busy_cpus);
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
Schedule BruteforceMapper::GenerateSchedule(std::vector<Client *> clients,
                                            double start_time,
                                            CPUThreadSet blocked_cpus) {
  // Initialize internal data structures
  _clients = clients;
  _start_time = start_time;
  _blocked = blocked_cpus;
  _best_ops.clear();
  _best_value = std::make_tuple(0, 0.0);
  _cur_ops.clear();
  _cur_ops.resize(clients.size());

  // Start bruteforce
  IterateClient(0, blocked_cpus);

  return ToSchedule(_best_ops);
}

} // namespace tetris
