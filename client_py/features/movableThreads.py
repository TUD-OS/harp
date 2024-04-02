import threading
from logging import Logger
from typing import Dict

from client_py.mappingFeature import MappingFeature
from client_py.utils.cpuSets import CPUThreadSet
from client_py.utils.mapping import Mapping as MappingUpdate
from proto.tetris_pb2 import ClientMessage

# Accessing OperatingPointsInfo
MappingsInfo = ClientMessage.OperatingPointsInfo


class MovableThreads(MappingFeature):
    """
    MovableThreads class.
    """

    class ThreadInfo:
        """
        ThreadInfo class to store information about a thread.
        """

        def __init__(self, name: str, tid: int, managed: bool, named: bool = False):
            self.name = name
            self.tid = tid
            self.cpu = 0
            self.managed = managed
            self.named = named
            self.assigned = False

    def __init__(self):
        """
        Constructor for MovableThreads class.
        """
        super().__init__()
        self._logger = Logger("movable threads logger")
        self._threads = []
        self._mtx = threading.Lock()
        self._active_mapping = None
        self._available_cpus = CPUThreadSet

    def need_handshake(self) -> bool:
        """F
        Override method from MappingFeature interface.
        """
        return False

    def mapping_update(self, mapping: MappingUpdate, conv: Dict[int, int]):
        """
        Override method from MappingFeature interface.
        """
        pass  # Implement mapping_update method logic here

    def extend_mapping(self, mappings: MappingsInfo) -> bool:
        """
        Override method from MappingFeature interface.
        """
        pass  # Implement extend_mapping method logic here

    def register_thread(self, tid: int, name: str = "") -> bool:
        """
        Register a thread.

        :param tid: Thread ID.
        :param name: Thread name.
        :return: True if successful, False otherwise.
        """
        with self._mtx:
            if name:
                thread_info = self.ThreadInfo(name, tid, True, True)
            else:
                thread_info = self.ThreadInfo(str(tid), tid, True)
            self._threads.append(thread_info)
        return True  # Implement logic to return False if failed

    def unregister_thread(self, tid: int) -> bool:
        """
        Unregister a thread.

        :param tid: Thread ID.
        :return: True if successful, False otherwise.
        """
        with self._mtx:
            for thread_info in self._threads:
                if thread_info.tid == tid:
                    self._threads.remove(thread_info)
                    return True
        return False

    def map_thread(self, t: ThreadInfo):
        """
        Map a thread to a CPU.

        :param t: ThreadInfo object.
        """
        pass  # Implement map_thread method logic here

    def move_thread(self, t: ThreadInfo, cpu: int) -> bool:
        """
        Move a thread to a specific CPU.

        :param t: ThreadInfo object.
        :param cpu: CPU number.
        :return: True if successful, False otherwise.
        """
        pass  # Implement move_thread method logic here

    def move_threads(self, t: ThreadInfo, cpus: CPUThreadSet) -> bool:
        """
        Move a thread to a set of CPUs.

        :param t: ThreadInfo object.
        :param cpus: Set of CPUs.
        :return: True if successful, False otherwise.
        """
        pass  # Implement move_thread method logic here
