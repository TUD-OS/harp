#include "equiv_res_alloc.h"
#include "platform.h"

#include <iostream>

std::string CoreTypeBasedEquivResAllocator::GetEquivClassName(
    const CPUCoreSet &core_set) const {
  auto cores = _platform->GetCPUCores(core_set);
  std::map<std::string, int> core_type_cnt;
  for (auto &c : cores) {
    core_type_cnt[c->GetType().GetName()]++;
  }

  std::string res{};
  for (const auto &[ty, cnt] : core_type_cnt) {
    if (res.size() > 0) {
      res += " + ";
    }
    res += std::to_string(cnt) + " " + ty;
  }
  return res;
}

std::string CoreTypeBasedEquivResAllocator::GetEquivClassName(
    const CPUThreadSet &threads) const {
  return GetEquivClassName(_platform->ToCPUCoreSet(threads));
}
