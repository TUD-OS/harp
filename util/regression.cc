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

void Regression::FitModel(const std::vector<std::vector<double>> &X,
                          const std::vector<std::vector<double>> &Y) {
  if (X.size() != Y.size()) {
    std::string msg =
        "Sizes of input and output training tables mismatch. X.size() = " +
        std::to_string(X.size()) + ", Y.size() = " + std::to_string(Y.size()) +
        ".";
    throw std::runtime_error(msg);
  }

  Eigen::MatrixXd Xm = ConvertToMatrixXd(X);
  Eigen::MatrixXd Ym = ConvertToMatrixXd(Y);

  auto Xm_ext = ExtendPolynomial(Xm);

  _beta = (Xm_ext.transpose() * Xm_ext).ldlt().solve(Xm_ext.transpose() * Ym);
}

std::vector<std::vector<double>>
Regression::Predict(const std::vector<std::vector<double>> &X) const {
  Eigen::MatrixXd Xm = ConvertToMatrixXd(X);
  auto Xm_ext = ExtendPolynomial(Xm);
  auto Y = Xm_ext * _beta;

  return ConvertFromMatrixXd(Y);
}

Eigen::MatrixXd
Regression::ConvertToMatrixXd(const std::vector<std::vector<double>> &V) const {
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
      M(i, j) = V[i][j];
    }
  }

  return M;
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
