#include <gtest/gtest.h>

#include "util/platform/reader.h"

class BasePlatformTest : public ::testing::Test {
protected:
  tetris::YamlPlatformReader reader;
  std::unique_ptr<tetris::Platform> platform;
  tetris::EquivResAllocator *allocator; // shortcut for allocator

  virtual std::string GetPlatformFilePath() = 0;

  void SetUp() override {
    platform = reader.ReadFromFile(GetPlatformFilePath());
    allocator = &platform->GetEquivResAllocator();
  }

  void TearDown() override {}
};

class OdroidTest : public BasePlatformTest {
protected:
  std::string GetPlatformFilePath() override {
    return "../examples/odroid/platform_odroid.yaml";
  }
};

class RaptorLakeTest : public BasePlatformTest {
protected:
  std::string GetPlatformFilePath() override {
    return "../examples/raptor-lake-8P16E/platform_raptor-8P16E.yaml";
  }
};
