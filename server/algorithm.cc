#include "algorithm.h"

std::vector<Mapping> tetris_mappings(const EquivResAllocator &allocator,
                                     const std::vector<Mapping> &all_mappings,
                                     const CPUCoreSet &occupied_cpus) {
  /* Get all the mappings that don't overlap with the already occupied CPUs.
   * Consider all the transformed mappings as well (do the TETRiS). */
  std::vector<Mapping> result;

  /* TODO: Do this properly ;) */
  for (const auto &m : all_mappings) {
    auto opt_m = allocator.FindEquivMapping(m, occupied_cpus);
    if (opt_m) {
      result.push_back(opt_m.value());
    }
  }

  return result;
}
