#include "mapping_reader.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>

#include "util/csv.h"
#include "util/debug_util.h"
#include "util/string_util.h"
#include "util/platform/platform.h"


namespace tetris {

/**
 * \brief Parse mapping from given JSON object
 *
 * The function reads the JSON object which is expected to contain a mapping of
 * threads and regions, and their corresponding CPU affinities. Each of these
 * mappings are then stored in the respective containers for later use.
 *
 * \param json_mapping JSON object containing the mapping
 * \return Mapping object
 */
Mapping JsonMappingReader::parse_mapping(const Platform &platform,
                                         const nlohmann::json &json_mapping) {
  // Container for thread mappings.
  std::vector<std::pair<std::string, std::string>> threads;

  // Container for region mappings.
  RegionAffinities<std::string> regions{};

  // Container for mapping characteristics.
  std::vector<std::pair<std::string, std::string>> characteristics;

  // Iterate over all process mapping information.
  for (const auto &mapping : json_mapping["mapping"]) {
    if (mapping["type"] == "process") {
      // A regular process. Retrieve its name and assigned core.
      std::string thread_name = mapping["name"];
      std::string cpu_name = mapping["core"];
      threads.emplace_back(thread_name, cpu_name);
    } else if (mapping["type"] == "DLP") {
      // A parallel region. Process each replica within the region.
      ReplicaAffinities<std::string> replica_affinities{};
      for (const auto &replica : mapping["replicas"]) {
        ProcessAffinities<std::string> process_affinities{};
        // Retrieve each process's name and assigned core within the replica.
        for (const auto &process : replica) {
          std::string process_name = process["name"];
          std::string cpu_name = process["core"];
          process_affinities.emplace(process_name, cpu_name);
        }
        replica_affinities.push_back(process_affinities);
      }
      // Store the region's name and its corresponding replicas' affinities.
      regions.emplace(mapping["name"], replica_affinities);
    }
  }

  // Retrieve the mapping's name.
  auto name = json_mapping["name"];

  // Retrieve all other attributes as characteristics of the mapping.
  for (const auto &item : json_mapping.items()) {
    const auto attribute = item.key();
    if (attribute != "name" && attribute != "mapping") {
      characteristics.emplace_back(attribute, json_mapping[attribute]);
    }
  }

  // Create and return the mapping object.
  return Mapping{platform, name, threads, regions, characteristics};
}

/**
 * \brief Read mappings from the given directory.
 *
 * This function reads and processes mapping files from a specified directory.
 * It parses each file and checks its validity against a knob description.
 * Valid mappings are added to a list and returned.
 *
 * \param dir The directory from which to read the mapping files.
 * \return A vector of valid Mapping objects.
 */
std::vector<Mapping> JsonMappingReader::read_mappings(const Platform &platform,
                                                      const std::string &dir) {
  // Prepare a container to store valid mappings
  std::vector<Mapping> mappings;

  try {
    // Define a path object representing the directory
    std::filesystem::path dir_path(dir);

    // Iterate over all entries in the directory
    for (const auto &entry : std::filesystem::directory_iterator(dir_path)) {
      // Ignore if the entry is not a regular file
      if (!entry.is_regular_file())
        continue;

      std::filesystem::path file = entry.path();

      // If the file has a '.json' extension and is not the knob description
      // file
      if (file.extension() == ".json" &&
          file.filename() != JsonMappingReader::kKnobDescFilename) {
        // Open the JSON mapping file
        std::ifstream json_mapping_file{file};

        // Parse the JSON file into a JSON object
        nlohmann::json json_mapping;
        json_mapping_file >> json_mapping;

        // Parse the JSON mapping into a Mapping object
        auto parsed_mapping = parse_mapping(platform, json_mapping);

        // Add mapping to the vector
        mappings.emplace_back(parsed_mapping);
      }
    }
  } catch (std::exception &e) {
    // Log any exceptions that occur during file reading or mapping parsing
    LOGGER->error("Reading mappings failed with: %s\n", e.what());
  }

  // Return the vector of valid mappings
  return mappings;
}

std::vector<Mapping>
CsvMappingReader::read_mappings(const Platform &platform,
                                const std::string &file_path) {
  // implement CSV reading here
  CSVData data{file_path};
  std::vector<Mapping> mappings;

  for (const auto &row : data.row_iter()) {
    std::vector<std::pair<std::string, std::string>> threads;
    RegionAffinities<std::string> regions{};
    std::vector<std::pair<std::string, std::string>> characteristics;

    for (const auto &col : row.names()) {
      if (string_util::starts_with(col, "t_")) {
        /* Columns starting with 't_' are interpreted as threads */
        std::string thread_name = col.substr(2);
        std::string cpu_name = row(col);

        threads.emplace_back(thread_name, cpu_name);
      } else {
        /* All the other columns are characteristics of the mapping */
        std::string value = row(col);

        characteristics.emplace_back(col, value);
      }
    }

    auto name = row.fixed();

    mappings.emplace_back(platform, name, threads, regions, characteristics);
  }

  return mappings;
}

/**
 * \brief Read mappings from given YAML file
 *
 * This function reads the YAML file which is expected to contain a mapping
 * template and list of mappings. The mapping template consists of processes,
 * regions, and metadata. Each mapping contains a name, list of processes,
 * regions and metadata.
 *
 * \param file_path File path to the YAML mapping file
 * \return Vector of Mapping objects
 */
std::vector<Mapping>
YamlMappingReader::read_mappings(const Platform &platform,
                                 const std::string &file_path) {
  // Load the root node from YAML file
  YAML::Node root = YAML::LoadFile(file_path);

  // Extract mapping template from root
  auto mapping_template_node = root["mapping_template"];

  // Extract template data
  auto template_processes =
      mapping_template_node["processes"].as<std::vector<std::string>>();
  auto template_regions =
      mapping_template_node["regions"]
          .as<std::map<std::string, std::vector<std::string>>>();
  auto template_metadata =
      mapping_template_node["metadata"].as<std::vector<std::string>>();

  // Initialize vector to hold all Mapping objects
  std::vector<Mapping> mappings;

  // Process each mapping node
  for (const auto &mapping_node : root["mappings"]) {
    auto mapping_name = mapping_node["name"].as<std::string>();

    // Map process threads
    std::vector<std::pair<std::string, std::string>> threads;

    auto process_cores =
        mapping_node["processes"].as<std::vector<std::string>>();
    if (template_processes.size() != process_cores.size()) {
      LOGGER->error(
          "Mismatch between number of template processes and process cores "
          "(%s)",
          file_path);
      return std::vector<Mapping>{};
    }

    for (size_t i = 0; i < template_processes.size(); ++i) {
      threads.emplace_back(template_processes[i], process_cores[i]);
    }

    // Map regions
    RegionAffinities<std::string> region_affinities;
    auto region_node = mapping_node["regions"];
    for (const auto &region : region_node) {
      ReplicaAffinities<std::string> replica_affinities;
      auto region_name = region.first.as<std::string>();
      auto processes_in_region = template_regions[region_name];
      for (const auto &replica : region.second) {
        ProcessAffinities<std::string> process_affinities;

        auto cores_for_replica = replica.as<std::vector<std::string>>();

        if (processes_in_region.size() != cores_for_replica.size()) {
          LOGGER->error(
              "Mismatch between number of processes and cores in replica (%s)",
              file_path);
          return std::vector<Mapping>{};
        }

        for (size_t i = 0; i < processes_in_region.size(); ++i) {
          process_affinities.emplace(processes_in_region[i],
                                     cores_for_replica[i]);
        }

        replica_affinities.push_back(process_affinities);
      }

      region_affinities.emplace(region_name, replica_affinities);
    }

    // Metadata
    std::vector<std::pair<std::string, std::string>> characteristics;
    auto mapping_metadata =
        mapping_node["metadata"].as<std::vector<std::string>>();

    if (template_metadata.size() != mapping_metadata.size()) {
      LOGGER->error(
          "Mismatch between number of template metadata and mapping metadata "
          "(%s)",
          file_path);
    }

    for (size_t i = 0; i < template_metadata.size(); ++i) {
      characteristics.emplace_back(template_metadata[i], mapping_metadata[i]);
    }

    // Create the mapping object and add it to the vector
    mappings.emplace_back(platform, mapping_name, threads, region_affinities,
                          characteristics);
  }

  return mappings;
}

/**
 * \brief Reads mapping data from a specified directory and returns a map that
 * associates each application with its vector of Mapping objects.
 *
 * The function traverses the provided base directory, reading mapping data from
 * files and directories within it. It supports mappings stored in CSV, YAML,
 * and JSON formats. For CSV and YAML, the mapping data must be in individual
 * files with the appropriate extension (.csv or .yaml). For JSON, the mapping
 * data must be in a directory with a descriptor file named as per
 * JsonMappingReader::kKnobDescFilename.
 *
 * \param base_dir The base directory containing the mapping data.
 * \return A map that associates each application's name with its vector of
 * Mapping objects.
 *
 * \throws This function might throw exceptions related to file system
 * operations (e.g., when the base_dir does not exist).
 */

std::map<std::string, std::vector<Mapping>>
MappingReader::read_mapping_directory(const std::string &base_dir) {
  std::map<std::string, std::vector<Mapping>> app_mappings;
  for (const auto &entry : std::filesystem::directory_iterator(base_dir)) {
    auto entryname = entry.path().filename().string();
    if (entry.is_regular_file()) {
      auto file_extension = entry.path().extension().string();
      auto application_name = entry.path().stem().string();
      if (app_mappings.count(application_name) > 0) {
        LOGGER->warning(
            "Mappings for the application '%s' have already been parsed; these "
            "will be replaced.\n",
            application_name);
      }
      if (file_extension == ".csv") {
        CsvMappingReader reader;
        app_mappings[application_name] =
            reader.read_mappings(_platform, entry.path().string());
      } else if (file_extension == ".yaml") {
        YamlMappingReader reader;
        app_mappings[application_name] =
            reader.read_mappings(_platform, entry.path().string());
      } else {
        LOGGER->warning("Unrecognized mapping format for '%s'.\n",
                        entryname.c_str());
      }
    } else if (entry.is_directory()) {
      auto application_name = entry.path().filename().string();
      auto knob_desc_path = entry.path() / JsonMappingReader::kKnobDescFilename;
      if (app_mappings.count(application_name) > 0) {
        LOGGER->warning(
            "Mappings for the application '%s' have already been parsed; these "
            "will be replaced.\n",
            application_name);
      }
      if (std::filesystem::exists(knob_desc_path)) {
        JsonMappingReader reader;
        app_mappings[application_name] =
            reader.read_mappings(_platform, entry.path().string());
      } else {
        LOGGER->warning("Unrecognized mapping format for the directory '%s'.\n",
                        entryname.c_str());
      }
    }
  }

  // Log details of the mappings read
  log_mappings_details(app_mappings);

  return app_mappings;
}

/**
 * \brief Logs the details of the mappings for all applications.
 *
 * This method prints information about the number of mappings, threads, and
 * characteristics found for each application.
 *
 * \param app_mappings Map of application names to a vector of valid Mapping
 * objects.
 */
void MappingReader::log_mappings_details(
    const std::map<std::string, std::vector<Mapping>> &app_mappings) {
  auto &allocator = _platform.GetEquivResAllocator();
  for (const auto &app_mapping : app_mappings) {
    const auto &mappings = app_mapping.second;

    // If mappings exist, extract thread names and characteristics for logging
    if (!mappings.empty()) {
      std::vector<std::string> thread_names;
      std::vector<std::string> characteristic_names;
      Mapping mapping = mappings.back();

      // Extract thread names from the last mapping
      for (const auto &key_val : mapping.thread_map)
        thread_names.push_back(key_val.first);

      // Extract thread names from regions in the last mapping
      for (const auto &[region_name, replicas] : mapping.region_map) {
        for (const auto &key_val : replicas.back()) {
          auto process_name = key_val.first;
          thread_names.push_back(region_name + "::" + key_val.first);
        }
      }

      // Extract characteristic names from the last mapping
      for (const auto &key_val : mapping.characteristics_map)
        characteristic_names.push_back(key_val.first);

      // Log the count and names of threads and characteristics found
      LOGGER->info(" -> found mapping for '%s'\n", app_mapping.first.c_str());
      LOGGER->debug("  * found %i mapping(s)\n", mappings.size());
      LOGGER->debug("  |-> %i thread(s): %s\n", thread_names.size(),
                    string_util::join(thread_names, ", ").c_str());
      LOGGER->debug("  |-> %i characteristic(s): %s\n",
                    characteristic_names.size(),
                    string_util::join(characteristic_names, ", ").c_str());

      // Log detailed information about each mapping
      for (const auto &m : mappings) {
        std::vector<std::string> mapping_characteristics;

        for (const auto &c : characteristic_names) {
          std::stringstream ss;
          ss << std::setprecision(0) << std::fixed << c << ":"
             << m.characteristic(c);
          mapping_characteristics.push_back(ss.str());
        }

        LOGGER->debug("  |=> %s [%s] %s\n", m.name.c_str(),
                      allocator.GetEquivClassName(m).c_str(),
                      string_util::join(mapping_characteristics, ", ").c_str());
      }
    }
  }
}

} /* namespace tetris */
