
#include <gtest/gtest.h>

#include <list>
#include <vector>

#include "util/platform/cpu_sets.h"

TEST(CPUCoreSetTest, Constructors) {
  CPUCoreSet set1;
  EXPECT_EQ(set1.Size(), 0);

  CPUCoreSet set2 = {1, 2, 3};
  EXPECT_EQ(set2.Size(), 3);
  EXPECT_TRUE(set2 == CPUCoreSet({1, 2, 3}));

  std::vector<int> vec = {1, 2, 3, 4};
  CPUCoreSet set3(vec);
  EXPECT_EQ(set3.Size(), 4);
  EXPECT_TRUE(set3 == CPUCoreSet({1, 2, 3, 4}));
}

TEST(CPUCoreSetTest, SetAndErase) {
  CPUCoreSet set;
  set.Set(1);
  set.Set(2);
  EXPECT_EQ(set.Size(), 2);
  set.Erase(2);
  EXPECT_EQ(set.Size(), 1);
  EXPECT_TRUE(set == CPUCoreSet({1}));
}

TEST(CPUCoreSetTest, BinaryOperators) {
  CPUCoreSet set1 = {1, 2, 3, 4};
  CPUCoreSet set2 = {3, 4, 5, 6};
  CPUCoreSet set3 = {7, 8};
  CPUCoreSet res_union = set1 | set2;
  EXPECT_TRUE(res_union == CPUCoreSet({1, 2, 3, 4, 5, 6}));
  CPUCoreSet res_intersection = set1 & set2;
  EXPECT_TRUE(res_intersection == CPUCoreSet({3, 4}));
  EXPECT_TRUE(set1.OverlapsWith(set2));
  EXPECT_FALSE(set1.OverlapsWith(set3));
}

TEST(CPUCoreSetTest, GetList) {
  CPUCoreSet set = {1, 2, 3};
  std::vector<int> core_list = set.GetList();
  EXPECT_EQ(core_list, std::vector<int>({1, 2, 3}));
}

TEST(CPUThreadSetTest, Constructors) {
  CPUThreadSet set1;
  EXPECT_EQ(set1.Size(), 0);

  CPUThreadSet set2 = {1, 2, 3};
  EXPECT_EQ(set2.Size(), 3);
  EXPECT_TRUE(set2 == CPUThreadSet({1, 2, 3}));

  std::vector<int> vec = {1, 2, 3, 4};
  CPUThreadSet set3(vec);
  EXPECT_EQ(set3.Size(), 4);
  EXPECT_TRUE(set3 == CPUThreadSet({1, 2, 3, 4}));
}

TEST(CPUThreadSetTest, SetAndErase) {
  CPUThreadSet set;
  set.Set(1);
  set.Set(2);
  EXPECT_EQ(set.Size(), 2);
  set.Erase(2);
  EXPECT_EQ(set.Size(), 1);
  EXPECT_TRUE(set == CPUThreadSet({1}));
}

TEST(CPUThreadSetTest, BinaryOperators) {
  CPUThreadSet set1 = {1, 2, 3, 4};
  CPUThreadSet set2 = {3, 4, 5, 6};
  CPUThreadSet set3 = {7, 8};
  CPUThreadSet res_union = set1 | set2;
  EXPECT_TRUE(res_union == CPUThreadSet({1, 2, 3, 4, 5, 6}));
  CPUThreadSet res_intersection = set1 & set2;
  EXPECT_TRUE(res_intersection == CPUThreadSet({3, 4}));
  EXPECT_TRUE(set1.OverlapsWith(set2));
  EXPECT_FALSE(set1.OverlapsWith(set3));
}

TEST(CPUThreadSetTest, GetList) {
  CPUThreadSet set = {1, 2, 3};
  std::vector<int> core_list = set.GetList();
  EXPECT_EQ(core_list, std::vector<int>({1, 2, 3}));
}

TEST(CPUThreadSetTest, ToCpuSetT) {
  CPUThreadSet set = {1, 2, 3};
  cpu_set_t res = set.ToCpuSetT();

  cpu_set_t ref;
  CPU_ZERO(&ref);
  CPU_SET(1, &ref);
  CPU_SET(2, &ref);
  CPU_SET(3, &ref);
  EXPECT_NE(CPU_EQUAL(&res, &ref), 0);
}
