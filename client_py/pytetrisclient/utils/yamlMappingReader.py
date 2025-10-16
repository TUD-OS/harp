import yaml

from pytetrisclient.utils.mapping import Mapping


class YamlMappingReader:

    @staticmethod
    def read_mappings(file_path, platform):
        mappings_list = []

        with open(file_path, "r") as file:
            data = yaml.safe_load(file)
            app_name = data["application"]
            metadata_order = data.get("mapping_template", {}).get("metadata", [])

            if "mappings" in data and data["mappings"] is not None:
                for mapping_data in data["mappings"]:
                    name = mapping_data["name"]
                    threads = mapping_data["cores"]
                    cpu_ids = platform.get_affinities_of_threads(threads)
                    execution_time = mapping_data["metadata"][0]
                    energy = mapping_data["metadata"][1]

                    # Create a dictionary for metadata using the specified order
                    metadata_dict = {}
                    for index, metadata_field in enumerate(metadata_order):
                        metadata_dict[metadata_field] = mapping_data["metadata"][index]

                    mapping = Mapping(
                        name=name,
                        thread_affinities=cpu_ids,
                        characteristics=metadata_dict,
                    )
                    mappings_list.append(mapping)
        return mappings_list, app_name
