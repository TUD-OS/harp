#include "util/operating_point_table.h"

namespace tetris {

void OperatingPointTable::SetValueObjective(ObjectiveValueFunc new_objective) {
  // Intialize _pareto_filter
  std::vector<ObjectiveBetterFunc> objectives;
  auto types = _platform.GetCPUTypes();
  for (const auto &t : types) {
    objectives.push_back([t](const OperatingPoint &a, const OperatingPoint &b) {
      return a.cores_count.at(t) < b.cores_count.at(t);
    });
  }

  if (new_objective) {
    objectives.push_back(
        [new_objective](const OperatingPoint &a, const OperatingPoint &b) {
          return new_objective(a) < new_objective(b);
        });
  }
  _pareto_filter->Reset(objectives);

  // Mark to regenerate Pareto Front
  _update_pareto = true;
}

std::vector<OperatingPoint> OperatingPointTable::GetParetoFront() {
  if (_update_pareto) {
    auto ops = GetOperatingPoints(EnabledApproximation());
    _pareto = _pareto_filter->Filter(ops);
    _update_pareto = false;
  }
  return _pareto;
}

ThreadSetOperatingPointTable::ThreadSetOperatingPointTable(
    const Platform &platform, ObjectiveValueFunc value_objective,
    bool measurement, bool approximation, double ema_alpha)
    : OperatingPointTable(platform, value_objective, measurement,
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

  if (EnabledApproximation()) {
    std::vector<std::string> features{"utility", "power"};
    _regression = std::make_unique<Regression>(_num_core_thread_levels, 2, 2);
  }
}

std::vector<OperatingPoint>
ThreadSetOperatingPointTable::GetOperatingPoints(bool approximated) {
  std::vector<OperatingPoint> res;

  if (approximated && !EnabledApproximation()) {
    LOGGER->warning("Cannot return approximated points since the "
                    "approximation was not enabled\n");
    approximated = false;
  }

  if (approximated) {
    GenerateApproximatedOperatingPoints();
  }

  for (const auto &config : _all_configurations) {
    if (_ops.count(config) > 0) {
      auto &opres = _ops.at(config);
      auto op = ConstructOperatingPoint(config, opres);
      res.push_back(op);
    } else {
      if (approximated) {
        auto &opres = _approx_ops.at(config);
        auto op = ConstructOperatingPoint(config, opres);
        res.push_back(op);
      }
    }
  }

  return res;
}

void ThreadSetOperatingPointTable::AddOperatingPoint(const OperatingPoint &op) {
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
  _update_approximated = true;
  _update_pareto = true;
}

void ThreadSetOperatingPointTable::AddOperatingPointMeasurement(
    const OperatingPoint &op, const OperatingPointResult &result) {
  if (!EnabledMeasurement()) {
    throw std::runtime_error(
        "OperatingPointTable does not support adding measured data");
  }

  auto config = GetConfiguration(op);

  _update_approximated = true;
  _update_pareto = true;

  if (_ops.count(config) == 0) {
    _ops.emplace(config, result);
    _sample_counts.emplace(config, 1);
    return;
  }
  auto &opres = _ops.at(config);
  auto &sample_count = _sample_counts.at(config);
  double alpha_eff = std::max(_ema_alpha, 1.0 / (sample_count + 1));
  opres.utility = alpha_eff * result.utility + (1 - alpha_eff) * opres.utility;
  opres.power = alpha_eff * result.power + (1 - alpha_eff) * opres.power;
  sample_count += 1;
}

ThreadSetOperatingPointTable::Configuration
ThreadSetOperatingPointTable::GetConfiguration(const OperatingPoint &op) const {
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
    const Configuration &config, const OperatingPointResult &res) const {
  auto name = GetConfigurationString(config);
  std::map<std::string, double> characteristics{{"utility", res.utility},
                                                {"power", res.power}};
  auto thread_set = ConstructCPUThreadSet(config);
  auto core_set = _platform.ToCPUCoreSet(thread_set);
  auto core_count = _platform.GetCoreCountPerType(core_set);

  return OperatingPoint(name, characteristics, thread_set, core_count);
}

void ThreadSetOperatingPointTable::GenerateApproximatedOperatingPoints() {
  if (!_update_approximated) {
    return;
  }

  // Collect operating points for training the model
  std::vector<Configuration> X_train;
  std::vector<std::vector<double>> Y_train;
  for (const auto &[config, res] : _ops) {
    X_train.push_back(config);
    Y_train.push_back({res.utility, res.power});
  }

  // Train Model
  _regression->FitModel(X_train, Y_train);

  // Collect configs to approximate
  std::vector<Configuration> X_test;
  for (const auto &config : _all_configurations) {
    if (!_ops.contains(config)) {
      X_test.push_back(config);
    }
  }

  // Approximate points and store results
  auto Y_test = _regression->Predict(X_test);
  _approx_ops.clear();
  for (int i = 0; i < X_test.size(); ++i) {
    OperatingPointResult res{Y_test[i][0], Y_test[i][1]};
    _approx_ops.emplace(X_test[i], res);
  }

  _update_approximated = false;
}
} // namespace tetris
