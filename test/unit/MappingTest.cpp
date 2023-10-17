#include "unit/PlatformFixtures.h"

#include "util/mapping.h"
#include "util/platform/cpu_sets.h"
#include "util/platform/equiv_res_alloc.h"

using namespace tetris;

class OdroidMappingTest : public OdroidTest {
protected:
  Mapping GetMapping_0() {
    Mapping m{*platform};
    return m;
  }

  Mapping GetMapping_2L() {
    std::vector<std::pair<std::string, std::string>> threads{{"a", "ARM00"},
                                                             {"b", "ARM01"}};
    RegionAffinities<std::string> region_threads{};
    std::vector<std::pair<std::string, std::string>> characteristics{
        {"execution_time", "100"}, {"energy", "150"}};

    Mapping m{*platform, "2L", threads, region_threads, characteristics};
    return m;
  }

  Mapping GetMapping_2B() {
    std::vector<std::pair<std::string, std::string>> threads{{"a", "ARM04"},
                                                             {"b", "ARM07"}};
    RegionAffinities<std::string> region_threads{};
    std::vector<std::pair<std::string, std::string>> characteristics{
        {"execution_time", "50"}, {"energy", "250"}};

    Mapping m{*platform, "2B", threads, region_threads, characteristics};
    return m;
  }

  Mapping GetMapping_1L1B() {
    std::vector<std::pair<std::string, std::string>> threads{{"a", "ARM02"},
                                                             {"b", "ARM06"}};
    RegionAffinities<std::string> region_threads{};
    std::vector<std::pair<std::string, std::string>> characteristics{
        {"execution_time", "70"}, {"energy", "200"}};

    Mapping m{*platform, "1L1B", threads, region_threads, characteristics};
    return m;
  }
};

class RaptorLakeMappingTest : public RaptorLakeTest {
protected:
  Mapping GetMapping_0() {
    Mapping m{*platform};
    return m;
  }

  /* 1HP - 1 HyperThreaded P-Core (both threads are active)
   * 1SP - 1 single-threaded P-Core (a single thread is active)
   * 1E - 1 E-core */
  Mapping GetMapping_1HP1SP2E() {
    std::vector<std::pair<std::string, std::string>> threads{{"a", "P0_0"},
                                                             {"b", "P0_1"},
                                                             {"c", "P2_1"},
                                                             {"d", "E02"},
                                                             {"e", "E05"}};
    RegionAffinities<std::string> region_threads{};
    std::vector<std::pair<std::string, std::string>> characteristics{
        {"execution_time", "100"}, {"energy", "150"}};

    Mapping m{*platform, "1HP1SP2E", threads, region_threads, characteristics};
    return m;
  }
};

TEST_F(OdroidMappingTest, MappingConstructor) {
  auto m1 = GetMapping_0();
  EXPECT_EQ(m1.name, "");
  EXPECT_EQ(m1.thread_map.size(), 0);
  EXPECT_EQ(m1.region_map.size(), 0);
  EXPECT_EQ(m1.characteristics_map.size(), 0);
  EXPECT_EQ(m1.cpus, CPUThreadSet{});

  auto m2 = GetMapping_2L();
  EXPECT_EQ(m2.name, "2L");
  EXPECT_EQ(m2.thread_map.size(), 2);
  EXPECT_EQ(m2.region_map.size(), 0);
  EXPECT_EQ(m2.characteristic("execution_time"), 100);
  EXPECT_EQ(m2.characteristic("energy"), 150);
  EXPECT_EQ(m2.cpus, (CPUThreadSet{0, 1}));

  auto m3{m1};
  EXPECT_EQ(m3.name, "");
  EXPECT_EQ(m3.cpus, CPUThreadSet{});

  m3 = m2;
  EXPECT_EQ(m3.name, "2L");
  EXPECT_EQ(m3.cpus, (CPUThreadSet{0, 1}));

  auto m4 = GetMapping_2B();
  EXPECT_EQ(m4.name, "2B");
  EXPECT_EQ(m4.thread_map.size(), 2);
  EXPECT_EQ(m4.region_map.size(), 0);
  EXPECT_EQ(m4.characteristic("execution_time"), 50);
  EXPECT_EQ(m4.characteristic("energy"), 250);
  EXPECT_EQ(m4.cpus, (CPUThreadSet{4, 7}));

  auto m5 = GetMapping_1L1B();
  EXPECT_EQ(m5.name, "1L1B");
  EXPECT_EQ(m5.thread_map.size(), 2);
  EXPECT_EQ(m5.region_map.size(), 0);
  EXPECT_EQ(m5.characteristic("execution_time"), 70);
  EXPECT_EQ(m5.characteristic("energy"), 200);
  EXPECT_EQ(m5.cpus, (CPUThreadSet{2, 6}));
}

