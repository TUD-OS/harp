
#include "util/platform/cpu_sets.h"

#include "unit/PlatformFixtures.h"

using namespace tetris;

TEST_F(OdroidTest, PlatformStructure) {
  EXPECT_EQ(platform->GetCPUCores().size(), 8);

  auto cores = platform->GetCPUCores(CPUCoreSet{0, 1, 4, 5});

  EXPECT_EQ(cores[0]->GetID(), 0);
  EXPECT_EQ(cores[0]->GetType().GetName(), "A7");

  auto threads_0 = cores[0]->GetCPUThreads();
  EXPECT_EQ(threads_0.size(), 1);
  EXPECT_EQ(threads_0[0]->GetName(), "ARM00");
  EXPECT_EQ(threads_0[0]->GetID(), 0);

  EXPECT_EQ(cores[1]->GetID(), 1);
  EXPECT_EQ(cores[1]->GetType().GetName(), "A7");
  EXPECT_EQ(cores[1]->GetType().GetNumThreads(), 1);

  auto threads_1 = cores[1]->GetCPUThreads();
  EXPECT_EQ(threads_1.size(), 1);
  EXPECT_EQ(threads_1[0]->GetName(), "ARM01");
  EXPECT_EQ(threads_1[0]->GetID(), 1);

  EXPECT_EQ(cores[2]->GetID(), 4);
  EXPECT_EQ(cores[2]->GetType().GetName(), "A15");

  EXPECT_EQ(cores[3]->GetID(), 5);
  EXPECT_EQ(cores[3]->GetType().GetName(), "A15");
  EXPECT_EQ(cores[3]->GetType().GetNumThreads(), 1);

  auto threads_3 = cores[3]->GetCPUThreads();
  EXPECT_EQ(threads_3.size(), 1);
  EXPECT_EQ(threads_3[0]->GetName(), "ARM05");
  EXPECT_EQ(threads_3[0]->GetID(), 5);
}

TEST_F(RaptorLakeTest, PlatformStructure) {
  EXPECT_EQ(platform->GetCPUCores().size(), 24);

  auto cores = platform->GetCPUCores(CPUCoreSet{0, 1, 8, 9});

  EXPECT_EQ(cores[0]->GetID(), 0);
  EXPECT_EQ(cores[0]->GetType().GetName(), "P-core");

  EXPECT_EQ(cores[1]->GetID(), 1);
  EXPECT_EQ(cores[1]->GetType().GetName(), "P-core");
  EXPECT_EQ(cores[1]->GetType().GetNumThreads(), 2);

  auto threads_1 = cores[1]->GetCPUThreads();
  EXPECT_EQ(threads_1.size(), 2);
  EXPECT_EQ(threads_1[0]->GetName(), "P1_0");
  EXPECT_EQ(threads_1[0]->GetID(), 2);
  EXPECT_EQ(threads_1[1]->GetName(), "P1_1");
  EXPECT_EQ(threads_1[1]->GetID(), 3);

  EXPECT_EQ(cores[2]->GetID(), 8);
  EXPECT_EQ(cores[2]->GetType().GetName(), "E-core");

  EXPECT_EQ(cores[3]->GetID(), 9);
  EXPECT_EQ(cores[3]->GetType().GetName(), "E-core");
  EXPECT_EQ(cores[3]->GetType().GetNumThreads(), 1);

  auto threads_3 = cores[3]->GetCPUThreads();
  EXPECT_EQ(threads_3.size(), 1);
  EXPECT_EQ(threads_3[0]->GetName(), "E01");
  EXPECT_EQ(threads_3[0]->GetID(), 17);
}

TEST_F(OdroidTest, GetCPUThreads) {
  auto threads = platform->GetCPUThreads(CPUThreadSet{0, 2, 4, 6});

  EXPECT_EQ(threads.size(), 4);

  auto cores = platform->GetCPUCores();
  EXPECT_EQ(threads[0]->GetID(), 0);
  EXPECT_EQ(threads[0]->GetName(), "ARM00");
  EXPECT_EQ(threads[0]->GetCPUCore().GetID(), 0);

  EXPECT_EQ(threads[1]->GetID(), 2);
  EXPECT_EQ(threads[1]->GetName(), "ARM02");
  EXPECT_EQ(threads[1]->GetCPUCore().GetID(), 2);

  EXPECT_EQ(threads[2]->GetID(), 4);
  EXPECT_EQ(threads[2]->GetName(), "ARM04");
  EXPECT_EQ(threads[2]->GetCPUCore().GetID(), 4);

  EXPECT_EQ(threads[3]->GetID(), 6);
  EXPECT_EQ(threads[3]->GetName(), "ARM06");
  EXPECT_EQ(threads[3]->GetCPUCore().GetID(), 6);
}

TEST_F(RaptorLakeTest, GetCPUThreads) {
  auto threads = platform->GetCPUThreads(CPUThreadSet{0, 8, 16, 20});

  EXPECT_EQ(threads.size(), 4);

  auto cores = platform->GetCPUCores();
  EXPECT_EQ(threads[0]->GetID(), 0);
  EXPECT_EQ(threads[0]->GetName(), "P0_0");
  EXPECT_EQ(threads[0]->GetCPUCore().GetID(), 0);

  EXPECT_EQ(threads[1]->GetID(), 8);
  EXPECT_EQ(threads[1]->GetName(), "P4_0");
  EXPECT_EQ(threads[1]->GetCPUCore().GetID(), 4);

  EXPECT_EQ(threads[2]->GetID(), 16);
  EXPECT_EQ(threads[2]->GetName(), "E00");
  EXPECT_EQ(threads[2]->GetCPUCore().GetID(), 8);

  EXPECT_EQ(threads[3]->GetID(), 20);
  EXPECT_EQ(threads[3]->GetName(), "E04");
  EXPECT_EQ(threads[3]->GetCPUCore().GetID(), 12);
}

TEST_F(OdroidTest, ConvertCPUSets) {
  auto threads1 = CPUThreadSet{0, 2, 5, 7};
  auto cores1 = CPUCoreSet{0, 2, 5, 7};
  EXPECT_EQ(platform->ToCPUCoreSet(threads1), cores1);

  auto cores2 = CPUCoreSet{0, 1, 3, 7};
  auto threads2 = CPUThreadSet{0, 1, 3, 7};
  EXPECT_EQ(platform->ToCPUThreadSet(cores2), threads2);
}

TEST_F(RaptorLakeTest, ConvertCPUSets) {
  auto threads1 = CPUThreadSet{0, 2, 3, 7, 10, 16, 18};
  auto cores1 = CPUCoreSet{0, 1, 3, 5, 8, 10};
  EXPECT_EQ(platform->ToCPUCoreSet(threads1), cores1);

  auto cores2 = CPUCoreSet{0, 1, 5, 10, 15};
  auto threads2 = CPUThreadSet{0, 1, 2, 3, 10, 11, 18, 23};
  EXPECT_EQ(platform->ToCPUThreadSet(cores2), threads2);
}
