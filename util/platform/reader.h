#ifndef __PLATFORM_READER_H__
#define __PLATFORM_READER_H__

#pragma once

#include <string>

#include "util/platform/platform.h"
#include <yaml-cpp/yaml.h>

class YamlPlatformReader {
public:
  Platform ReadFromFile(const std::string &filename) {
    YAML::Node platformNode = YAML::LoadFile(filename);

    Platform platform;

    // Read CPU Types
    for (const auto &node : platformNode["core_types"]) {
      std::string name = node["type"].as<std::string>();
      int threads = node["threads"].as<int>();
      platform.AddCPUType(name, threads);
    }

    // Read Cores
    for (const auto &node : platformNode["cores"]) {
      std::string type = node["type"].as<std::string>();
      CPUCore &core = platform.AddCore(type);

      for (const auto &threadNode : node["threads"]) {
        std::string name = threadNode["name"].as<std::string>();
        int affinity = threadNode["affinity"].as<int>();
        core.AddThread(name, affinity);
      }
    }

    return platform;
  }
};

#endif /* __PLATFORM_READER_H__ */
