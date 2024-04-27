#include "base.h"

#include "util/platform/platform.h"

namespace tetris {

BaseClientMapper::BaseClientMapper(const Platform &platform)
    : _platform{platform}, _platform_cores_count{
                               platform.GetCoreCountPerType()} {}
/**
 * Create a ClientMapping object with the current mapping list
 */
ClientMapping
BaseClientMapper::ToClientMapping(const std::vector<Client *> clients,
                                  const MappingList &ops,
                                  CPUCoreSet busy_cores) {
  auto &op_allocator = _platform.GetEquivResAllocator();
  ClientMapping client_mapping;
  for (int i = 0; i < clients.size(); ++i) {
    if (ops[i] != nullptr) {
      auto opt_opa = op_allocator.FindEquivOP(*ops[i], busy_cores);
      assert(opt_opa.has_value());
      auto opa = *opt_opa;
      client_mapping.Set(clients[i], opa);
      busy_cores |= _platform.ToCPUCoreSet(opa.threads());
    }
  }
  return client_mapping;
}

std::vector<std::vector<OperatingPoint>>
BaseClientMapper::GetClientsParetoFront(std::vector<Client *> clients) {
  std::vector<std::vector<OperatingPoint>> res;
  for (const auto &c : clients) {
    c->op_table->SetOperatingPointEvaluator(_evaluator);
    res.push_back(c->op_table->GetParetoFront());
    LOGGER->debug("  - '%s' [%d]: %d operating points.\n", c->exec.c_str(),
                  c->pid, res.back().size());
  }
  return res;
}

} // namespace tetris
