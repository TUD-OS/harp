#include "bruteforce.h"

#include "util/debug_util.h"
#include "util/platform/platform.h"

namespace tetris {

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

  for (auto &op : _client_ops[n]) {
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
    double op_value = _objective->EvaluateOP(op);
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
 * \param blocked_cpus The list of blocked CPUs.
 * \return The selected mappings.
 */
ClientMapping
BruteforceMapper::GenerateClientMapping(std::vector<Client *> clients,
                                        CPUCoreSet blocked_cores) {
  // Initialize internal data structures
  _clients = clients;
  _blocked = blocked_cores;
  _client_ops.clear();
  _best_value = std::make_tuple(0, 0.0);
  _cur_ops.clear();
  _cur_ops.resize(clients.size());

  LOGGER->debug("Allocating clients to the resource using BruteforceMapper\n");
  LOGGER->debug("Current clients:\n");

  // Filter Pareto-front for each client
  for (const auto &c : _clients) {
    _client_ops.push_back(c->op_table->GetParetoFront());
    LOGGER->debug("  - '%s' [%d]: %d operating points.\n", c->exec.c_str(),
                  c->pid, _client_ops.back().size());
  }

  // Start bruteforce
  IterateClient(0, _platform.GetCoreCountPerType(_blocked), 0, 0.0);

  return ToClientMapping(clients, _best_ops, _blocked);
}

} // namespace tetris
