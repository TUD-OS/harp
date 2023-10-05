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

std::optional<std::map<int, int>>
CoreTypeBasedEquivResAllocator::GenerateCorePermutation(
    const CPUCoreSet &mapping_cores, const CPUCoreSet &used_cores) const {
  /* List of free cores for each type */
  std::map<const CPUType *, std::set<int>> free_cores{};
  for (auto &c : _platform->GetCPUCores()) {
    auto cid = c->GetID();
    if (used_cores.At(cid))
      continue;
    const CPUType *cty = &c->GetType();
    free_cores[cty].insert(cid);
  }

  std::map<int, int> core_perm{};
  CPUCoreSet remaining_cores{mapping_cores};

  /* First, keep threads on the same location if it is not used */
  for (auto cid : remaining_cores.GetList()) {
    auto c = _platform->FindCPUCore(cid);
    auto cty = &c->GetType();
    if (free_cores[cty].count(cid) == 1) {
      core_perm[cid] = cid;
      free_cores[cty].erase(cid);
      remaining_cores.Erase(cid);
    }
  }

  /* Then, move the remaining threads to the free cores */
  for (auto cid_from : remaining_cores.GetList()) {
    auto c = _platform->FindCPUCore(cid_from);
    auto cty = &c->GetType();

    /* No other cores of the same type */
    if (free_cores[cty].size() == 0) {
      return {};
    }

    int cid_to = *free_cores[cty].begin();
    core_perm[cid_from] = cid_to;
    free_cores[cty].erase(cid_to);
    remaining_cores.Erase(cid_from);
  }

  return core_perm;
}

std::map<int, int> CoreTypeBasedEquivResAllocator::ToThreadPermutation(
    const std::map<int, int> &core_perm) const {
  std::map<int, int> thread_perm;
  for (auto &[cid_from, cid_to] : core_perm) {
    auto threads_from = _platform->FindCPUCore(cid_from)->GetCPUThreads();
    auto threads_to = _platform->FindCPUCore(cid_to)->GetCPUThreads();
    assert(threads_from.size() == threads_to.size());
    for (int i = 0; i < threads_from.size(); ++i) {
      auto tid_from = threads_from[i]->GetID();
      auto tid_to = threads_to[i]->GetID();
      thread_perm[tid_from] = tid_to;
    }
  }
  return thread_perm;
}

std::optional<Mapping> CoreTypeBasedEquivResAllocator::FindEquivMapping(
    const Mapping &m, const CPUCoreSet &used_cpus) const {
  auto core_perm =
      GenerateCorePermutation(_platform->ToCPUCoreSet(m.cpus), used_cpus);
  if (!core_perm) {
    return {};
  }

  auto thread_perm = ToThreadPermutation(*core_perm);

  return Mapping{m, thread_perm};
}
