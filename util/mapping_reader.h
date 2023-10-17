#ifndef MAPPING_READER_H
#define MAPPING_READER_H

#include <map>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "util/json.h"
#include "util/mapping.h"

namespace tetris {

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
  virtual std::vector<Mapping> read_mappings(const Platform &,
                                             const std::string &) = 0;
};

/**
 * @brief Reader for JSON format mappings
 */
class JsonMappingReader : public BaseMappingReader {
public:
  static constexpr char kKnobDescFilename[] = "__confdefs__.json";
  std::vector<Mapping> read_mappings(const Platform &,
                                     const std::string &) override;

private:
  Mapping parse_mapping(const Platform &, const nlohmann::json &);
};

// CsvMappingReader and YamlMappingReader classes are to be defined later
class CsvMappingReader : public BaseMappingReader {
public:
  std::vector<Mapping> read_mappings(const Platform &,
                                     const std::string &) override;
  ;
};

class YamlMappingReader : public BaseMappingReader {

  std::vector<Mapping> parse_mappings_dpm(const Platform &, const std::string &,
                                          const YAML::Node &);
  std::vector<Mapping> parse_mappings_omp(const Platform &, const std::string &,
                                          const YAML::Node &);

public:
  std::vector<Mapping> read_mappings(const Platform &,
                                     const std::string &) override;
  ;
};

class MappingReader {
public:
  explicit MappingReader(const Platform &platform) : _platform{platform} {}

  std::map<std::string, std::vector<Mapping>>
  read_mapping_directory(const std::string &base_dir);

private:
  void
  log_mappings_details(const std::map<std::string, std::vector<Mapping>> &);

  const Platform &_platform;
};

} /* namespace tetris */

#endif // MAPPING_READER_H
