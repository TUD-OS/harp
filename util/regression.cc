#include "util/regression.h"

#include "util/platform/platform.h"

namespace tetris {

OperatingPointRegression::OperatingPointRegression(
    const Platform &platform,
    const std::vector<std::string> &dependent_features, int degree)
    : _platform{platform},
      _dependent_features{dependent_features}, _degree{degree} {

  // Calculate num input features, and their correspondence of their indices
  // to the cores
  _thread_capacity = _platform.GetThreadCapacityPerCoreType();
  _num_input_features = 0;
  for (const auto &[key, v] : _thread_capacity) {
    _core_usage_start_idxs[key] = _num_input_features;
    _num_input_features += v;
  }

  // Calculate all feature combinations
  for (int k = 0; k <= _degree; ++k) {
    std::vector<int> current;
    ComputeFeatureCombinations(current, 0, _num_input_features, k);
  }
}

void OperatingPointRegression::FitModel(
    const std::vector<OperatingPoint> &ops) {
  int n = ops.size();

  Eigen::MatrixXd X(n, _num_input_features);
  Eigen::MatrixXd Y(n, _dependent_features.size());

  for (int i = 0; i < n; ++i) {
    auto row = ExtractOPData(ops[i]);
    for (int j = 0; j < _num_input_features; ++j) {
      X(i, j) = row[j];
    }
    for (int j = 0; j < _dependent_features.size(); ++j) {
      Y(i, j) = row[_num_input_features + j];
    }
  }

  auto X_poly = ExtendPolynomial(X);

  _beta = (X_poly.transpose() * X_poly).ldlt().solve(X_poly.transpose() * Y);
}

std::vector<std::map<std::string, double>> OperatingPointRegression::Predict(
    const std::vector<OperatingPoint> &ops) const {
  auto Y = PredictInternal(ops);

  std::vector<std::map<std::string, double>> res;
  for (int i = 0; i < ops.size(); ++i) {
    std::map<std::string, double> r;
    for (int j = 0; j < _dependent_features.size(); ++j) {
      r.emplace(_dependent_features[j], Y(i, j));
    }
    res.push_back(r);
  }
  return res;
}

void OperatingPointRegression::PredictAndUpdate(
    std::vector<OperatingPoint> &ops) const {
  auto Y = PredictInternal(ops);

  for (int i = 0; i < ops.size(); ++i) {
    std::map<std::string, double> r;
    for (int j = 0; j < _dependent_features.size(); ++j) {
      ops[j].characteristics.emplace(_dependent_features[j], Y(i, j));
    }
  }
}

Eigen::MatrixXd OperatingPointRegression::PredictInternal(
    const std::vector<OperatingPoint> &ops) const {
  int n = ops.size();
  Eigen::MatrixXd X(n, _num_input_features);
  for (int i = 0; i < n; ++i) {
    auto row = ExtractOPData(ops[i]);
    for (int j = 0; j < _num_input_features; ++j) {
      X(i, j) = row[j];
    }
  }
  auto X_poly = ExtendPolynomial(X);

  return X_poly * _beta;
}

void OperatingPointRegression::ComputeFeatureCombinations(
    std::vector<int> &current, int start, int n, int k) {
  if (k == 0) {
    _combinations.push_back(current);
    return;
  }

  for (int i = start; i < n; ++i) {
    current.push_back(i);
    ComputeFeatureCombinations(current, i, n, k - 1);
    current.pop_back();
  }
}

std::vector<double>
OperatingPointRegression::ExtractOPData(const OperatingPoint &op) const {
  auto thread_set = op.cpus;
  auto thread_usage = _platform.GetThreadUsagePerCoreType(thread_set);

  std::vector<double> res(_num_input_features + _dependent_features.size(),
                          0.0);

  // Fill input features
  for (const auto &[type, distr] : thread_usage) {
    for (int i = 0; i < distr.size(); ++i) {
      res[_core_usage_start_idxs.at(type) + i] = distr[i];
    }
  }

  // Fill dependent features
  for (int i = 0; i < _dependent_features.size(); ++i) {
    res[_num_input_features + i] = op.characteristic(_dependent_features[i]);
  }

  return res;
}

Eigen::MatrixXd
OperatingPointRegression::ExtendPolynomial(const Eigen::MatrixXd &input) const {
  int num_samples = static_cast<int>(input.rows());
  ; // how to get number of input rows?
  int num_output_features = _combinations.size();
  Eigen::MatrixXd X(num_samples, num_output_features);

  for (int i = 0; i < _combinations.size(); ++i) {
    auto &combo = _combinations[i];
    Eigen::VectorXd feature = Eigen::VectorXd::Ones(num_samples);
    for (int j : combo) {
      feature = feature.array() * input.col(j).array();
    }
    X.col(i) = feature;
  }

  return X;
}
} // namespace tetris
