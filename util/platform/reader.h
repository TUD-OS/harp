#ifndef __PLATFORM_READER_H__
#define __PLATFORM_READER_H__

#pragma once

#include <string>

#include "util/platform/platform.h"
#include <yaml-cpp/yaml.h>

class YamlPlatformReader {
public:
  std::unique_ptr<Platform> ReadFromFile(const std::string &filename) {
    YAML::Node platformNode = YAML::LoadFile(filename);

    auto platform = std::make_unique<Platform>();

    // Read CPU Types
    for (const auto &node : platformNode["core_types"]) {
      std::string name = node["type"].as<std::string>();
      int threads = node["threads"].as<int>();
      platform->AddCPUType(name, threads);
    }

    // Read Cores
    for (const auto &node : platformNode["cores"]) {
      std::string type = node["type"].as<std::string>();
      auto core = platform->AddCore(type);

      for (const auto &threadNode : node["threads"]) {
        std::string name = threadNode["name"].as<std::string>();
        int affinity = threadNode["affinity"].as<int>();
        core->AddThread(name, affinity);
      }
    }

    // Read equivalence scheme
    if (platformNode["equivalence_scheme"]) {
      std::string scheme = platformNode["equivalence_scheme"].as<std::string>();

      if (scheme == "core-type") {
        platform->SetEquivResAllocator(
            std::make_unique<CoreTypeBasedEquivResAllocator>(platform.get()));
      } else {
        throw std::runtime_error("Unknown Equivalent Resource Allocator");
      }
    }

    return platform;
  }
};

#endif /* __PLATFORM_READER_H__ */
