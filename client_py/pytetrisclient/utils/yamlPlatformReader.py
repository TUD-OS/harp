import yaml

from pytetrisclient.utils.platform import Platform


class YamlPlatformReader:
    @staticmethod
    def read_platform(file_path):
        platform = Platform()

        with open(file_path, 'r') as file:
            data = yaml.safe_load(file)

            for core_data in data['cores']:
                for thread in core_data["threads"]:
                    platform.add_thread_affinity_mapping(thread["name"], thread["affinity"])

        return platform
