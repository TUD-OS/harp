#ifndef __OPERATING_POINT_TABLE_H__
#define __OPERATING_POINT_TABLE_H__

#pragma once

#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/platform/platform.h"
#include "util/regression.h"
#include "util/string_util.h"

#include <vector>

namespace tetris {

/**
 * \struct OperaringPointResult
 * \brief Struct to hold results for an operating point, including utility and
 * power consumption metrics.
 */
struct OperatingPointResult {
  double utility; // Instructions per second
  double power;   // Average power consumption
};

/**
 * \class OperatingPointTable
 * \brief Abstract base class for managing operating points on a platform.
 *
 * Supports operations like adding and retrieving operating points, and
 * potentially measuring or approximating them.
 */
class OperatingPointTable {
public:
  explicit OperatingPointTable(const Platform &platform, bool measurement,
                               bool approximation)
      : _platform(platform), _measurement(measurement),
        _approximation(approximation) {}

  virtual ~OperatingPointTable() = default;

  bool EnabledMeasurement() const { return _measurement; }

  bool EnabledApproximation() const { return _approximation; }

  virtual std::vector<OperatingPoint> GetParetoFront() = 0;

  virtual std::vector<OperatingPoint> GetOperatingPoints(bool approximated) = 0;

  virtual void AddOperatingPoint(const OperatingPoint &op) = 0;

  virtual void
  AddOperatingPointMeasurement(const OperatingPoint &op,
                               const OperatingPointResult &result) = 0;

  virtual void Clear() = 0;

protected:
  const Platform &_platform;
  bool _measurement;
  bool _approximation;
};

/**
 * \class ThreadSetOperatingPointTable
 * \brief An implementation of OperatingPointTable that manages thread-based
 * operating points.
 *
 * Utilizes exponential moving averages (EMA) for dynamic data handling and
 * regression for approximations.
 */
class ThreadSetOperatingPointTable : public OperatingPointTable {
public:
  explicit ThreadSetOperatingPointTable(const Platform &platform,
                                        bool measurement = true,
                                        bool approximation = true,
                                        double ema_alpha = 0.1)
      : OperatingPointTable(platform, measurement, approximation),
        _ema_alpha(ema_alpha) {

    // Initialize core thread levels
    _core_threads_count = _platform.GetThreadCapacityInfo();
    _num_core_thread_levels = 0;
    for (const auto &[key, v] : _core_threads_count) {
      _core_offsets[key] = _num_core_thread_levels;
      _num_core_thread_levels += v;
    }

    // Initialize core thread level names
    _core_thread_level_names.resize(_num_core_thread_levels);
    for (const auto &[key, offset] : _core_offsets) {
      auto &thread_count = _core_threads_count.at(key);
      for (int i = 0; i < thread_count; ++i) {
        if (thread_count == 1) {
          _core_thread_level_names[offset + i] = key;
        } else {
          _core_thread_level_names[offset + i] =
              key + "(" + std::to_string(i + 1) + ")";
        }
      }
    }

    // Initialize cores count map
    _cores_count = _platform.GetCoreCountPerType();

    // Generate all possible configurations
    // (This could be optimized by saving the vector statically)
    _all_configurations = GenerateAllConfigurations();

    if (EnabledApproximation()) {
      std::vector<std::string> features{"utility", "power"};
      _regression = std::make_unique<Regression>(_num_core_thread_levels, 2, 2);
    }
  }

  std::vector<OperatingPoint> GetParetoFront() override {
    throw std::runtime_error("NYI");
  }

  std::vector<OperatingPoint>
  GetOperatingPoints(bool approximated = true) override {
    std::vector<OperatingPoint> res;

    if (approximated && !EnabledApproximation()) {
      LOGGER->warning("Cannot return approximated points since the "
                      "approximation was not enabled\n");
      approximated = false;
    }

    for (const auto &config : _all_configurations) {
      if (_ops.count(config) > 0) {
        auto &opres = _ops.at(config);
        auto op = ConstructOperatingPoint(config, opres);
        res.push_back(op);
      } else {
        if (approximated) {
          throw std::runtime_error("NYI");
        }
      }
    }

    return res;
  }

