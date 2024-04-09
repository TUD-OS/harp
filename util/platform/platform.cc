#include "util/platform/platform.h"


namespace tetris {

int CPUThread::GetPowerCoefficient() const {
  return _core.GetType().GetPowerCoefficient();
}

void CPUCore::AddThread(const std::string &name, int affinity) {
  auto cpu_thread = std::make_unique<CPUThread>(*this, name, affinity);
  auto raw_ptr = cpu_thread.get();
  _threads.push_back(std::move(cpu_thread));
  _platform.RegisterThread(affinity, raw_ptr);
}

} /* namespace tetris */
