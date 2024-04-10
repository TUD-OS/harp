import yaml

from client_py.utils.mapping import Mapping


class YamlMappingReader:

    @staticmethod
    def read_mapping(yaml_file):
        mappings_list = []

        with open(yaml_file, 'r') as file:
            data = yaml.safe_load(file)

            for mapping_data in data['mappings']:
                name = mapping_data['name']
                cpu_ids = mapping_data['cores']
                execution_time = mapping_data['metadata'][0]
                energy = mapping_data['metadata'][1]

                mapping = Mapping(name=name, thread_affinities=cpu_ids, exec_time=execution_time, energy=energy)
                mappings_list.append(mapping)

        return mappings_list
