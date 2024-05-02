#ifndef __OPERATING_POINT_TABLE_H__
#define __OPERATING_POINT_TABLE_H__

#pragma once

#include <optional>
#include <vector>

#include "proto/tetris.pb.h"

#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/operating_point_evaluator.h"
#include "util/pareto.h"
#include "util/regression.h"
#include "util/string_util.h"

namespace tetris {

class Platform;

inline int kExplorationPoints = 4;
inline int kMatureReliablePoints = 5;
inline int kReliableMeasurements = 10;

enum class OperatingPointTableStage {
  kStatic,  // No approximation, used by CustomOperatingPointTable
  kInitial, // Initial data gathering and reliance on platform-default points
  kExploration, // Operating point exploration and model refinement
  kMature,      // Matured model, approximation using only reliable points
};

// Helper function to print stage
std::ostream &operator<<(std::ostream &os, OperatingPointTableStage stage);

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
   */
  OperatingPointTable(const Platform &platform,
                      std::shared_ptr<OperatingPointEvaluator> evaluator,
                      OperatingPointTableStage stage, bool measurement,
                      bool approximation)
      : _platform(platform), _stage(stage),
        _measurement(measurement), _pareto_filter{nullptr} {
    SetOperatingPointEvaluator(std::move(evaluator));
  }

  virtual ~OperatingPointTable() = default;

  /** Returns the current stage of the table */
  OperatingPointTableStage Stage() const { return _stage; }

  /**
   * Returns if measurement functionality is enabled.
   */
  bool EnabledMeasurement() const { return _measurement; }

  virtual std::vector<OperatingPoint> GetOperatingPoints() = 0;

  virtual void AddOperatingPoint(const OperatingPoint &op) = 0;

  virtual void AddOperatingPoint(const OperatingPoint::Configuration &config,
                                 const OperatingPoint::Metrics &metrics) = 0;

  void AddOperatingPoint(const ClientMessage::OperatingPointsInfo::OPData &op);

  virtual void
  AddOperatingPointMeasurement(const OperatingPoint::Configuration &config,
                               const OperatingPoint::Metrics &result) = 0;

  virtual void Clear() = 0;

  void SetOperatingPointEvaluator(
      std::shared_ptr<OperatingPointEvaluator> evaluator);

  std::vector<OperatingPoint> GetParetoFront();

  virtual std::optional<OperatingPoint>
  GetOperatingPointToMeasure(const CPUCoreSet &core_set) = 0;

  virtual void Dump() = 0;

protected:
  const Platform &_platform;
  OperatingPointTableStage _stage;
  bool _measurement;

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

  std::vector<OperatingPoint> GetOperatingPoints() override;

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

  std::optional<OperatingPoint>
  GetOperatingPointToMeasure(const CPUCoreSet &core_set) override;

  void Dump() override;

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

  void EvaluateStage();

  const OperatingPoint::Metrics &
  GetOperatingPointMetrics(const Configuration &config) const;

  CPUThreadSet ConstructCPUThreadSet(const Configuration &config) const;

  OperatingPoint
  ConstructOperatingPoint(const Configuration &config,
                          const OperatingPoint::Metrics &res) const;

  bool DoesConfigurationFitCPUCoreSet(const Configuration &config,
                                      const CPUCoreSet &core_set) const;

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
      : OperatingPointTable(platform, std::move(evaluator),
                            OperatingPointTableStage::kStatic, measurement,
                            false) {}

  std::vector<OperatingPoint> GetOperatingPoints() override {
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

  std::optional<OperatingPoint>
  GetOperatingPointToMeasure(const CPUCoreSet &core_set) override {
    return {};
  }

  void Dump() override;

private:
  std::map<std::string, OperatingPoint> _ops;
};

} // namespace tetris

#endif