  void AddOperatingPoint(const OperatingPoint &op) override {
    auto config = GetConfiguration(op);
    if (_ops.count(config) > 0) {
      LOGGER->warning("The operating point with the same configuration (%s) "
                      "was already added. Rewriting the old operating point.\n",
                      GetConfigurationString(config).c_str());
    }

    OperatingPointResult res{};
    if (op.characteristics.count("utility") == 0) {
      LOGGER->warning("Missing utility information for the configutation %s.\n",
                      GetConfigurationString(config).c_str());
    }
    res.utility = op.characteristic("utility");

    if (op.characteristics.count("power") == 0) {
      LOGGER->warning("Missing power information for the configutation %s.\n",
                      GetConfigurationString(config).c_str());
    }
    res.power = op.characteristic("power");

    _ops.emplace(config, res);
    _sample_counts.emplace(config, 1);
  }

  void
  AddOperatingPointMeasurement(const OperatingPoint &op,
                               const OperatingPointResult &result) override {
    if (!EnabledMeasurement()) {
      throw std::runtime_error(
          "OperatingPointTable does not support adding measured data");
    }

    auto config = GetConfiguration(op);

    if (_ops.count(config) == 0) {
      _ops.emplace(config, result);
      _sample_counts.emplace(config, 1);
      return;
    }
    auto &opres = _ops.at(config);
    auto &sample_count = _sample_counts.at(config);
    double alpha_eff = std::max(_ema_alpha, 1.0 / (sample_count + 1));
    opres.utility =
        alpha_eff * result.utility + (1 - alpha_eff) * opres.utility;
    opres.power = alpha_eff * result.power + (1 - alpha_eff) * opres.power;
    sample_count += 1;
  }

  void Clear() override {
    _ops.clear();
    _sample_counts.clear();
  }

private:
  using Configuration = std::vector<int>;

  Configuration GetConfiguration(const OperatingPoint &op) const {
    auto &threads = op.cpus;
    auto thread_usage = _platform.GetThreadUsageInfo(threads);

    Configuration config(_num_core_thread_levels);
    // Fill input features
    for (const auto &[type, distr] : thread_usage) {
      for (int i = 0; i < distr.size(); ++i) {
        config[_core_offsets.at(type) + i] = distr[i];
      }
    }

    if (config.size() != _num_core_thread_levels) {
      std::string msg =
          "Unexpected number of elements in the configuration vector: " +
          std::to_string(config.size()) +
          ". Expected: " + std::to_string(_num_core_thread_levels) + ".";
      throw std::runtime_error(msg);
    }

    return config;
  }

  std::string GetConfigurationString(const Configuration &config) const {
    if (config.size() != _num_core_thread_levels) {
      std::string msg =
          "Unexpected number of elements in the configuration vector: " +
          std::to_string(config.size()) +
          ". Expected: " + std::to_string(_num_core_thread_levels) + ".";
      throw std::runtime_error(msg);
    }

    std::vector<std::string> s;
    for (int i = 0; i < config.size(); ++i) {
      s.push_back(_core_thread_level_names[i] + ":" +
                  std::to_string(config.at(i)));
    }

    return string_util::join(s, ", ");
  }

  std::vector<Configuration> GenerateAllConfigurations() const {
    std::vector<Configuration> all;
    Configuration current;
    GenerateAllConfigurationsLevel(all, current);
    return all;
  }

