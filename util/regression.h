#ifndef __REGRESSION_H__
#define __REGRESSION_H__

#pragma once

#include "util/operating_point.h"

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace tetris {

class OperatingPointRegression {
public:
  OperatingPointRegression(const Platform &platform,
                           const std::vector<std::string> &dependent_features,
                           int degree = 2);

  void FitModel(const std::vector<OperatingPoint> &ops);

  Eigen::MatrixXd GetBeta() const { return _beta; }

  std::vector<std::map<std::string, double>>
  Predict(const std::vector<OperatingPoint> &ops) const;

  void PredictAndUpdate(std::vector<OperatingPoint> &ops) const;

private:
  void ComputeFeatureCombinations(std::vector<int> &current, int start, int n,
                                  int k);

  std::vector<double> ExtractOPData(const OperatingPoint &op) const;

  Eigen::MatrixXd ExtendPolynomial(const Eigen::MatrixXd &input) const;

  Eigen::MatrixXd PredictInternal(const std::vector<OperatingPoint> &ops) const;

  const Platform &_platform;
  std::vector<std::string> _dependent_features;
  int _degree;

  Eigen::MatrixXd _beta;

  int _num_input_features;
  std::map<std::string, int> _thread_capacity;
  std::map<std::string, int> _core_usage_start_idxs;

  std::vector<std::vector<int>> _combinations;
};

} // namespace tetris

#endif
