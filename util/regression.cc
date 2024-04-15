#include "util/regression.h"

namespace tetris {

Regression::Regression(int num_input, int num_output, int degree)
    : _num_input(num_input), _num_output(num_output), _degree(degree) {

  // Calculate all feature combinations
  for (int k = 0; k <= degree; ++k) {
    std::vector<int> current;
    ComputeFeatureCombinations(current, 0, num_input, k);
  }
}

void Regression::FitModel(const Eigen::MatrixXd &X, const Eigen::MatrixXd &Y) {
  if (X.rows() != Y.rows()) {
    std::string msg =
        "Number of rows in input and output matrices mismatch. X.rows() = " +
        std::to_string(X.size()) + ", Y.rows() = " + std::to_string(Y.size()) +
        ".";
    throw std::runtime_error(msg);
  }

  auto X_ext = ExtendPolynomial(X);

  _beta = (X_ext.transpose() * X_ext).ldlt().solve(X_ext.transpose() * Y);
}

Eigen::MatrixXd Regression::Predict(const Eigen::MatrixXd &X) const {
  auto X_ext = ExtendPolynomial(X);
  return X_ext * _beta;
}

std::vector<std::vector<double>>
Regression::ConvertFromMatrixXd(const Eigen::MatrixXd &M) const {
  // Retrieve dimensions of the matrix
  std::size_t rows = M.rows();
  std::size_t cols = M.cols();

  // Initialize the vector of vectors with the dimensions of the matrix
  std::vector<std::vector<double>> V(rows, std::vector<double>(cols));

  // Copy data from the Eigen matrix to the 2D vector
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t j = 0; j < cols; ++j) {
      V[i][j] = M(i, j);
    }
  }

  return V;
}

void Regression::ComputeFeatureCombinations(std::vector<int> &current,
                                            int start, int n, int k) {
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

Eigen::MatrixXd
Regression::ExtendPolynomial(const Eigen::MatrixXd &input) const {
  int num_samples = static_cast<int>(input.rows());
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
