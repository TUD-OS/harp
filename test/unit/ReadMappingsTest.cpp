#include <gtest/gtest.h>

#include "server/mapping_reader.h"

// This test checks the functionality of the read_mapping_directory function
TEST(MappingReaderTest, ReadMappingDirectoryTest) {
  // Define the directory containing the test mapping data
  std::string test_directory = "../mappings";

  // Call the function to read mappings from the test directory
  auto app_mappings = MappingReader::read_mapping_directory(test_directory);

  // Check the number of applications for which mappings have been read
  EXPECT_EQ(app_mappings.size(), 4);

  // Check the number of mappings read for the 'mandelbrot' application
  EXPECT_EQ(app_mappings.count("mandelbrot"), 1);
  EXPECT_EQ(app_mappings["mandelbrot"].size(), 3);

  EXPECT_EQ(app_mappings.count("mandelbrot_2"), 1);
  EXPECT_EQ(app_mappings["mandelbrot_2"].size(), 3);

  EXPECT_EQ(app_mappings.count("htop"), 1);
  EXPECT_EQ(app_mappings["htop"].size(), 1);

  EXPECT_EQ(app_mappings.count("jpeg"), 1);
  EXPECT_EQ(app_mappings["jpeg"].size(), 18);
}
