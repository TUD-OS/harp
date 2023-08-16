#ifndef MAPPING_READER_H
#define MAPPING_READER_H

#include <map>
#include <string>
#include <vector>

#include "util/json.h"

#include "util/mapping.h"

/**
 * @brief Abstract class for mapping reader
 */
class BaseMappingReader {
 public:
  virtual ~BaseMappingReader() = default;

  /**
   * @brief Read mappings from given path
   *
   * @param path The path to the mappings
   * @return vector of mappings
   */
  virtual std::vector<Mapping> read_mappings(const std::string &path) = 0;
};

/**
 * @brief Reader for JSON format mappings
 */
class JsonMappingReader : public BaseMappingReader {
 public:
  static constexpr char kKnobDescFilename[] = "__confdefs__.json";
  std::vector<Mapping> read_mappings(const std::string &dir_path) override;

 private:
  Mapping parse_mapping(const nlohmann::json &json_mapping);
};

// CsvMappingReader and YamlMappingReader classes are to be defined later
class CsvMappingReader : public BaseMappingReader {
 public:
  std::vector<Mapping> read_mappings(const std::string &file_path);
};

class YamlMappingReader : public BaseMappingReader {
 public:
  std::vector<Mapping> read_mappings(const std::string &file_path);
};

class MappingReader {
 public:
  static std::map<std::string, std::vector<Mapping>> read_mapping_directory(
      const std::string &base_dir);

 private:
  static void log_mappings_details(
      const std::map<std::string, std::vector<Mapping>> &);
};

#endif  // MAPPING_READER_H
