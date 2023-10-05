#ifndef __ALGORITHM_H__
#define __ALGORITHM_H__

#pragma once

#include "util/mapping.h"
#include "util/platform/equiv_res_alloc.h"

#include <vector>

std::vector<Mapping> tetris_mappings(const EquivResAllocator &allocator,
                                     const std::vector<Mapping> &all_mappings,
                                     const CPUCoreSet &occupied_cpus);

#endif /* __ALGORITHM_H__ */
