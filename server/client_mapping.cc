#include "client_mapping.h"

namespace tetris {

CPUThreadSet ClientMapping::GetAllThreads() const {
  CPUThreadSet allThreads;
  for (const auto &pair : _map) {
    allThreads |= pair.second.threads();
  }
  return allThreads;
}

bool ClientMapping::HasOverlaps() const {
  CPUThreadSet totalSet;
  for (const auto &pair : _map) {
    CPUThreadSet currentSet = pair.second.threads();
    if (totalSet.OverlapsWith(currentSet)) {
      return true;
    }
    totalSet |= currentSet;
  }
  return false;
}

std::string ClientMapping::ToString() const {
  std::stringstream ss;
  ss << "ClientMapping | " << _map.size() << " clients\n";
  for (const auto &[c, opa] : _map) {
    ss << "  - '" << c->exec << "' [" << c->pid << "] | ";
    ss << "OP '" << opa.name() << "' utility " << opa.utility() << " power "
       << opa.power() << " | ";
    auto cores = opa.threads();
    ss << string_util::join(cores.GetList(), ",") << "\n";
  }
  return ss.str();
}

std::tuple<int, double>
ClientMapping::EvaluateWith(const OperatingPointEvaluator &evaluator) const {
  int num_apps = 0;
  double value = 0.0;
  for (auto &[client, op] : _map) {
    value += evaluator.Evaluate(op.base);
    num_apps += 1;
  }
  return std::make_tuple(num_apps, value);
}

} // namespace tetris
