#ifndef MAPPING_READER_H
#define MAPPING_READER_H

#include <map>
#include <vector>
#include <string>
#include "mapping.h"

class JsonMappingReader
{
public:
    Mapping read_mapping(const std::string& file_path);
};

class CsvMappingReader
{
public:
    std::vector<Mapping> read_mappings(const std::string& file_path);
};

class YamlMappingReader
{
public:
    std::vector<Mapping> read_mappings(const std::string& file_path);
};

class MappingReader
{
public:
    static std::map<std::string, std::vector<Mapping>> read_mapping_directory(const std::string& base_dir);
};

#endif // MAPPING_READER_H