  void GenerateAllConfigurationsLevel(std::vector<Configuration> &all,
                                      Configuration &current) const {
    int level = current.size();

    if (level == _num_core_thread_levels) {
      int sum_cores = 0;
      for (const auto &v : current) {
        sum_cores += v;
      }
      if (sum_cores > 0) {
        all.push_back(current);
      }
      return;
    }

    // Determine the cyrrent core type
    std::string type{};
    for (const auto &[key, offset] : _core_offsets) {
      auto &threads_count = _core_threads_count.at(key);
      if (offset <= level && level < offset + threads_count) {
        type = key;
        break;
      }
    }
    assert(type != "");

    // Count remaining cores of this type
    int remaining = _cores_count.at(type);
    int offset = _core_offsets.at(type);
    int thread_count = _core_threads_count.at(type);
    for (int i = offset; i < std::min(level, offset + thread_count); ++i) {
      remaining -= current.at(i);
    }

    // Iterate over the current level values
    for (int i = 0; i <= remaining; ++i) {
      current.push_back(i);
      GenerateAllConfigurationsLevel(all, current);
      current.pop_back();
    }
  }

  CPUThreadSet ConstructCPUThreadSet(const Configuration &config) const {
    std::map<std::string, std::vector<int>> thread_usage;
    for (const auto &[type, offset] : _core_offsets) {
      std::vector<int> core_thread_usage{};
      int threads_count = _core_threads_count.at(type);
      for (int i = 0; i < threads_count; ++i) {
        core_thread_usage.push_back(config.at(offset + i));
      }
      thread_usage.emplace(type, core_thread_usage);
    }

    return _platform.GetCPUThreadSetFromThreadUsageInfo(thread_usage);
  }

  OperatingPoint
  ConstructOperatingPoint(const Configuration &config,
                          const OperatingPointResult &res) const {
    auto name = GetConfigurationString(config);
    std::map<std::string, double> characteristics{{"utility", res.utility},
                                                  {"power", res.power}};
    auto thread_set = ConstructCPUThreadSet(config);
    auto core_set = _platform.ToCPUCoreSet(thread_set);
    auto core_count = _platform.GetCoreCountPerType(core_set);

    return OperatingPoint(name, characteristics, thread_set, core_count);
  }

  // Exponential Moving Average parameter
  double _ema_alpha;

  // Helper struct to encode/decode configuration
  int _num_core_thread_levels;
  std::map<std::string, int> _core_threads_count;
  std::map<std::string, int> _core_offsets;
  std::map<std::string, int> _cores_count;
  std::vector<std::string> _core_thread_level_names;

  // Helper data: all possible configurations
  std::vector<Configuration> _all_configurations;

  // Store primary operating points
  std::map<Configuration, OperatingPointResult> _ops; // EMA results
  std::map<Configuration, int> _sample_counts;        // Sample counts

  // Approximation model
  std::unique_ptr<Regression> _regression;
  std::map<Configuration, OperatingPointResult> _approx_ops;
};

/**
 * \class CustomOperaringPointTable
 * \brief OperatingPointTable which supports custom mappings.
 *
 * Does not support measurement or approximation functionalities.
 */
class CustomOperatingPointTable : public OperatingPointTable {
public:
  explicit CustomOperatingPointTable(const Platform &platform,
                                     bool measurement = false)
      : OperatingPointTable(platform, measurement, false) {}

  virtual std::vector<OperatingPoint> GetParetoFront() = 0;

  std::vector<OperatingPoint>
  GetOperatingPoints(bool approximated = false) override {
    if (approximated) {
      LOGGER->warning(
          "CustomOperatingPointTable does not support approximation.");
    }
    return _ops;
  }

  void AddOperatingPoint(const OperatingPoint &op) override {
    _ops.emplace_back(op);
  }

  void
  AddOperatingPointMeasurement(const OperatingPoint &op,
                               const OperatingPointResult &result) override {
    throw std::runtime_error("CustomOperatingPointTable does not support "
                             "adding measured Operating Point");
  }

  void Clear() override { _ops.clear(); }

private:
  std::vector<OperatingPoint> _ops;
};

} // namespace tetris

#endif
