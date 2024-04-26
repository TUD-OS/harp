#ifndef __SCHED_FACTORY_H__
#define __SCHED_FACTORY_H__

#pragma once

#include "base.h"
#include "bruteforce.h"
#include "lr.h"

namespace tetris {

class ClientMapperFactory {
public:
  static std::unique_ptr<BaseClientMapper> Create(const std::string &type,
                                                  const Platform &platform) {
    if (type == "BF") {
      return std::make_unique<BruteforceMapper>(platform);
    } else if (type == "LR") {
      return std::make_unique<LagrangianRelaxationMapper>(platform);
    } else {
      throw std::invalid_argument("Unsupported mapper type: " + type);
    }
  }
};
} // namespace tetris

#endif