TEST_F(OdroidMappingTest, EquivalentMappings) {
  auto m1 = GetMapping_2L();
  EXPECT_EQ(m1.cpus, (CPUThreadSet{0, 1}));

  auto m1_0 = allocator->FindEquivMapping(m1, CPUCoreSet{});
  EXPECT_TRUE(m1_0.has_value());
  EXPECT_EQ(m1_0->cpus, (CPUThreadSet{0, 1}));

  auto m1_1 = allocator->FindEquivMapping(m1, CPUCoreSet{0});
  EXPECT_TRUE(m1_1.has_value());
  EXPECT_EQ(m1_1->cpus.Size(), 2);
  EXPECT_FALSE(m1_1->cpus.At(0));
  EXPECT_TRUE(m1_1->cpus.At(1));
  EXPECT_TRUE(m1_1->cpus.At(2) || m1_1->cpus.At(3));
  EXPECT_FALSE(m1_1->cpus.At(2) && m1_1->cpus.At(3));

  auto m2 = GetMapping_1L1B();
  EXPECT_EQ(m2.cpus, (CPUThreadSet{2, 6}));

  auto m2_0 = allocator->FindEquivMapping(m2, CPUCoreSet{0, 1, 3, 4, 5, 7});
  EXPECT_TRUE(m2_0.has_value());
  EXPECT_EQ(m2_0->cpus, (CPUThreadSet{2, 6}));

  auto m2_1 = allocator->FindEquivMapping(m2, CPUCoreSet{0, 1, 2, 3});
  EXPECT_FALSE(m2_1.has_value());
}

TEST_F(RaptorLakeMappingTest, MappingConstructor) {
  auto m1 = GetMapping_0();
  EXPECT_EQ(m1.name, "");
  EXPECT_EQ(m1.thread_map.size(), 0);
  EXPECT_EQ(m1.region_map.size(), 0);
  EXPECT_EQ(m1.characteristics_map.size(), 0);
  EXPECT_EQ(m1.cpus, CPUThreadSet{});

  auto m2 = GetMapping_1HP1SP2E();
  EXPECT_EQ(m2.name, "1HP1SP2E");
  EXPECT_EQ(m2.thread_map.size(), 5);
  EXPECT_EQ(m2.region_map.size(), 0);
  EXPECT_EQ(m2.characteristic("execution_time"), 100);
  EXPECT_EQ(m2.characteristic("energy"), 150);
  EXPECT_EQ(m2.cpus, (CPUThreadSet{0, 1, 5, 18, 21}));

  auto m3{m1};
  EXPECT_EQ(m3.name, "");
  EXPECT_EQ(m3.cpus, CPUThreadSet{});

  m3 = m2;
  EXPECT_EQ(m3.name, "1HP1SP2E");
  EXPECT_EQ(m3.cpus, (CPUThreadSet{0, 1, 5, 18, 21}));
}

TEST_F(RaptorLakeMappingTest, EquivalentMappings) {
  auto m1 = GetMapping_1HP1SP2E();
  EXPECT_EQ(m1.cpus, (CPUThreadSet{0, 1, 5, 18, 21}));

  auto m1_0 = allocator->FindEquivMapping(m1, CPUCoreSet{});
  EXPECT_TRUE(m1_0.has_value());
  EXPECT_EQ(m1_0->cpus, (CPUThreadSet{0, 1, 5, 18, 21}));

  auto m1_1 = allocator->FindEquivMapping(m1, CPUCoreSet{2});
  EXPECT_TRUE(m1_1.has_value());
  CPUThreadSet m1_1_cpus = m1_1->cpus;
  EXPECT_EQ(m1_1_cpus.Size(), 5);
  EXPECT_TRUE(m1_1_cpus.At(0) && m1_1_cpus.At(1) && m1_1_cpus.At(18) &&
              m1_1_cpus.At(21));
  m1_1_cpus ^= CPUThreadSet{0, 1, 18, 21};
  EXPECT_EQ(m1_1_cpus.Size(), 1);
  int rem = m1_1_cpus.GetList()[0];
  EXPECT_TRUE(rem == 2 || rem == 3 || (rem >= 4 && rem <= 15));

  auto m1_2 = allocator->FindEquivMapping(m1, CPUCoreSet{0, 1});
  EXPECT_TRUE(m1_2.has_value());
  CPUThreadSet m1_2_cpus = m1_2->cpus;
  EXPECT_EQ(m1_2_cpus.Size(), 5);
  EXPECT_TRUE(m1_2_cpus.At(5) && m1_2_cpus.At(18) && m1_2_cpus.At(21));
  m1_2_cpus ^= CPUThreadSet{5, 18, 21};
  auto rlist = m1_2_cpus.GetList();
  EXPECT_EQ(rlist.size(), 2);
  EXPECT_EQ(rlist[0] + 1, rlist[1]);
  EXPECT_TRUE(6 <= rlist[0] && rlist[0] < 15);
  EXPECT_EQ(rlist[0] % 2, 0);
}
