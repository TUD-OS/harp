#include <gtest/gtest.h>

#include "util/platform/cpu_sets.h"
#include "util/platform/reader.h"

TEST(PlatformReaderTest, OdroidTest) {
  YamlPlatformReader reader;
  Platform platform =
      reader.ReadFromFile("../examples/platforms/platform_odroid.yaml");

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

TEST(PlatformReaderTest, RaptorLakeTest) {
  YamlPlatformReader reader;
  Platform platform =
      reader.ReadFromFile("../examples/platforms/platform_raptor-8P16E.yaml");

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
