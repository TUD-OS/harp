
#include <gtest/gtest.h>

#include <list>
#include <vector>

#include "util/platform/cpu_sets.h"

TEST(CPUCoreSetTest, DefaultConstructor) {
  CPUCoreSet set;
  EXPECT_EQ(set.Size(), 0);
}

TEST(CPUCoreSetTest, InitializerListConstructor) {
  CPUCoreSet set = {1, 2, 3};
  EXPECT_EQ(set.Size(), 3);
  EXPECT_TRUE(set == CPUCoreSet({1, 2, 3}));
}

TEST(CPUCoreSetTest, TemplateConstructor) {
  std::vector<int> vec = {1, 2, 3, 4};
  CPUCoreSet set(vec);
  EXPECT_EQ(set.Size(), 4);
  EXPECT_TRUE(set == CPUCoreSet({1, 2, 3, 4}));

  std::list<int> lst = {5, 6, 7};
  CPUCoreSet set2(lst);
  EXPECT_EQ(set2.Size(), 3);
  EXPECT_TRUE(set2 == CPUCoreSet({5, 6, 7}));
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

TEST(CPUCoreSetTest, GetCoreList) {
  CPUCoreSet set = {1, 2, 3};
  std::vector<int> core_list = set.GetList();
  EXPECT_EQ(core_list, std::vector<int>({1, 2, 3}));
}
