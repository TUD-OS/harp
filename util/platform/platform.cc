#include "util/platform/platform.h"


namespace tetris {

int CPUThread::GetPowerCoefficient() const {
  return _core.GetType().GetPowerCoefficient();
}

std::vector<CPUThread*> CPUThread::GetSiblings() const {
    std::vector<CPUThread*> res;
    for (auto &t : _core.GetCPUThreads())
      if (t->GetID() != _id)
        res.push_back(t);

    return res;
}

void CPUCore::AddThread(const std::string &name, int affinity) {
  auto cpu_thread = std::make_unique<CPUThread>(*this, name, affinity);
  auto raw_ptr = cpu_thread.get();
  _threads.push_back(std::move(cpu_thread));
  _platform.RegisterThread(affinity, raw_ptr);
}

} /* namespace tetris */
