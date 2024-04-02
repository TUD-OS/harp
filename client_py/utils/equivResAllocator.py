from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional, Dict

from client_py.utils.cpuSets import CPUCoreSet, CPUThreadSet
from client_py.utils.mapping import Mapping
from client_py.utils.operatingPoint import OperatingPoint, OperatingPointAllocation


class EquivResAllocator(ABC):

    @abstractmethod
    def get_equiv_class_name(self, cpus) -> str:
        pass

    @abstractmethod
    def find_equiv_mapping(self, m: Mapping, used_cpus: CPUCoreSet) -> Optional[Mapping]:
        pass

    @abstractmethod
    def find_equiv_op(self, op: OperatingPoint, used_cpus: CPUCoreSet) -> Optional[OperatingPointAllocation]:
        pass

    def get_equiv_class_name_from_mapping(self, map: Mapping) -> str:
        return self.get_equiv_class_name(map.cpus)

    def get_equiv_class_name_from_op(self, op: OperatingPoint) -> str:
        return self.get_equiv_class_name(op.cpus)

    def get_equiv_class_name_from_op_allocation(self, op: OperatingPointAllocation) -> str:
        return self.get_equiv_class_name(op.base)

    @abstractmethod
    def get_equiv_class_name(self, threads: CPUThreadSet) -> str:
        pass


# Define the CoreTypeBasedEquivResAllocator class
@dataclass
class CoreTypeBasedEquivResAllocator(EquivResAllocator):

    def get_equiv_class_name(self, core_set: CPUCoreSet) -> str:
        raise NotImplementedError

    def get_equiv_class_name_from_mapping(self, map: Mapping) -> str:
        return self.get_equiv_class_name(map.cpus)

    def get_equiv_class_name_from_op(self, op: OperatingPoint) -> str:
        return self.get_equiv_class_name(op.cpus)

    def find_equiv_mapping(self, m: Mapping, used_cpus: CPUCoreSet) -> Optional[Mapping]:
        raise NotImplementedError

    def find_equiv_op(self, op: OperatingPoint, used_cpus: CPUCoreSet) -> Optional[OperatingPointAllocation]:
        raise NotImplementedError

    def generate_core_permutation(self, core_set1: CPUCoreSet, core_set2: CPUCoreSet) -> Optional[Dict[int, int]]:
        pass

    def to_thread_permutation(self, core_permutation: Dict[int, int]) -> Dict[int, int]:
        pass
