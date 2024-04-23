#ifndef __PERF_H__
#define __PERF_H__

#pragma once

#include <filesystem>
#include <initializer_list>
#include <map>
#include <memory>
#include <semaphore>
#include <thread>
#include <tuple>
#include <vector>
#include <optional>


namespace tetris {
namespace perf {

class Handle {
protected:
  bool start(int fd);
  void close(int &fd);

public:
  virtual ~Handle() = default;
  virtual std::map<std::string, uint64_t> read() = 0;
};

using HandlePtr = std::unique_ptr<Handle>;

/* \brief The general interface to open a new perf session */
class Starter {
public:
  virtual ~Starter() = default;
  virtual HandlePtr new_process(int pid, const std::vector<uint64_t> &events) = 0;
};

using StarterPtr = std::unique_ptr<Starter>;

class PerfManager {
private:
  StarterPtr starter;

public:
  PerfManager();

  std::optional<HandlePtr> open(int pid);
};

} /* namespace perf */
} /* namespace tetris */


#endif /* ifndef __PERF_H__ */
