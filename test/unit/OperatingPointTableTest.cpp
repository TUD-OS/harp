#include "unit/PlatformFixtures.h"

#include "util/operating_point_table.h"

using namespace tetris;

inline const std::string kIPS = "utility";
inline const std::string kPower = "power";

class OdroidOperatingPointTableTest : public OdroidTest {
protected:
  OperatingPoint CreateOperatingPoint(const CPUThreadSet &threads, double ips,
                                      double power) {
    std::map<std::string, double> characteristics{{kIPS, ips}, {kPower, power}};
    auto cores = platform->ToCPUCoreSet(threads);
    auto cores_count = platform->GetCoreCountPerType(cores);
    OperatingPoint op{"default", characteristics, threads, cores_count};
    return op;
  }
};

class RaptorLakeOperatingPointTableTest : public RaptorLakeTest {
protected:
  OperatingPoint CreateOperatingPoint(const CPUThreadSet &threads, double ips,
                                      double power) {
    std::map<std::string, double> characteristics{{kIPS, ips}, {kPower, power}};
    auto cores = platform->ToCPUCoreSet(threads);
    auto cores_count = platform->GetCoreCountPerType(cores);
    OperatingPoint op{"default", characteristics, threads, cores_count};
    return op;
  }
};

TEST_F(RaptorLakeOperatingPointTableTest, OperatingPointTable) {
  ThreadSetOperatingPointTable op_table(*platform);
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

  auto ops = op_table.GetOperatingPoints(false);
  EXPECT_EQ(ops.size(), 15);
}
