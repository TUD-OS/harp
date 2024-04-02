from dataclasses import dataclass, field
from typing import List, Dict, Union

from client_py.utils.cpuSets import CPUCoreSet, CPUThreadSet
from client_py.utils.equivResAllocator import EquivResAllocator


# Define the CPUType class
@dataclass
class CPUType:
    name: str
    num_threads: int


# Define the CPUThread class
@dataclass
class CPUThread:
    core: 'CPUCore'
    name: str
    id: int


# Define the CPUCore class
@dataclass
class CPUCore:
    platform: 'Platform'
    type: CPUType
    id: int
    threads: List[CPUThread] = field(default_factory=list)


# Define the Platform class
@dataclass
class Platform:
    equiv_res_allocator: Union[EquivResAllocator, None] = None
    cpu_types: Dict[str, CPUType] = field(default_factory=dict)
    cpu_cores: List[CPUCore] = field(default_factory=list)
    cpu_threads: Dict[int, CPUThread] = field(default_factory=dict)

    # Methods
    def set_equiv_res_allocator(self, allocator: EquivResAllocator):
        self.equiv_res_allocator = allocator

    def get_equiv_res_allocator(self) -> EquivResAllocator:
        return self.equiv_res_allocator

    def find_cpu_thread_by_index(self, index: int) -> Union[CPUThread, None]:
        return self.cpu_threads.get(index)

    def find_cpu_thread_by_name(self, name: str) -> Union[CPUThread, None]:
        for thread in self.cpu_threads.values():
            if thread.name == name:
                return thread
        return None

    def find_cpu_core(self, index: int) -> Union[CPUCore, None]:
        if 0 <= index < len(self.cpu_cores):
            return self.cpu_cores[index]
        return None

    def get_cpu_cores(self) -> List[CPUCore]:
        return self.cpu_cores

    def get_cpu_cores_by_set(self, core_set: 'CPUCoreSet') -> List[CPUCore]:
        cores = []
        for index in core_set:
            if 0 <= index < len(self.cpu_cores):
                cores.append(self.cpu_cores[index])
            else:
                raise IndexError("Invalid CPU core index encountered.")
        return cores

    def get_cpu_threads_by_set(self, thread_set: 'CPUThreadSet') -> List[CPUThread]:
        threads = []
        for index in thread_set:
            thread = self.find_cpu_thread_by_index(index)
            if thread is None:
                raise IndexError("Invalid CPU thread affinity encountered.")
            threads.append(thread)
        return threads

    def to_cpu_core_set(self, thread_set: 'CPUThreadSet') -> 'CPUCoreSet':
        core_set = CPUCoreSet()
        for thread in self.get_cpu_threads_by_set(thread_set):
            core_set.add(thread.core.id)
        return core_set

    def to_cpu_thread_set(self, core_set: 'CPUCoreSet') -> 'CPUThreadSet':
        thread_set = CPUThreadSet()
        for core in self.get_cpu_cores_by_set(core_set):
            for thread in core.threads:
                thread_set.add(thread.id)
        return thread_set

    def get_core_count_per_type(self) -> Dict[str, int]:
        core_count = {name: 0 for name in self.cpu_types}
        for core in self.cpu_cores:
            core_count[core.type.name] += 1
        return core_count

    def get_core_count_per_type_by_set(self, core_set: 'CPUCoreSet') -> Dict[str, int]:
        core_count = {name: 0 for name in self.cpu_types}
        for core in self.get_cpu_cores_by_set(core_set):
            core_count[core.type.name] += 1
        return core_count

    # Private methods
    def add_cpu_type(self, name: str, num_threads: int):
        self.cpu_types[name] = CPUType(name, num_threads)

    def get_cpu_type(self, name: str) -> CPUType:
        return self.cpu_types[name]

    def add_core(self, core_type: str) -> CPUCore:
        cpu_type_ = self.get_cpu_type(core_type)
        core = CPUCore(self, cpu_type_, len(self.cpu_cores))
        self.cpu_cores.append(core)
        return core

    def register_thread(self, index: int, thread: CPUThread):
        if index in self.cpu_threads:
            raise RuntimeError("Several CPU Threads have the same affinity.")
        self.cpu_threads[index] = thread
