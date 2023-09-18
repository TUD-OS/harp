#include <gtest/gtest.h>

class BasePlatformTest : public ::testing::Test {
protected:
  YamlPlatformReader reader;
  Platform platform;

  virtual std::string GetPlatformFilePath() = 0;

  void SetUp() override {
    platform = reader.ReadFromFile(GetPlatformFilePath());
  }

  void TearDown() override {}
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
