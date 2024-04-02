from typing import List

import yaml

from client_py.utils.cpuSets import CPUThreadSet
from client_py.utils.mapping import Mapping
from client_py.utils.platform import Platform


class YamlMappingReader:

    @staticmethod
    def read_mappings(platform: Platform, file_path: str) -> List[Mapping]:
        with open(file_path, 'r') as file:
            root = yaml.safe_load(file)

        application_name = root["application"]
        mappings = root["mappings"]

        parsed_mappings = []

        for idx, mapping in enumerate(mappings):
            cores = mapping["cores"]
            threads = CPUThreadSet()
            for core in cores:
                core_id = int(core[1:])
                threads.set(core_id)
            metadata = mapping["metadata"]
            name = mapping["name"]
            exec_time = metadata["execution_time"]
            energy = metadata["energy"]
            nr_threads = metadata["nr_threads"]
            parsed_mappings.append(
                Mapping(name=name, platform=platform, threads=threads, energy=energy, exec_time=exec_time,
                        nr_threads=nr_threads)
            )

        return parsed_mappings
