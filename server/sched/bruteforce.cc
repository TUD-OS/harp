#include "bruteforce.h"

#include "util/debug_util.h"

namespace tetris {

/**
 * Create a schedule object with the current mapping list
 */
std::unique_ptr<Schedule> BruteforceMapper::ToSchedule(const MappingList &ops,
                                                       CPUCoreSet busy_cores) {
  auto &op_allocator = _platform.GetEquivResAllocator();
  auto schedule = std::make_unique<Schedule>(_clients, _start_time, false);
  schedule->AddSegment();
  for (int i = 0; i < _clients.size(); ++i) {
    if (ops[i] != nullptr) {
      auto opt_opa = op_allocator.FindEquivOP(*ops[i], busy_cores);
      assert(opt_opa.has_value());
      auto opa = *opt_opa;
      schedule->SetOperatingPoint(0, _clients[i], opa);
      busy_cores |= _platform.ToCPUCoreSet(opa.threads());
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
void BruteforceMapper::UpdateBestSolution(int num_apps, double value) {
  const auto [best_num_apps, best_value] = _best_value;
  if ((num_apps > best_num_apps) ||
      (num_apps == best_num_apps && value < best_value)) {
    _best_value = std::make_pair(num_apps, value);
    _best_ops = _cur_ops;
    LOGGER->debug("New best mapping: num_apps=%d, value=%f\n", num_apps, value);
  }
}

/**
 * Recursively iterates over all clients and tries all possible mappings.
 *
 * \param n The index of the current client.
 * \param busy_cores The list of busy CPUs.
 */
void BruteforceMapper::IterateClient(
    int n, const std::map<std::string, int> &used_cores, int cur_apps,
    double cur_value) {
  if (n >= _clients.size()) {
    UpdateBestSolution(cur_apps, cur_value);
    return;
  }

  // Check whether the current partial solution still can improve
  {
    int max_apps = cur_apps + (_clients.size() - n);
    int best_apps = std::get<0>(_best_value);
    double best_value = std::get<1>(_best_value);
    if (max_apps < best_apps)
      return;

    if (max_apps == best_apps) {
      if (cur_value > best_value)
        return;
    }
  }

  for (auto &op : _cl_pareto[n]) {
    // Check the operating point can be added
    std::map<std::string, int> added_cores{used_cores};
    bool all_fit = true;
    for (auto &[core_type, op_core_count] : op.core_counts()) {
      added_cores[core_type] += op_core_count;
      if (added_cores[core_type] > _platform_cores_count[core_type])
        all_fit = false;
    }
    if (!all_fit) {
      continue;
    }
    _cur_ops[n] = &op;
    double rem_cratio = 1.0 - _clients[n]->progress;
    double op_value = _objective->EvaluateOP(op, rem_cratio);
    IterateClient(n + 1, added_cores, cur_apps + 1, cur_value + op_value);
  }

  // Assign no mapping
  {
    _cur_ops[n] = nullptr;
    IterateClient(n + 1, used_cores, cur_apps, cur_value);
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
  _cl_pareto.clear();
  _best_value = std::make_tuple(0, 0.0);
  _cur_ops.clear();
  _cur_ops.resize(clients.size());

  // Filter Pareto-front for each client
  for (const auto &c : _clients) {
    _cl_pareto.push_back(_objective->FilterParetoFront(_platform, c->ops));
    LOGGER->debug("Filtering operating points for '%s' [%d] from %d to %d.\n",
                  c->exec.c_str(), c->pid, c->ops.size(),
                  _cl_pareto.back().size());
  }

  // Start bruteforce
  IterateClient(0, _platform.GetCoreCountPerType(_blocked), 0, 0.0);

  return ToSchedule(_best_ops, _blocked);
}

} // namespace tetris
