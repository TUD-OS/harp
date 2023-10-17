#include <gtest/gtest.h>

#include "util/mapping_reader.h"
#include "util/platform/reader.h"

using namespace tetris;

// This test checks the functionality of the read_mapping_directory function
TEST(MappingReaderTest, ReadMappingDirectoryOdroidTest) {
  YamlPlatformReader platform_reader;
  std::unique_ptr<Platform> platform =
      platform_reader.ReadFromFile("../examples/odroid/platform_odroid.yaml");
  // Define the directory containing the test mapping data
  std::string test_directory = "../examples/odroid/mappings";

  // Call the function to read mappings from the test directory
  auto mapping_reader = MappingReader(*platform.get());
  auto app_mappings = mapping_reader.read_mapping_directory(test_directory);

  // Check the number of applications for which mappings have been read
  EXPECT_EQ(app_mappings.size(), 4);

  // Check the number of mappings read for the 'mandelbrot' application
  EXPECT_EQ(app_mappings.count("mandelbrot"), 1);
  EXPECT_EQ(app_mappings["mandelbrot"].size(), 4);

  EXPECT_EQ(app_mappings.count("mandelbrot_2"), 1);
  EXPECT_EQ(app_mappings["mandelbrot_2"].size(), 4);

  EXPECT_EQ(app_mappings.count("htop"), 1);
  EXPECT_EQ(app_mappings["htop"].size(), 2);

  EXPECT_EQ(app_mappings.count("jpeg"), 1);
  EXPECT_EQ(app_mappings["jpeg"].size(), 18);
}

// This test checks the functionality of the read_mapping_directory function
TEST(MappingReaderTest, ReadMappingDirectoryRaptorTest) {
  YamlPlatformReader platform_reader;
  std::unique_ptr<Platform> platform = platform_reader.ReadFromFile(
      "../examples/raptor-lake-8P16E/platform_raptor-8P16E.yaml");
  // Define the directory containing the test mapping data
  std::string test_directory = "../examples/raptor-lake-8P16E/mappings";

  // Call the function to read mappings from the test directory
  auto mapping_reader = MappingReader(*platform.get());
  auto app_mappings = mapping_reader.read_mapping_directory(test_directory);

  // Check the number of applications for which mappings have been read
  EXPECT_EQ(app_mappings.size(), 3);

  // Check the number of mappings read for the 'mandelbrot' application
  EXPECT_EQ(app_mappings.count("ep_C"), 1);
  EXPECT_EQ(app_mappings["ep_C"].size(), 86);

  EXPECT_EQ(app_mappings.count("mg_C"), 1);
  EXPECT_EQ(app_mappings["mg_C"].size(), 48);

  EXPECT_EQ(app_mappings.count("sp_B"), 1);
  EXPECT_EQ(app_mappings["sp_B"].size(), 164);
}
