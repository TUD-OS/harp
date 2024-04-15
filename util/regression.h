#ifndef __REGRESSION_H__
#define __REGRESSION_H__

#pragma once

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace tetris {

/**
 * \class Regression
 * \brief A regression model class designed to fit a polynomial regression model
 * to provided data.
 *
 * This class supports multiple input and output variables and can be configured
 * for different degrees of polynomial complexity.
 */
class Regression {
public:
  /**
   * \brief Constructs a Regression object.
   *
   * \param num_input The number of input features.
   * \param num_output The number of output features.
   * \param degree The degree of the polynomial regression (default is 2).
   */
  Regression(int num_input, int num_output, int degree = 2);

  /**
   * \brief Fits the polynomial regression model to the provided input and
   * output data.
   *
   * \param X The input data, each inner vector represents a single data point.
   * \param Y The output data corresponding to the input data points.
   */
  void FitModel(const std::vector<std::vector<double>> &X,
                const std::vector<std::vector<double>> &Y);

  /**
   * \brief Returns the coefficient matrix (beta) of the fitted model.
   *
   * \return Eigen::MatrixXd representing the model coefficients.
   */
  Eigen::MatrixXd GetBeta() const { return _beta; }

  /**
   * \brief Predicts the output for the given input data based on the fitted
   * model.
   *
   * \param X The input data to predict the output for.
   * \return A 2D vector containing the predicted outputs for each input vector.
   */
  std::vector<std::vector<double>>
  Predict(const std::vector<std::vector<double>> &X) const;

private:
  /**
   * \brief Computes all possible combinations of feature indices for polynomial
   * extension.
   *
   * \param current The current combination (used internally for recursion).
   * \param start The starting index for combination generation.
   * \param n Total number of features.
   * \param k The current degree of combination.
   */
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

  /**
   * \brief Extends the input data matrix to include polynomial terms up to the
   * specified degree.
   *
   * \param input The original input matrix.
   * \return An extended input matrix including polynomial terms.
   */
  Eigen::MatrixXd ExtendPolynomial(const Eigen::MatrixXd &input) const;

  int _num_input;  // Number of input features
  int _num_output; // Number of output variables
  int _degree;     // Degree of the polynomial regression

  // Stores combinations of input feature indices for polynomial terms
  std::vector<std::vector<int>> _combinations;

  Eigen::MatrixXd _beta; // Coefficient matrix of the regression model
};

} // namespace tetris

#endif
