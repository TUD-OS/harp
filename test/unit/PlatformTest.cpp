#include <gtest/gtest.h>

#include "util/platform/cpu_sets.h"
#include "util/platform/reader.h"

#include "util/platform/cpu_sets.h"
#include "util/platform/reader.h"
#include <gtest/gtest.h>

class BasePlatformTest : public ::testing::Test {
protected:
  YamlPlatformReader reader;
  Platform platform;

  virtual std::string GetPlatformFilePath() = 0;

  void SetUp() override {
    platform = reader.ReadFromFile(GetPlatformFilePath());
  }

  void TearDown() override {
  }
};

class OdroidTest : public BasePlatformTest {
protected:
  std::string GetPlatformFilePath() override {
    return "../examples/platforms/platform_odroid.yaml";
  }
};

class RaptorLakeTest : public BasePlatformTest {
protected:
  std::string GetPlatformFilePath() override {
    return "../examples/platforms/platform_raptor-8P16E.yaml";
  }
};

TEST_F(OdroidTest, PlatformStructure) {
  EXPECT_EQ(platform.GetCPUCores().size(), 8);
  auto cores = platform.GetCPUCores(CPUCoreSet{0, 1, 4, 5});
  EXPECT_EQ(cores[0].get().GetID(), 0);
  EXPECT_EQ(cores[0].get().GetType().GetName(), "A7");
  EXPECT_EQ(cores[1].get().GetID(), 1);
  EXPECT_EQ(cores[1].get().GetType().GetName(), "A7");
  EXPECT_EQ(cores[1].get().GetType().GetNumThreads(), 1);

  EXPECT_EQ(cores[2].get().GetID(), 4);
  EXPECT_EQ(cores[2].get().GetType().GetName(), "A15");
  EXPECT_EQ(cores[3].get().GetID(), 5);
  EXPECT_EQ(cores[3].get().GetType().GetName(), "A15");
  EXPECT_EQ(cores[3].get().GetType().GetNumThreads(), 1);
}

TEST_F(RaptorLakeTest, PlatformStructure) {
  EXPECT_EQ(platform.GetCPUCores().size(), 24);
  auto cores = platform.GetCPUCores(CPUCoreSet{0, 1, 8, 9});
  EXPECT_EQ(cores[0].get().GetID(), 0);
  EXPECT_EQ(cores[0].get().GetType().GetName(), "P-core");
  EXPECT_EQ(cores[1].get().GetID(), 1);
  EXPECT_EQ(cores[1].get().GetType().GetName(), "P-core");
  EXPECT_EQ(cores[1].get().GetType().GetNumThreads(), 2);

  EXPECT_EQ(cores[2].get().GetID(), 8);
  EXPECT_EQ(cores[2].get().GetType().GetName(), "E-core");
  EXPECT_EQ(cores[3].get().GetID(), 9);
  EXPECT_EQ(cores[3].get().GetType().GetName(), "E-core");
  EXPECT_EQ(cores[3].get().GetType().GetNumThreads(), 1);
}

// Add more tests as needed for each platform...
