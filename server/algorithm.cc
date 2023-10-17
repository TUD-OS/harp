#include "algorithm.h"
#include "util/operating_point.h"


using namespace tetris;

std::vector<OperatingPointAllocation> tetris_mappings(const EquivResAllocator &allocator,
                                     const std::vector<OperatingPoint> &ops,
                                     const CPUCoreSet &occupied_cpus) {
  /* Get all the mappings that don't overlap with the already occupied CPUs.
   * Consider all the transformed mappings as well (do the TETRiS). */
  std::vector<OperatingPointAllocation> result;

  /* TODO: Do this properly ;) */
  for (const auto &op : ops) {
    auto o = allocator.FindEquivOP(op, occupied_cpus);
    if (o) {
      result.push_back(o.value());
    }
  }

  return result;
}
