import yaml

from client_py.utils.equivResAllocator import CoreTypeBasedEquivResAllocator
from platform import Platform


class YamlPlatformReader:
    @staticmethod
    def read_from_file(filename: str) -> Platform:
        with open(filename, 'r') as file:
            platform_data = yaml.safe_load(file)

        platform = Platform()

        # Read CPU Types
        if "core_types" in platform_data:
            for node in platform_data["core_types"]:
                name = node["type"]
                threads = node["threads"]
                platform.add_cpu_type(name, threads)

        # Read Cores
        if "cores" in platform_data:
            for node in platform_data["cores"]:
                type_ = node["type"]
                core = platform.add_core(type_)

                for thread_node in node["threads"]:
                    name = thread_node["name"]
                    affinity = node["affinity"]
                    core.add_thread(name, affinity)

        # Read equivalence scheme
        if "equivalence_scheme" in platform_data:
            scheme = platform_data["equivalence_scheme"]

            if scheme == "core-type":
                platform.set_equiv_res_allocator(CoreTypeBasedEquivResAllocator())
            else:
                raise RuntimeError("Unknown Equivalent Resource Allocator")

        return platform
