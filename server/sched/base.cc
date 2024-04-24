#include "base.h"

#include "util/platform/platform.h"

namespace tetris {

BaseClientMapper::BaseClientMapper(
    const Platform &platform, std::unique_ptr<OptimizationObjective> objective)
    : _platform{platform},
      _platform_cores_count{platform.GetCoreCountPerType()},
      _objective{std::move(objective)} {}
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

} // namespace tetris
