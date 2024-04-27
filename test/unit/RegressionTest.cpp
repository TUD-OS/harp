#include <gtest/gtest.h>

#include "util/regression.h"

using namespace tetris;

class RegressionTest : public ::testing::Test {
protected:
  std::unique_ptr<Regression> regression;

  void SetUp() override { regression = std::make_unique<Regression>(3, 2, 2); }
};

TEST_F(RegressionTest, Regression) {
  std::vector<std::vector<double>> X;
  std::vector<std::vector<double>> Y;

  // clang-format off
  X.push_back({3, 0, 11});  Y.push_back({14408, 83.7});
  X.push_back({5, 0, 3});   Y.push_back({11766, 74.2});
  X.push_back({0, 1, 6});   Y.push_back({8568, 46.2});
  X.push_back({5, 1, 2});   Y.push_back({12020, 77.1});
  X.push_back({2, 2, 13});  Y.push_back({15802, 94.6});
  X.push_back({3, 2, 7});   Y.push_back({13112, 81.4});
  X.push_back({4, 2, 14});  Y.push_back({18428, 118.7});
  X.push_back({3, 4, 3});   Y.push_back({11885, 79.5});
  X.push_back({1, 5, 6});   Y.push_back({11814, 76.5});
  X.push_back({1, 5, 12});  Y.push_back({15783, 99.3});
  X.push_back({3, 5, 12});  Y.push_back({17911, 120.2});
  X.push_back({1, 7, 9});   Y.push_back({15166, 102.8});
  X.push_back({0, 8, 2});   Y.push_back({11460, 83.6});
  X.push_back({0, 8, 11});  Y.push_back({18137, 122.8});
  X.push_back({0, 8, 16});  Y.push_back({20589, 125.1});
  // clang-format on

  regression->FitModel(X, Y);

  auto beta = regression->GetBeta();

  EXPECT_EQ(beta.cols(), 2);
  EXPECT_EQ(beta.rows(), 10);

  std::vector<std::vector<double>> X_test;
  X_test.push_back({1, 0, 0});
  X_test.push_back({0, 1, 0});
  X_test.push_back({0, 0, 1});
  X_test.push_back({8, 0, 16});

  auto res = regression->Predict(X_test);
  EXPECT_EQ(res.size(), 4);

  for (int i = 0; i < res.size(); ++i) {
    auto &item = res.at(i);
    switch (i) {
    case 0:
      EXPECT_NEAR(item.at(0), 4056, 1);
      EXPECT_NEAR(item.at(1), -1.054, 0.001);
      break;
    case 1:
      EXPECT_NEAR(item.at(0), 3626, 1);
      EXPECT_NEAR(item.at(1), 2.257, 0.001);
      break;
    case 2:
      EXPECT_NEAR(item.at(0), 3937, 1);
      EXPECT_NEAR(item.at(1), 3.492, 0.001);
      break;
    case 3:
      EXPECT_NEAR(item.at(0), 24926, 1);
      EXPECT_NEAR(item.at(1), 199.569, 0.001);
      break;
    default:
      ADD_FAILURE() << "Unexpected index";
    }
  }
}
