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
   *        output data after converting them into Eigen matrix format.
   *
   * \tparam T1 The data type of the elements in the input dataset X.
   * \tparam T2 The data type of the elements in the output dataset Y.
   *
   * \param X The input data, where each inner vector represents a single data
   *          point. Each data point can consist of one or more features.
   * \param Y The output data corresponding to the input data points. Each
   * output data point can consist of one or more outcome variables.
   *
   * \note The input and output vectors X and Y must be of compatible sizes,
   * where each data point in X has a corresponding output in Y.
   */
  template <typename T1, typename T2>
  void FitModel(const std::vector<std::vector<T1>> &X,
                const std::vector<std::vector<T2>> &Y) {
    Eigen::MatrixXd Xm = ConvertToMatrixXd(X);
    Eigen::MatrixXd Ym = ConvertToMatrixXd(Y);
    FitModel(Xm, Ym);
  }

  /**
   * \brief Fits the polynomial regression model using Eigen::MatrixXd inputs
   * for both predictors (X) and responses (Y).
   *
   * \param X An Eigen::MatrixXd where each row represents a single data point
   *          and each column corresponds to a feature.
   * \param Y An Eigen::MatrixXd where each row corresponds to the output data
   * point and each column represents an outcome variable. The number of rows in
   * Y must match the number of rows in X.
   *
   * \note This method assumes that the matrices X and Y are already
   * appropriately preprocessed and that their sizes are compatible for
   * regression analysis.
   */
  void FitModel(const Eigen::MatrixXd &X, const Eigen::MatrixXd &Y);

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
  template <typename T>
  std::vector<std::vector<double>>
  Predict(const std::vector<std::vector<T>> &X) const {
    Eigen::MatrixXd Xm = ConvertToMatrixXd(X);
    auto Y = Predict(Xm);
    return ConvertFromMatrixXd(Y);
  }

  Eigen::MatrixXd Predict(const Eigen::MatrixXd &X) const;

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
   * \brief Converts a std::vector<std::vector<T>> to an Eigen::MatrixXd.
   *
   * \param vec A 2D vector of doubles.
   * \return Eigen::MatrixXd containing the data from the input vector.
   */
  template <typename T>
  Eigen::MatrixXd ConvertToMatrixXd(const std::vector<std::vector<T>> &V) const;

  /**
   * \brief Converts an Eigen::MatrixXd to a std::vector<std::vector<double>>.
   *
   * \tparam T The data type of the elements in the input 2D vector. Must be a
   * type that is convertible to double. \param V A 2D vector of elements of
   * type T. \return Eigen::MatrixXd containing the data from the input vector,
   * with each element converted to double.
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

template <typename T>
Eigen::MatrixXd
Regression::ConvertToMatrixXd(const std::vector<std::vector<T>> &V) const {
  if (V.empty() || V[0].empty()) {
    return Eigen::MatrixXd(); // Return an empty matrix if input is empty
  }

  // Determine the size of the matrix
  std::size_t rows = V.size();
  std::size_t cols = V[0].size();

  // Initialize an Eigen::MatrixXd with the dimensions of the vector of vectors
  Eigen::MatrixXd M(rows, cols);

  // Copy data from the 2D vector to the Eigen matrix
  for (std::size_t i = 0; i < rows; ++i) {
    if (V[i].size() != cols) {
      throw std::runtime_error("All rows must have the same number of columns");
    }
    for (std::size_t j = 0; j < cols; ++j) {
      M(i, j) = static_cast<double>(V[i][j]);
    }
  }

  return M;
}

} // namespace tetris

#endif
