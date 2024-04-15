import yaml

from client_py.utils.mapping import Mapping


class YamlMappingReader:

    @staticmethod
    def read_mappings(file_path, platform):
        mappings_list = []

        with open(file_path, 'r') as file:
            data = yaml.safe_load(file)

            for mapping_data in data['mappings']:
                name = mapping_data['name']
                threads = mapping_data['cores']
                cpu_ids = platform.get_affinities_of_threads(threads)
                execution_time = mapping_data['metadata'][0]
                energy = mapping_data['metadata'][1]

                mapping = Mapping(name=name, thread_affinities=cpu_ids, exec_time=execution_time, energy=energy)
                mappings_list.append(mapping)

        return mappings_list
