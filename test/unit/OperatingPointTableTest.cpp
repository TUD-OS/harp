#include "unit/PlatformFixtures.h"

#include "util/operating_point_table.h"

using namespace tetris;

class RaptorLakeOperatingPointTableTest : public RaptorLakeTest {
protected:
  OperatingPoint::Configuration
  CreateConfiguration(const CPUThreadSet &threads) {
    auto cores = platform->ToCPUCoreSet(threads);
    auto core_counts = platform->GetCoreCountPerType(cores);
    return OperatingPoint::Configuration{"default", threads, core_counts};
  }
  OperatingPoint CreateOperatingPoint(const CPUThreadSet &threads, double ips,
                                      double power) {
    auto config = CreateConfiguration(threads);
    OperatingPoint::Metrics metrics{ips, power};
    return OperatingPoint{config, metrics};
  }
};

TEST_F(RaptorLakeOperatingPointTableTest, OperatingPointTable) {
  std::shared_ptr<OperatingPointEvaluator> evaluator =
      OperatingPointEvaluatorFactory::Create("balanced");
  ThreadSetOperatingPointTable op_table(*platform, evaluator);

  // clang-format off
  // P-HT=3 P-ST=0 E=11
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26},
      14408, 83.7));
  // P-HT=5 P-ST=0 E=3
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 17, 18},
      11766, 74.2));
  // P-HT=0 P-ST=1 E=6
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 16, 17, 18, 19, 20, 21},
      8568, 46.2));
  // P-HT=5 P-ST=1 E=2
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 17},
      12020, 77.1));
  // P-HT=2 P-ST=2 E=13
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 6, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28},
      15802, 94.6));
  // P-HT=3 P-ST=2 E=7
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 8, 16, 17, 18, 19, 20, 21, 22},
      13112, 81.4));
  // P-HT=4 P-ST=2 E=14
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 16, 17, 18, 19, 20, 21, 22, 23,
       24, 25, 26, 27, 28, 29},
      18428, 118.7));
  // P-HT=3 P-ST=4 E=3
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 17, 18},
      11885, 79.5));
  // P-HT=1 P-ST=5 E=6
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 4, 6, 8, 10, 16, 17, 18, 19, 20, 21},
      11814, 76.5));
  // P-HT=1 P-ST=5 E=12
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 4, 6, 8, 10, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
      15783, 99.3));
  // P-HT=3 P-ST=5 E=12
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0,  1,  2,  3,  4,  5,  6,  8,  10, 12, 14, 16, 17, 18, 19, 20, 21, 22,
       23, 24, 25, 26, 27},
      17911, 120.2));
  // P-HT=1 P-ST=7 E=9
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 1, 2, 4, 6, 8, 10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24},
      15166, 102.8));
  // P-HT=0 P-ST=8 E=2
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 2, 4, 6, 8, 10, 12, 14, 16, 17},
      11460, 83.6));
  // P-HT=0 P-ST=8 E=11
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0, 2, 4, 6, 8, 10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26},
      18137, 122.8));
  // P-HT=0 P-ST=8 E=16
  op_table.AddOperatingPoint(CreateOperatingPoint(
      {0,  2,  4,  6,  8,  10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
       26, 27, 28, 29, 30, 31},
      20589, 125.1));
  // clang-format on

  auto ops_ema = op_table.GetOperatingPoints(false);
  EXPECT_EQ(ops_ema.size(), 15);

  auto ops_approx = op_table.GetOperatingPoints(true);
  EXPECT_EQ(ops_approx.size(), 764);

  int flag = 0;

  for (const auto &op : ops_approx) {
    const auto &utility = op.utility();
    const auto &power = op.power();
    if (op.threads() == CPUThreadSet{0, 1}) {
      flag |= 1 << 0;
      EXPECT_NEAR(utility, 4056, 1);
      EXPECT_NEAR(power, -1.054, 0.001);
    }
    if (op.threads() == CPUThreadSet{0}) {
      flag |= 1 << 1;
      EXPECT_NEAR(utility, 3626, 1);
      EXPECT_NEAR(power, 2.257, 0.001);
    }
    if (op.threads() == CPUThreadSet{16}) {
      flag |= 1 << 2;
      EXPECT_NEAR(utility, 3937, 1);
      EXPECT_NEAR(power, 3.492, 0.001);
    }
    if (op.threads() == CPUThreadSet{0, 1, 2, 3, 4, 16, 17}) {
      flag |= 1 << 3;
      EXPECT_NEAR(utility, 7311, 1);
      EXPECT_NEAR(power, 29.770, 0.001);
    }
  }

  EXPECT_EQ(flag, (1 << 4) - 1);

  auto ops_pareto = op_table.GetParetoFront();
  EXPECT_EQ(ops_pareto.size(), 2);

  // Check updating operating point table
  // P-HT=1 P-ST=0 E=0
  op_table.AddOperatingPointMeasurement(CreateConfiguration({2, 3}),
                                        OperatingPoint::Metrics{4481, 26.2});

  ops_ema = op_table.GetOperatingPoints(false);
  EXPECT_EQ(ops_ema.size(), 16);
  ops_pareto = op_table.GetParetoFront();
  EXPECT_EQ(ops_pareto.size(), 115);

  // P-HT=5 P-ST=1 E=2
  op_table.AddOperatingPointMeasurement(
      CreateConfiguration({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13, 17, 20}),
      OperatingPoint::Metrics{25000, 50});

  ops_ema = op_table.GetOperatingPoints(false);
  EXPECT_EQ(ops_ema.size(), 16);

  int flag2 = 0;
  for (const auto &op : ops_ema) {
    const auto &utility = op.utility();
    const auto &power = op.power();
    if (op.threads() ==
        CPUThreadSet{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 17}) {
      flag2 |= 1 << 0;
      EXPECT_NEAR(utility, 18510, 1);
      EXPECT_NEAR(power, 63.55, 0.001);
    }
    if (op.threads() == CPUThreadSet{0, 1}) {
      flag2 |= 1 << 1;
      EXPECT_NEAR(utility, 4481, 1);
      EXPECT_NEAR(power, 26.2, 0.001);
    }
  }
  EXPECT_EQ(flag2, (1 << 2) - 1);

  ops_approx = op_table.GetOperatingPoints(true);
  EXPECT_EQ(ops_approx.size(), 764);

  int flag3 = 0;

  for (const auto &op : ops_approx) {
    const auto &utility = op.utility();
    const auto &power = op.power();
    if (op.threads() == CPUThreadSet{0, 1}) {
      flag3 |= 1 << 0;
      EXPECT_NEAR(utility, 4481, 1);
      EXPECT_NEAR(power, 26.2, 0.001);
    }
    if (op.threads() == CPUThreadSet{0}) {
      flag3 |= 1 << 1;
      EXPECT_NEAR(utility, 3982, 1);
      EXPECT_NEAR(power, 24.227, 0.001);
    }
    if (op.threads() == CPUThreadSet{16}) {
      flag3 |= 1 << 2;
      EXPECT_NEAR(utility, 4293, 1);
      EXPECT_NEAR(power, 23.778, 0.001);
    }
    if (op.threads() == CPUThreadSet{0, 1, 2}) {
      flag3 |= 1 << 3;
      EXPECT_NEAR(utility, 4702, 1);
      EXPECT_NEAR(power, 30.200, 0.001);
    }
    if (op.threads() == CPUThreadSet{0, 1, 2, 3, 4, 16, 17}) {
      flag3 |= 1 << 4;
      EXPECT_NEAR(utility, 7235, 1);
      EXPECT_NEAR(power, 45.999, 0.001);
    }
  }

  EXPECT_EQ(flag3, (1 << 5) - 1);
}
