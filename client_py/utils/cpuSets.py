from typing import Set, List


class CPUSetBase:
    def __init__(self):
        self._set: Set[int] = set()

    def __eq__(self, o: 'CPUSetBase') -> bool:
        return self._set == o._set

    def __ne__(self, o: 'CPUSetBase') -> bool:
        return not self.__eq__(o)

    def __and__(self, o: 'CPUSetBase') -> 'CPUSetBase':
        tmp = self.__class__()
        for c in self._set:
            if c in o._set:
                tmp._set.add(c)
        return tmp

    def __iand__(self, o: 'CPUSetBase') -> 'CPUSetBase':
        for c in self._set.copy():
            if c not in o._set:
                self._set.remove(c)
        return self

    def __or__(self, o: 'CPUSetBase') -> 'CPUSetBase':
        tmp = self.__class__()
        tmp._set = self._set.union(o._set)
        return tmp

    def __ior__(self, o: 'CPUSetBase') -> 'CPUSetBase':
        self._set = self._set.union(o._set)
        return self

    def __xor__(self, o: 'CPUSetBase') -> 'CPUSetBase':
        self.__class__()
        for c in o._set:
            if c in self._set:
                self._set.remove(c)
            else:
                self._set.add(c)
        return self

    def __ixor__(self, o: 'CPUSetBase') -> 'CPUSetBase':
        for c in o._set:
            if c in self._set:
                self._set.remove(c)
            else:
                self._set.add(c)
        return self

    def __contains__(self, core_id: int) -> bool:
        return core_id in self._set

    def set(self, core_id: int) -> None:
        self._set.add(core_id)

    def erase(self, core_id: int) -> None:
        self._set.discard(core_id)

    def zero(self) -> None:
        self._set.clear()

    def size(self) -> int:
        return len(self._set)

    def overlaps_with(self, o: 'CPUSetBase') -> bool:
        return bool(self & o)

    def get_list(self) -> List[int]:
        return list(self._set)

    def __iter__(self):
        return iter(self._set)


class CPUCoreSet(CPUSetBase):
    pass


class CPUThreadSet(CPUSetBase):

    def __init__(self):
        super().__init__()

    def to_cpu_set_t(self) -> set:
        cpu_set = set()
        for c in self._set:
            cpu_set.add(c)
        return cpu_set
