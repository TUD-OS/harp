from typing import List

import yaml

from client_py.utils.mapping import Mapping


class YamlMappingReader:

    @staticmethod
    def read_mappings(file_path: str) -> List[Mapping]:
        with open(file_path, 'r') as file:
            data = yaml.safe_load(file)

        mappings = []
        for item in data:
            name = item['identifier']
            thread_affinities = item['cpu_ids']
            exec_time = item['exec-time']
            energy = item['energy']

            mapping = Mapping(name, thread_affinities, exec_time, energy)
            mappings.append(mapping)

        return mappings
