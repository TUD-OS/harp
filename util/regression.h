#ifndef __REGRESSION_H__
#define __REGRESSION_H__

#pragma once

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace tetris {

class Regression {
public:
  Regression(int num_input, int num_output, int degree = 2);

  void FitModel(const std::vector<std::vector<double>> &X,
                const std::vector<std::vector<double>> &Y);

  Eigen::MatrixXd GetBeta() const { return _beta; }

  std::vector<std::vector<double>>
  Predict(const std::vector<std::vector<double>> &X) const;

private:
  void ComputeFeatureCombinations(std::vector<int> &current, int start, int n,
                                  int k);

  /**
   * \brief Converts a std::vector<std::vector<double>> to an Eigen::MatrixXd.
   *
   * \param vec A 2D vector of doubles.
   * \return Eigen::MatrixXd containing the data from the input vector.
   */
  Eigen::MatrixXd
  ConvertToMatrixXd(const std::vector<std::vector<double>> &V) const;

  /**
   * \brief Converts an Eigen::MatrixXd to a std::vector<std::vector<double>>.
   *
   * \param matrix The Eigen::MatrixXd to be converted.
   * \return A 2D vector of doubles representing the matrix.
   */
  std::vector<std::vector<double>>
  ConvertFromMatrixXd(const Eigen::MatrixXd &M) const;

  Eigen::MatrixXd ExtendPolynomial(const Eigen::MatrixXd &input) const;

  int _num_input;
  int _num_output;
  int _degree;

  // Input feature combinations for polynomial of higher degrees
  std::vector<std::vector<int>> _combinations;

  Eigen::MatrixXd _beta;
};

} // namespace tetris

#endif
