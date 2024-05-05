#include "util/operating_point_table.h"

#include "util/platform/platform.h"

namespace tetris {

std::ostream &operator<<(std::ostream &os, OperatingPointTableStage stage) {
  switch (stage) {
  case OperatingPointTableStage::kStatic:
    os << "Static";
    break;
  case OperatingPointTableStage::kInitial:
    os << "Initial";
    break;
  case OperatingPointTableStage::kExploration:
    os << "Exploration";
    break;
  case OperatingPointTableStage::kMature:
    os << "Mature";
    break;
  default:
    os << "Unknown Stage";
    break;
  }
  return os;
}

OperatingPointTable::OperatingPointTable(
    const Platform &platform,
    std::shared_ptr<OperatingPointEvaluator> evaluator,
    OperatingPointTableStage stage, bool measurement, bool approximation)
    : _platform(platform), _stage(stage),
      _measurement(measurement), _pareto_filter{nullptr} {
  SetOperatingPointEvaluator(std::move(evaluator));
  _params = platform.GetOperatingPointTableParams();
}

void OperatingPointTable::AddOperatingPoint(
    const ClientMessage::OperatingPointsInfo::OPData &op) {
  CPUThreadSet threads;

  for (int i = 0; i < op.cpu_ids_size(); ++i) {
    threads.Set(op.cpu_ids(i));
  }
  auto cores_count =
      _platform.GetCoreCountPerType(_platform.ToCPUCoreSet(threads));

  OperatingPoint::Configuration config{op.identifier(), threads, cores_count};
  OperatingPoint::Metrics metrics{op.utility(), op.power()};
  AddOperatingPoint(config, metrics);
}

void OperatingPointTable::SetOperatingPointEvaluator(
    std::shared_ptr<OperatingPointEvaluator> evaluator) {
  if (_pareto_filter && _evaluator == evaluator) {
    // This is the same evaluator, do nothing
    return;
  }

  // Store evaluator
  _evaluator = std::move(evaluator);

  // Intialize _pareto_filter
  std::vector<ObjectiveBetterFunc> objectives;
  auto types = _platform.GetCPUTypes();
  for (const auto &t : types) {
    objectives.push_back([t](const OperatingPoint &a, const OperatingPoint &b) {
      return a.core_counts().at(t) < b.core_counts().at(t);
    });
  }

  if (_evaluator) {
    objectives.push_back(
        [this](const OperatingPoint &a, const OperatingPoint &b) {
          return this->_evaluator->Evaluate(a) < this->_evaluator->Evaluate(b);
        });
  }
  if (_pareto_filter) {
    _pareto_filter->Reset(objectives);
  } else {
    _pareto_filter =
        std::make_unique<ParetoFrontFilter<OperatingPoint>>(objectives);
  }

  // Mark to regenerate Pareto Front
  _update_pareto = true;
}

std::vector<OperatingPoint> OperatingPointTable::GetParetoFront() {
  if (_update_pareto) {
    LOGGER->debug("Updating the Pareto front of the operating points.\n");
    auto ops = GetOperatingPoints();
    _pareto = _pareto_filter->Filter(ops);
    _update_pareto = false;
  } else {
    LOGGER->debug("Returning the previously filtered Pareto front of the "
                  "operating points.\n");
  }
  return _pareto;
}

ThreadSetOperatingPointTable::ThreadSetOperatingPointTable(
    const Platform &platform,
    std::shared_ptr<OperatingPointEvaluator> evaluator, bool measurement,
    bool approximation, double ema_alpha)
    : OperatingPointTable(platform, std::move(evaluator),
                          OperatingPointTableStage::kInitial, measurement,
                          approximation),
      _ema_alpha(ema_alpha), _update_approximated(false) {

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

  std::vector<std::string> features{"utility", "power"};
  _regression = std::make_unique<Regression>(_num_core_thread_levels, 2, 2);
}

std::vector<OperatingPoint> ThreadSetOperatingPointTable::GetOperatingPoints() {
  std::vector<OperatingPoint> res;

  GenerateApproximatedOperatingPoints();

  for (const auto &config : _all_configurations) {
    const auto &metrics = GetOperatingPointMetrics(config);
    auto op = ConstructOperatingPoint(config, metrics);
    res.push_back(op);
  }

  return res;
}

void ThreadSetOperatingPointTable::AddOperatingPoint(const OperatingPoint &op) {
  AddOperatingPoint(op.config, op.metrics);
}

void ThreadSetOperatingPointTable::AddOperatingPoint(
    const OperatingPoint::Configuration &op_config,
    const OperatingPoint::Metrics &metrics) {
  auto config = GetConfiguration(op_config);
  if (_ops.contains(config)) {
    LOGGER->warning("The operating point with the same configuration (%s) "
                    "was already added. Rewriting the old operating point.\n",
                    GetConfigurationString(config).c_str());
  }

  _ops.emplace(config, metrics);
  _sample_counts.emplace(config, 1);
  _update_approximated = true;
  _update_pareto = true;
  EvaluateStage();
}

void ThreadSetOperatingPointTable::AddOperatingPointMeasurement(
    const OperatingPoint::Configuration &op_config,
    const OperatingPoint::Metrics &result) {
  if (!EnabledMeasurement()) {
    throw std::runtime_error(
        "OperatingPointTable does not support adding measured data");
  }

  auto config = GetConfiguration(op_config);

  _update_approximated = true;
  _update_pareto = true;

  if (!_ops.contains(config)) {
    _ops.emplace(config, result);
    _sample_counts.emplace(config, 1);
    EvaluateStage();
    return;
  }
  auto &opres = _ops.at(config);
  auto &sample_count = _sample_counts.at(config);
  double alpha_eff = std::max(_ema_alpha, 1.0 / (sample_count + 1));
  opres.utility = alpha_eff * result.utility + (1 - alpha_eff) * opres.utility;
  opres.power = alpha_eff * result.power + (1 - alpha_eff) * opres.power;
  sample_count += 1;

  EvaluateStage();
}

double CalculateNormalizedError(double measured, double approximated) {
  return std::abs(measured - approximated) / std::max(measured, approximated);
}

double CalculateUtilityPowerError(double utility_current, double utility_approx,
                                  double power_current, double power_approx) {
  double utility_error =
      CalculateNormalizedError(utility_current, utility_approx);
  double power_error = CalculateNormalizedError(power_current, power_approx);

  // Compute geometric mean of the two errors
  return std::sqrt(utility_error * power_error);
}

std::optional<OperatingPoint>
ThreadSetOperatingPointTable::GetOperatingPointToMeasure(
    const CPUCoreSet &core_set) {
  if (_stage == OperatingPointTableStage::kInitial) {
    return GetOperatingPointToMeasureInitial(core_set);
  }
  if (_stage == OperatingPointTableStage::kExploration) {
    return GetOperatingPointToMeasureExploration(core_set);
  }
  return {};
}

std::optional<OperatingPoint>
ThreadSetOperatingPointTable::GetOperatingPointToMeasureInitial(
    const CPUCoreSet &core_set) {
  int res_distance = 0;
  Configuration res_config;
  for (const auto &config : _all_configurations) {
    if (_ops.contains(config)) {
      continue;
    }
    if (!DoesConfigurationFitCPUCoreSet(config, core_set)) {
      continue;
    }
    int min_distance = INT_MAX;
    for (const auto &[ref_config, _] : _ops) {
      int cur_distance = 0;
      for (int i = 0; i < _num_core_thread_levels; ++i) {
        cur_distance += abs(config[i] - ref_config[i]);
      }
      if (cur_distance < min_distance) {
        min_distance = cur_distance;
      }
    }
    if (min_distance > res_distance) {
      res_distance = min_distance;
      res_config = config;
    }
  }

  if (res_config.size() > 0) {
    return ConstructOperatingPoint(res_config, {0, 0});
  }

  return {};
}

std::optional<OperatingPoint>
ThreadSetOperatingPointTable::GetOperatingPointToMeasureExploration(
    const CPUCoreSet &core_set) {
  GenerateApproximatedOperatingPoints();

  // 1. Collect reliable operating points. If not enough (kExplorationPoints),
  // collect all
  std::vector<Configuration> X_train;
  std::vector<std::vector<double>> Y_train;

  // Add zero point
  // X_train.push_back(Configuration(_num_core_thread_levels));
  // Y_train.push_back({0, 0});

  for (const auto &[config, res] : _ops) {
    if (_sample_counts.at(config) >= _params.at("reliable_measurements")) {
      X_train.push_back(config);
      Y_train.push_back({res.utility, res.power});
    }
  }
  int config_size = _all_configurations[0].size();
  if (X_train.size() < _params.at("initial_points")) {
    for (const auto &[config, res] : _ops) {
      if (_sample_counts.at(config) < _params.at("reliable_measurements")) {
        X_train.push_back(config);
        Y_train.push_back({res.utility, res.power});
      }
    }
  }

  // Train Model
  _regression->FitModel(X_train, Y_train);

  // 2. Get approximation of unreliable and unmeasured operating points that
  // fits core set
  std::vector<Configuration> X_test;
  for (const auto &config : _all_configurations) {
    if (!DoesConfigurationFitCPUCoreSet(config, core_set)) {
      continue;
    }
    if (!_ops.contains(config) ||
        _sample_counts.at(config) < _params.at("reliable_measurements")) {
      X_test.push_back(config);
    }
  }

  if (X_test.size() == 0) {
    return {};
  }

  auto Y_test = _regression->Predict(X_test);

  // 3. Calculate Error and select one with the largest error
  double max_error = 0;
  Configuration res_config;
  for (int i = 0; i < X_test.size(); ++i) {
    const auto &config = X_test[i];
    double utility_test = Y_test[i][0];
    double power_test = Y_test[i][1];
    double utility_current, power_current;
    if (_ops.contains(config)) {
      utility_current = _ops.at(config).utility;
      power_current = _ops.at(config).power;
    } else {
      utility_current = _approx_ops.at(config).utility;
      power_current = _approx_ops.at(config).power;
    }
    double error = CalculateUtilityPowerError(utility_current, utility_test,
                                              power_current, power_test);
    if (error > max_error) {
      res_config = config;
      max_error = error;
    }
  }

  if (max_error > 0) {
    auto metrics = GetOperatingPointMetrics(res_config);
    return ConstructOperatingPoint(res_config, metrics);
  } else {
    // It is possible all points got 0 error, select any unmeasured point
    for (auto &[config, metrics] : _approx_ops) {
      if (_ops.contains(config) &&
          _sample_counts.at(config) >= _params.at("reliable_measurements")) {
        continue;
      }
      if (DoesConfigurationFitCPUCoreSet(config, core_set)) {
        return ConstructOperatingPoint(config, metrics);
      }
    }
    return {};
  }
}

void ThreadSetOperatingPointTable::Dump() {
  std::stringstream ss;
  ss << _stage;
  auto stage_str = ss.str();
  LOGGER->debug("Operating Points (stage %s):\n", stage_str.c_str());
  for (const auto &config : _all_configurations) {
    if (_ops.contains(config)) {
      const auto &metric = GetOperatingPointMetrics(config);
      LOGGER->debug(
          " - Name: %s, Count: %d, Utility (eff.): %lf, Power (eff.): %lf\n",
          GetConfigurationString(config).c_str(), _sample_counts.at(config),
          metric.utility, metric.power);
    } else {
      const auto &metric = _approx_ops.at(config);
      LOGGER->debug(" - Name: %s, Approximated, Utility: %lf, Power: %lf\n",
                    GetConfigurationString(config).c_str(), metric.utility,
                    metric.power);
    }
  }
}

ThreadSetOperatingPointTable::Configuration
ThreadSetOperatingPointTable::GetConfiguration(
    const OperatingPoint::Configuration &op_config) const {
  auto &threads = op_config.threads;
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

std::string ThreadSetOperatingPointTable::GetConfigurationString(
    const Configuration &config) const {
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

void ThreadSetOperatingPointTable::GenerateAllConfigurationsLevel(
    std::vector<Configuration> &all, Configuration &current) const {
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

void ThreadSetOperatingPointTable::EvaluateStage() {
  int num_measured = _ops.size();
  int num_reliable = 0;

  if (num_measured < _params.at("initial_points")) {
    _stage = OperatingPointTableStage::kInitial;
    return;
  }

  for (const auto &[_, count] : _sample_counts) {
    if (count >= _params.at("reliable_measurements")) {
      num_reliable++;
    }
  }

  if (num_reliable < _params.at("exploration_points")) {
    _stage = OperatingPointTableStage::kExploration;
  } else {
    _stage = OperatingPointTableStage::kMature;
  }
}

const OperatingPoint::Metrics &
ThreadSetOperatingPointTable::GetOperatingPointMetrics(
    const Configuration &config) const {
  switch (_stage) {
  case OperatingPointTableStage::kInitial:
  case OperatingPointTableStage::kExploration:
    if (_ops.contains(config)) {
      return _ops.at(config);
    } else {
      return _approx_ops.at(config);
    }
  case OperatingPointTableStage::kMature:
    if (_ops.contains(config) &&
        _sample_counts.at(config) >= _params.at("reliable_measurements")) {
      return _ops.at(config);
    } else {
      return _approx_ops.at(config);
    }
  default:
    throw std::runtime_error("Unknown OperatingPointTableStage");
  }
}

CPUThreadSet ThreadSetOperatingPointTable::ConstructCPUThreadSet(
    const Configuration &config) const {
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

OperatingPoint ThreadSetOperatingPointTable::ConstructOperatingPoint(
    const Configuration &config, const OperatingPoint::Metrics &res) const {
  auto name = GetConfigurationString(config);
  auto thread_set = ConstructCPUThreadSet(config);
  auto core_set = _platform.ToCPUCoreSet(thread_set);
  auto core_counts = _platform.GetCoreCountPerType(core_set);
  OperatingPoint::Configuration op_config{name, thread_set, core_counts};
  OperatingPoint::Metrics op_metrics = res;

  return OperatingPoint(op_config, op_metrics);
}

bool ThreadSetOperatingPointTable::DoesConfigurationFitCPUCoreSet(
    const Configuration &config, const CPUCoreSet &core_set) const {
  auto config_cores = _platform.ToCPUCoreSet(ConstructCPUThreadSet(config));
  auto config_counts = _platform.GetCoreCountPerType(config_cores);
  auto ref_counts = _platform.GetCoreCountPerType(core_set);

  for (const auto &[type, count] : config_counts) {
    if (count > ref_counts.at(type))
      return false;
  }

  return true;
}

void ThreadSetOperatingPointTable::GenerateApproximatedOperatingPoints() {
  if (!_update_approximated) {
    return;
  }

  // Collect operating points for training the model
  std::vector<Configuration> X_train;
  std::vector<std::vector<double>> Y_train;

  // Add a zero point
  X_train.push_back(Configuration(_num_core_thread_levels));
  Y_train.push_back({0, 0});

  if (_stage == OperatingPointTableStage::kInitial ||
      _stage == OperatingPointTableStage::kExploration) {
    for (const auto &[config, res] : _ops) {
      X_train.push_back(config);
      Y_train.push_back({res.utility, res.power});
    }
  } else if (_stage == OperatingPointTableStage::kMature) {
    for (const auto &[config, res] : _ops) {
      if (_sample_counts.at(config) >= _params.at("reliable_measurements")) {
        X_train.push_back(config);
        Y_train.push_back({res.utility, res.power});
      }
    }
  } else {
    std::runtime_error("Unknown OperatingPointTableStage");
  }

  // Train Model
  _regression->FitModel(X_train, Y_train);

  // Approximate points and store results
  auto Y_test = _regression->Predict(_all_configurations);
  _approx_ops.clear();
  for (int i = 0; i < _all_configurations.size(); ++i) {
    OperatingPoint::Metrics res{Y_test[i][0], Y_test[i][1]};
    _approx_ops.emplace(_all_configurations[i], res);
  }

  _update_approximated = false;
}

void CustomOperatingPointTable::Dump() {
  auto ops = GetOperatingPoints();
  LOGGER->debug("Operating Points:\n");
  for (const auto &op : ops) {
    LOGGER->debug(" - %s\n", op.ToString().c_str());
  }
}

} // namespace tetris
