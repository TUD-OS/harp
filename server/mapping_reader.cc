#include "mapping_reader.h"
#include <filesystem>

Mapping JsonMappingReader::read_mapping(const std::string& file_path)
{
    // implement JSON reading here
}

std::vector<Mapping> CsvMappingReader::read_mappings(const std::string& file_path)
{
    // implement CSV reading here
}

std::vector<Mapping> YamlMappingReader::read_mappings(const std::string& file_path)
{
    // implement YAML reading here
}

std::map<std::string, std::vector<Mapping>> MappingReader::read_mapping_directory(const std::string& base_dir)
{
    std::map<std::string, std::vector<Mapping>> app_mappings;
    for (const auto& entry : std::filesystem::directory_iterator(base_dir))
    {
        if (entry.is_regular_file())
        {
            auto file_extension = entry.path().extension().string();
            auto application_name = entry.path().stem().string();
            if (file_extension == ".csv")
            {
                CsvMappingReader reader;
                app_mappings[application_name] = reader.read_mappings(entry.path().string());
            }
            else if (file_extension == ".yaml")
            {
                YamlMappingReader reader;
                app_mappings[application_name] = reader.read_mappings(entry.path().string());
            }
        }
        else if (entry.is_directory())
        {
            auto application_name = entry.path().filename().string();
            for (const auto& json_file : std::filesystem::directory_iterator(entry))
            {
                if (json_file.path().extension().string() == ".json")
                {
                    JsonMappingReader reader;
                    auto mapping = reader.read_mapping(json_file.path().string());
                    app_mappings[application_name].push_back(mapping);
                }
            }
        }
    }
    return app_mappings;
}

