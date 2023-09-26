#ifndef __ALGORITHM_H__
#define __ALGORITHM_H__

#pragma once

#include "util/platform/equiv_res_alloc.h"
#include "util/operating_point.h"

#include <vector>

std::vector<tetris::OperatingPointAllocation> tetris_mappings(const tetris::EquivResAllocator &allocator,
                                     const std::vector<tetris::OperatingPoint> &ops,
                                     const tetris::CPUCoreSet &occupied_cpus);

#endif /* __ALGORITHM_H__ */
