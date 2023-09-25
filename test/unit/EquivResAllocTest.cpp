
#include "unit/PlatformFixtures.h"

#include "util/platform/cpu_sets.h"

TEST_F(OdroidTest, GetEquivClassName) {
  auto cores1 = CPUCoreSet{0, 2, 5, 7};
  EXPECT_EQ(allocator->GetEquivClassName(cores1), "2 A15 + 2 A7");

  auto cores2 = CPUCoreSet{0, 3};
  EXPECT_EQ(allocator->GetEquivClassName(cores2), "2 A7");

  auto threads1 = CPUThreadSet{4};
  EXPECT_EQ(allocator->GetEquivClassName(threads1), "1 A15");

  auto threads2 = CPUThreadSet{0, 1, 2, 3, 4, 5, 6, 7};
  EXPECT_EQ(allocator->GetEquivClassName(threads2), "4 A15 + 4 A7");
}

TEST_F(RaptorLakeTest, GetEquivClassName) {
  auto cores1 = CPUCoreSet{0, 1, 3, 5, 8, 10};
  EXPECT_EQ(allocator->GetEquivClassName(cores1), "2 E-core + 4 P-core");

  auto cores2 = CPUCoreSet{0, 3, 4};
  EXPECT_EQ(allocator->GetEquivClassName(cores2), "3 P-core");

  auto threads1 = CPUThreadSet{0, 2, 3, 7, 10, 16, 18};
  EXPECT_EQ(allocator->GetEquivClassName(threads1), "2 E-core + 4 P-core");

  auto threads2 = CPUThreadSet{0, 1, 2, 3, 4};
  EXPECT_EQ(allocator->GetEquivClassName(threads2), "3 P-core");
}
