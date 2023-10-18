#include "objective.h"

#include "util/platform/platform.h"

namespace tetris {

std::vector<OperatingPoint> OptimizationObjective::FilterParetoFront(
    const Platform &platform, const std::vector<OperatingPoint> &ops) const {
  std::vector<OPParetoState> op_states;
  for (const auto &op : ops) {
    CPUCoreSet core_set = platform.ToCPUCoreSet(op.cpus);
    auto cores = platform.CountCoresPerType(core_set);
    auto value = EvaluateOP(op);
    op_states.emplace_back(cores, value, true);
  }

  for (int i = 0; i < ops.size(); ++i) {
    if (!op_states[i].is_pareto)
      continue;
    for (int j = 0; j < ops.size(); ++j) {
      if (i == j)
        continue;
      if (!op_states[j].is_pareto)
        continue;
      if (op_states[i].Dominates(op_states[j])) {
        op_states[j].is_pareto = false;
      }
    }
  }

  std::vector<OperatingPoint> res;

  for (int i = 0; i < ops.size(); ++i) {
    if (op_states[i].is_pareto)
      res.push_back(ops[i]);
  }
  return res;
}

} // namespace tetris
