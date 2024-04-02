from typing import Dict

from client_py.utils.cpuSets import CPUThreadSet
from client_py.utils.platform import Platform
from proto.tetris_pb2 import ClientMessage


class OperatingPoint:
    def __init__(self, name: str = "default", characteristics: Dict[str, float] = {},
                 cpus: CPUThreadSet = CPUThreadSet(), cores_count: Dict[str, int] = {}):
        self.name = name
        self.characteristics = characteristics
        self.cpus = cpus
        self.cores_count = cores_count

    @classmethod
    def from_platform_and_op(cls, platform: Platform, op_data: ClientMessage.OperatingPointsInfo.OPData):
        # Implementation of this method would depend on the actual structure and usage of ClientMessage and OPData
        raise NotImplementedError

    def characteristic(self, criteria: str) -> float:
        if criteria in self.characteristics:
            return self.characteristics[criteria]
        raise RuntimeError("Unknown characteristic criteria.")

    def cores_count(self, core_type: str) -> int:
        if core_type in self.cores_count:
            return self.cores_count[core_type]
        raise RuntimeError("Unknown core type.")


class OperatingPointAllocation:
    def __init__(self, base: OperatingPoint = OperatingPoint(), cpu_allocation: Dict[int, int] = {}):
        self.base = base
        self.cpu_allocation = cpu_allocation

    def characteristic(self, criteria: str) -> float:
        return self.base.characteristic(criteria)

    def get_thread_set(self) -> CPUThreadSet:
        res = CPUThreadSet()
        cores = self.base.cpus.get_list()

        for c in cores:
            if c in self.cpu_allocation:
                res.set(self.cpu_allocation[c])
            else:
                res.set(c)
        return res
