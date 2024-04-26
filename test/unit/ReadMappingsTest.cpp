#include <gtest/gtest.h>

#include "util/mapping_reader.h"
#include "util/platform/reader.h"

using namespace tetris;

// This test checks the functionality of the read_mapping_directory function
TEST(MappingReaderTest, ReadMappingDirectoryOdroidTest) {
  YamlPlatformReader platform_reader;
  std::unique_ptr<Platform> platform =
      platform_reader.ReadFromFile("../examples/odroid/platform.yaml");
  // Define the directory containing the test mapping data
  std::string test_directory = "../examples/odroid/mappings";

  // Call the function to read mappings from the test directory
  auto mapping_reader = MappingReader(*platform.get());
  auto app_mappings = mapping_reader.read_mapping_directory(test_directory);

  // Check the number of applications for which mappings have been read
  EXPECT_EQ(app_mappings.size(), 14);

  // Check the number of mappings read for the 'mandelbrot' application
  EXPECT_EQ(app_mappings.count("mandelbrot"), 1);
  EXPECT_EQ(app_mappings["mandelbrot"].size(), 4);

  EXPECT_EQ(app_mappings.count("mandelbrot_2"), 1);
  EXPECT_EQ(app_mappings["mandelbrot_2"].size(), 4);

  EXPECT_EQ(app_mappings.count("sample_dpm"), 1);
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
      "../examples/raptor-lake-8P16E/platform.yaml");
  // Define the directory containing the test mapping data
  std::string test_directory = "../examples/raptor-lake-8P16E/mappings";

  // Call the function to read mappings from the test directory
  auto mapping_reader = MappingReader(*platform.get());
  auto app_mappings = mapping_reader.read_mapping_directory(test_directory);

  // Check the number of applications for which mappings have been read
  EXPECT_EQ(app_mappings.size(), 10);

  // Check the number of mappings read for the OpenMP applications
  EXPECT_EQ(app_mappings.count("ep.C"), 1);
  EXPECT_EQ(app_mappings["ep.C"].size(), 93);

  EXPECT_EQ(app_mappings.count("mg.C"), 1);
  EXPECT_EQ(app_mappings["mg.C"].size(), 50);

  EXPECT_EQ(app_mappings.count("sp.B"), 1);
  EXPECT_EQ(app_mappings["sp.B"].size(), 164);
}
