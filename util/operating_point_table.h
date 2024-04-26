#ifndef __OPERATING_POINT_TABLE_H__
#define __OPERATING_POINT_TABLE_H__

#pragma once

#include <vector>

#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/operating_point_evaluator.h"
#include "util/pareto.h"
#include "util/regression.h"
#include "util/string_util.h"

namespace tetris {

class Platform;

/**
 * \class OperatingPointTable
 * \brief Abstract base class for managing operating points on a platform.
 *
 * Supports operations like adding and retrieving operating points, and
 * potentially measuring or approximating them.
 */
class OperatingPointTable {
public:
  using ObjectiveBetterFunc =
      ParetoFrontFilter<OperatingPoint>::ObjectiveBetterFunc;

  /**
   * Constructs an operating point table.
   * \param platform The platform associated with the operating points.
   * \param measurement Indicates if measurement capabilities are enabled.
   * \param approximation Indicates if approximation capabilities are enabled.
   */
  explicit OperatingPointTable(
      const Platform &platform,
      std::shared_ptr<OperatingPointEvaluator> evaluator, bool measurement,
      bool approximation)
      : _platform(platform), _measurement(measurement),
        _approximation(approximation), _pareto_filter{nullptr} {
    SetOperatingPointEvaluator(std::move(evaluator));
  }

  virtual ~OperatingPointTable() = default;

  /**
   * Returns if measurement functionality is enabled.
   */
  bool EnabledMeasurement() const { return _measurement; }

  /**
   * Returns if approximation functionality is enabled.
   */
  bool EnabledApproximation() const { return _approximation; }

  virtual std::vector<OperatingPoint> GetOperatingPoints(bool approximated) = 0;

  virtual void AddOperatingPoint(const OperatingPoint &op) = 0;

  virtual void AddOperatingPoint(const OperatingPoint::Configuration &config,
                                 const OperatingPoint::Metrics &metrics) = 0;

  virtual void
  AddOperatingPointMeasurement(const OperatingPoint::Configuration &config,
                               const OperatingPoint::Metrics &result) = 0;

  virtual void Clear() = 0;

  void SetOperatingPointEvaluator(
      std::shared_ptr<OperatingPointEvaluator> evaluator);

  std::vector<OperatingPoint> GetParetoFront();

protected:
  const Platform &_platform;
  bool _measurement;
  bool _approximation;

  // Manage Pareto-Front Filtering
  std::shared_ptr<OperatingPointEvaluator> _evaluator;
  std::unique_ptr<ParetoFrontFilter<OperatingPoint>> _pareto_filter;
  std::vector<OperatingPoint> _pareto; // Store the current Pareto front
  bool _update_pareto;                 // Flag to update
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
  explicit ThreadSetOperatingPointTable(
      const Platform &platform,
      std::shared_ptr<OperatingPointEvaluator> evaluator = nullptr,
      bool measurement = true, bool approximation = true,
      double ema_alpha = 0.1);

  std::vector<OperatingPoint>
  GetOperatingPoints(bool approximated = true) override;

  void AddOperatingPoint(const OperatingPoint &op) override;

  void AddOperatingPoint(const OperatingPoint::Configuration &config,
                         const OperatingPoint::Metrics &metrics) override;

  void
  AddOperatingPointMeasurement(const OperatingPoint::Configuration &op,
                               const OperatingPoint::Metrics &result) override;

  void Clear() override {
    _ops.clear();
    _sample_counts.clear();
    _update_approximated = true;
  }

private:
  using Configuration = std::vector<int>;

  Configuration GetConfiguration(const OperatingPoint::Configuration &op) const;

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

  OperatingPoint
  ConstructOperatingPoint(const Configuration &config,
                          const OperatingPoint::Metrics &res) const;

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
  std::map<Configuration, OperatingPoint::Metrics> _ops; // EMA results
  std::map<Configuration, int> _sample_counts;           // Sample counts

  // Approximation model
  std::unique_ptr<Regression> _regression;
  std::map<Configuration, OperatingPoint::Metrics> _approx_ops;
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
  explicit CustomOperatingPointTable(
      const Platform &platform,
      std::shared_ptr<OperatingPointEvaluator> evaluator = nullptr,
      bool measurement = false)
      : OperatingPointTable(platform, std::move(evaluator), measurement,
                            false) {}

  std::vector<OperatingPoint>
  GetOperatingPoints(bool approximated = false) override {
    if (approximated) {
      LOGGER->warning(
          "CustomOperatingPointTable does not support approximation.");
    }
    std::vector<OperatingPoint> res;
    for (const auto &[_, op] : _ops) {
      res.push_back(op);
    }
    return res;
  }

  void AddOperatingPoint(const OperatingPoint &op) override {
    if (_ops.contains(op.name())) {
      LOGGER->warning("Operating Point \"%s\" was already added. Overriding.",
                      op.name().c_str());
    }
    _ops.emplace(op.name(), op);
    _update_pareto = true;
  }

  void AddOperatingPoint(const OperatingPoint::Configuration &config,
                         const OperatingPoint::Metrics &metrics) override {
    AddOperatingPoint(OperatingPoint(config, metrics));
  }

  void
  AddOperatingPointMeasurement(const OperatingPoint::Configuration &config,
                               const OperatingPoint::Metrics &result) override {
    throw std::runtime_error("CustomOperatingPointTable does not support "
                             "adding measured Operating Point");
  }

  void Clear() override {
    _ops.clear();
    _update_pareto = true;
  }

private:
  std::map<std::string, OperatingPoint> _ops;
};

} // namespace tetris

#endif
