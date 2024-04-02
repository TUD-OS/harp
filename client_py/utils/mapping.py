from dataclasses import dataclass

from client_py.utils.cpuSets import CPUThreadSet
from client_py.utils.platform import Platform


@dataclass
class Mapping:
    def __init__(self,
                 name,
                 platform: Platform,
                 threads: CPUThreadSet,
                 exec_time,
                 energy,
                 nr_threads,
                 ):
        self.platform = platform
        self.name = name
        self.characteristics = {
            "threads": threads,
            "exec_time": exec_time,
            "energy": energy,
            "nr_threads": nr_threads
        }

    '''
        self.thread_map = {k: int(v) for k, v in threads}
        self.region_map = {k: [{k2: int(v2)} for k2, v2 in v] for k, v in regions.items()}
        self.cpus = CPUThreadSet()  # Placeholder for CPUThreadSet instantiation

    def cpu(self, thread: str) -> CPUThreadSet:
        if thread in self.thread_map:
            return CPUThreadSet({self.thread_map[thread]})
        else:
            return self.cpus

    def characteristic(self, criteria: str) -> float:
        if criteria in self.characteristics_map:
            return self.characteristics_map[criteria]
        else:
            raise RuntimeError("Unknown characteristic criteria.")

    def __eq__(self, other: 'Mapping') -> bool:
        return (self._platform == other._platform and
                self.name == other.name and
                self.thread_map == other.thread_map and
                self.region_map == other.region_map and
                self.characteristics_map == other.characteristics_map and
                self.cpus == other.cpus)

    def op(self) -> OperatingPoint:
        # Placeholder for OperatingPoint instantiation
        return OperatingPoint()

    def __str__(self) -> str:
        return (f"Mapping(name={self.name}, "
                f"thread_map={self.thread_map}, "
                f"region_map={self.region_map}, "
                f"characteristics_map={self.characteristics_map}, "
                f"cpus={self.cpus})")

    def __repr__(self) -> str:
        return self.__str__()
    '''
