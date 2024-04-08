#include "unit/PlatformFixtures.h"

#include "util/regression.h"

using namespace tetris;

class OdroidRegressionTest : public OdroidTest {
protected:
  inline static const std::string kIPS = "ips";
  inline static const std::string kPower = "power_w";

  OperatingPoint CreateOperatingPoint(const CPUThreadSet &threads, double ips,
                                      double power) {
    std::map<std::string, double> characteristics{{kIPS, ips}, {kPower, power}};
    auto cores = platform->ToCPUCoreSet(threads);
    auto cores_count = platform->GetCoreCountPerType(cores);
    OperatingPoint op{"default", characteristics, threads, cores_count};
    return op;
  }
};

class RaptorLakeRegressionTest : public RaptorLakeTest {
protected:
  inline static const std::string kIPS = "ips";
  inline static const std::string kPower = "power_w";

  OperatingPoint CreateOperatingPoint(const CPUThreadSet &threads, double ips,
                                      double power) {
    std::map<std::string, double> characteristics{{kIPS, ips}, {kPower, power}};
    auto cores = platform->ToCPUCoreSet(threads);
    auto cores_count = platform->GetCoreCountPerType(cores);
    OperatingPoint op{"default", characteristics, threads, cores_count};
    return op;
  }
};

TEST_F(RaptorLakeRegressionTest, Regression) {
  std::vector<OperatingPoint> ops;
  // clang-format off
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26},
      14408, 83.7));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 17, 18},
      11766, 74.2));
  ops.push_back(CreateOperatingPoint(
      {0, 16, 17, 18, 19, 20, 21},
      8568, 46.2));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 17},
      12020, 77.1));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 6, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28},
      15802, 94.6));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 8, 16, 17, 18, 19, 20, 21, 22},
      13112, 81.4));
  ops.push_back(CreateOperatingPoint(
      {0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 16, 17, 18, 19, 20, 21, 22, 23,
       24, 25, 26, 27, 28, 29},
      18428, 118.7));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 17, 18},
      11885, 79.5));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 4, 6, 8, 10, 16, 17, 18, 19, 20, 21},
      11814, 76.5));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 4, 6, 8, 10, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
      15783, 99.3));
  ops.push_back(CreateOperatingPoint(
      {0,  1,  2,  3,  4,  5,  6,  8,  10, 12, 14, 16, 17, 18, 19, 20, 21, 22,
       23, 24, 25, 26, 27},
      17911, 120.2));
  ops.push_back(CreateOperatingPoint(
      {0, 1, 2, 4, 6, 8, 10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24},
      15166, 102.8));
  ops.push_back(CreateOperatingPoint(
      {0, 2, 4, 6, 8, 10, 12, 14, 16, 17},
      11460, 83.6));
  ops.push_back(CreateOperatingPoint(
      {0, 2, 4, 6, 8, 10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26},
      18137, 122.8));
  ops.push_back(CreateOperatingPoint(
      {0,  2,  4,  6,  8,  10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
       26, 27, 28, 29, 30, 31},
      20589, 125.1));
  // clang-format on

  OperatingPointRegression regression(*platform, {kIPS, kPower});

  regression.FitModel(ops);

  auto beta = regression.GetBeta();

  EXPECT_EQ(beta.cols(), 2);
  EXPECT_EQ(beta.rows(), 10);

  std::vector<OperatingPoint> test_ops;
  // clang-format off
  test_ops.push_back(CreateOperatingPoint({0, 1}, 0, 0));
  test_ops.push_back(CreateOperatingPoint({0}, 0, 0));
  test_ops.push_back(CreateOperatingPoint({16}, 0, 0));
  test_ops.push_back(CreateOperatingPoint(
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
         27,28,29,30,31}, 0, 0));
  // clang-format on

  auto res = regression.Predict(test_ops);
  // std::cout << res_map;
  EXPECT_EQ(res.size(), 4);

  for (int i = 0; i < res.size(); ++i) {
    auto &r = res.at(i);
    auto &ref_c = test_ops.at(i).characteristics;
    switch (i) {
    case 0:
      EXPECT_EQ(ref_c.at(kIPS), 0.0);
      EXPECT_EQ(ref_c.at(kPower), 0.0);
      EXPECT_NEAR(r.at(kIPS), 4056, 1);
      EXPECT_NEAR(r.at(kPower), -1.054, 0.001);
      break;
    case 1:
      EXPECT_EQ(ref_c.at(kIPS), 0.0);
      EXPECT_EQ(ref_c.at(kPower), 0.0);
      EXPECT_NEAR(r.at(kIPS), 3626, 1);
      EXPECT_NEAR(r.at(kPower), 2.257, 0.001);
      break;
    case 2:
      EXPECT_EQ(ref_c.at(kIPS), 0.0);
      EXPECT_EQ(ref_c.at(kPower), 0.0);
      EXPECT_NEAR(r.at(kIPS), 3937, 1);
      EXPECT_NEAR(r.at(kPower), 3.492, 0.001);
      break;
    case 3:
      EXPECT_EQ(ref_c.at(kIPS), 0.0);
      EXPECT_EQ(ref_c.at(kPower), 0.0);
      EXPECT_NEAR(r.at(kIPS), 24926, 1);
      EXPECT_NEAR(r.at(kPower), 199.569, 0.001);
      break;
    default:
      FAIL() << "Unexpected index";
    }
  }
}
