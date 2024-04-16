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
  /**
   * Constructs an operating point table.
   * \param platform The platform associated with the operating points.
   * \param measurement Indicates if measurement capabilities are enabled.
   * \param approximation Indicates if approximation capabilities are enabled.
   */
  explicit OperatingPointTable(const Platform &platform, bool measurement,
                               bool approximation)
      : _platform(platform), _measurement(measurement),
        _approximation(approximation) {}

  virtual ~OperatingPointTable() = default;

  /**
   * Returns if measurement functionality is enabled.
   */
  bool EnabledMeasurement() const { return _measurement; }

  /**
   * Returns if approximation functionality is enabled.
   */
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
                                        double ema_alpha = 0.1);

  std::vector<OperatingPoint> GetParetoFront() override {
    throw std::runtime_error("NYI");
  }

  std::vector<OperatingPoint>
  GetOperatingPoints(bool approximated = true) override;

  void AddOperatingPoint(const OperatingPoint &op) override;

  void
  AddOperatingPointMeasurement(const OperatingPoint &op,
                               const OperatingPointResult &result) override;

  void Clear() override {
    _ops.clear();
    _sample_counts.clear();
    _update_approximated = true;
  }

private:
  using Configuration = std::vector<int>;

  Configuration GetConfiguration(const OperatingPoint &op) const;

  std::string GetConfigurationString(const Configuration &config) const;

  std::vector<Configuration> GenerateAllConfigurations() const {
    std::vector<Configuration> all;
    Configuration current;
    GenerateAllConfigurationsLevel(all, current);
    return all;
  }

  void GenerateAllConfigurationsLevel(std::vector<Configuration> &all,
                                      Configuration &current) const;

  CPUThreadSet ConstructCPUThreadSet(const Configuration &config) const;

  OperatingPoint ConstructOperatingPoint(const Configuration &config,
                                         const OperatingPointResult &res) const;

  void GenerateApproximatedOperatingPoints();

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
  bool _update_approximated; // Flag to update approximated OP
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
