#include <gtest/gtest.h>

#include "util/platform/reader.h"

TEST(PlatformReaderTest, OdroidTest) {
  YamlPlatformReader reader;
  Platform platform =
      reader.ReadFromFile("../examples/platforms/platform_odroid.yaml");

  EXPECT_EQ(platform.GetCPUCores().size(), 8);
}

TEST(PlatformReaderTest, RaptorLakeTest) {
  YamlPlatformReader reader;
  Platform platform =
      reader.ReadFromFile("../examples/platforms/platform_raptor-8P16E.yaml");

  EXPECT_EQ(platform.GetCPUCores().size(), 24);
}
