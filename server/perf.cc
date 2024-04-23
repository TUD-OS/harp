#include "server/perf.h"

#include <util/string_util.h>
#include <util/debug_util.h>

#include <filesystem>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <exception>
#include <optional>

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>


namespace fs = std::filesystem;

namespace tetris {
namespace perf {

enum Events : uint64_t {
  Instructions = PERF_COUNT_HW_INSTRUCTIONS,
  CacheMisses = PERF_COUNT_HW_CACHE_MISSES
};

static const std::map<uint64_t, std::string> EventNames = {{Events::Instructions, "Instructions"},
                                                           {Events::CacheMisses, "Cache-Misses"}};

static const std::vector<uint64_t> EventList = { Events::Instructions, Events::CacheMisses };


static std::optional<uint64_t> start_perf(uint64_t type, uint64_t event, int pid, int &group_fd)
{
  struct perf_event_attr pea;
  memset(&pea, 0, sizeof(pea));

  pea.size = sizeof(pea);
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  pea.type = type;
  pea.config = event;

  auto tmp_fd = syscall(SYS_perf_event_open, &pea, pid, -1, group_fd, 0);
  if (tmp_fd == -1) {
    /* Something went wrong when opening the performance monitoring! */
    LOGGER->error(" -> Failed to enable perf tracing for event %llu for client %i\n",
            event, pid);
    return std::nullopt;
  }

  if (group_fd == -1)
    group_fd = tmp_fd;

  uint64_t id;
  if (ioctl(tmp_fd, PERF_EVENT_IOC_ID, &id)) {
    LOGGER->warning(" -> Failed to get id for event %llu for client %i\n",
            event, pid);
    return std::nullopt;
  }

  return id;
}

struct perf_format {
  uint64_t nr;
  struct {
    uint64_t value;
    u_int64_t id;
  } values[];
};

static std::optional<std::map<uint64_t, uint64_t>> perf_read(int fd)
{
  if (fd == -1) {
    LOGGER->debug("Perf not properly initialized for client\n");
    return std::nullopt;
  }

  char buf[4096];
  struct perf_format *formatted_buf = reinterpret_cast<struct perf_format*>(buf);

  if (read(fd, buf, sizeof(buf)) == -1) {
    LOGGER->warning("Failed to read values from perf.\n");
    return std::nullopt;
  }

  /* Extract the data from perf */
  std::map<uint64_t, uint64_t> data;

  for (uint64_t i = 0; i < formatted_buf->nr; ++i) {
    data[formatted_buf->values[i].id] = formatted_buf->values[i].value;
  }

  return data;
}


bool Handle::start(int fd) {
  /* Start the perf monitoring for all the grouped events */
  if (ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP)) {
    LOGGER->warning(" -> Failed to reset perf counters\n");
    return false;
  }
  if (ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP)) {
    LOGGER->warning(" -> Failed to enable perf counters\n");
    return false;
  }

  return true;
}

void Handle::close(int &fd) {
  if (fd != -1) {
    ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    ::close(fd);
    fd = -1;
  }
}


class SinglePMUHandle : public Handle {
private:
  int fd;
  std::map<uint64_t, std::string> event_ids;

public:
  SinglePMUHandle(int fd, const std::map<uint64_t, std::string> &event_ids)
    : fd{fd}, event_ids{event_ids} {
    if (!start(fd))
      LOGGER->warning("Failed to start perf monitoring on fd: %d\n", fd);
  }

  SinglePMUHandle(const SinglePMUHandle &o) = delete;

  SinglePMUHandle(SinglePMUHandle &&o) :
      fd{o.fd}, event_ids{std::move(o.event_ids)} {
    o.fd = -1;
  }

  ~SinglePMUHandle() {
    close(fd);
  }

  std::map<std::string, uint64_t> read() override {
    std::map<std::string, uint64_t> result;

    if (auto raw_data = perf_read(fd)) {
      for (auto &[id, val] : raw_data.value()) {
        if (!this->event_ids.contains(id))
          LOGGER->warning("Unknown event %llu occurred (value: %llu) on fd: %d\n", id, val, fd);
        else
          result[this->event_ids[id]] = val;
      }
    } else {
        LOGGER->warning("Reading perf values failed on fd: %d\n", fd);
    }

    return result;
  }
};

class SinglePMU : public Starter {
public:
  HandlePtr new_process(int pid, const std::vector<uint64_t> &events) override
  {
    int perf_fd = -1;
    std::map<uint64_t, std::string> event_ids;

    for (auto event : events) {
      if (auto id = start_perf(PERF_TYPE_HARDWARE, event, pid, perf_fd)) {
        event_ids.insert({id.value(), EventNames.at(event)});
      } else {
        LOGGER->warning("Failed to monitor perf event %s for client %d\n",
                EventNames.at(event).c_str(), pid);
      }
    }

    if (perf_fd != -1) {
      LOGGER->debug("Successfully started perf monitoring for client %d --> fd %d\n", pid, perf_fd);
      return std::make_unique<SinglePMUHandle>(perf_fd, event_ids);
    } else {
      LOGGER->warning("Failed to setup perf monitoring for client %d\n", pid);
      return {};
    }
  }
};


class MultiPMUHandle : public Handle {
private:
  std::vector<std::pair<int, std::map<uint64_t, std::string>>> pmu_events;

public:
  MultiPMUHandle(const std::vector<std::pair<int, std::map<uint64_t, std::string>>> &pmu_events)
    : pmu_events{pmu_events} {
    for (auto &[fd, ids] : pmu_events) {
      if (!start(fd))
        LOGGER->warning("Failed to start perf monitoring on fd: %d\n", fd);
    }
  }

  MultiPMUHandle(const MultiPMUHandle &o) = delete;

  MultiPMUHandle(MultiPMUHandle &&o) :
      pmu_events{std::move(o.pmu_events)} {}

  ~MultiPMUHandle() {
    for (auto &[fd, ids] : pmu_events) {
      close(fd);
    }
  }

  std::map<std::string, uint64_t> read() override {
    std::map<std::string, uint64_t> result;

    for (auto &[fd, ids] : pmu_events) {
      if (auto raw_data = perf_read(fd)) {
        auto data = raw_data.value();
        /* We get the raw reading from perf with id -> value. We now have to combine
         * reading from the different PMUs that are of the same type */
        for (const auto &[id, name] : ids) {
          if (!result.contains(name))
            result[name] = 0;
          if (!data.contains(id))
            LOGGER->warning("Missing value in perf results for %s on fd: %d\n", name.c_str(), fd);
          else
            result[name] += data[id];
        }
      }
    }

    return result;
  }
};


class PMU {
private:
  std::string _name;
  uint64_t _type;

public:
  PMU(fs::path path) {
    _name = path.filename().string();

    std::ifstream type_file(path / "type");
    if (!type_file.is_open()) {
      LOGGER->error("Failed to open 'type' file for PMU %s\n", _name.c_str());
      _type = 0;
    } else {
      type_file >> _type;
      type_file.close();

      LOGGER->debug("Enabled Perf PMU '%s' with type '%llu'\n", _name.c_str(), _type);
    }
  }

  std::string name() const {
    return _name;
  }

  uint64_t perf_event_type(uint64_t event) {
    return (_type << 32) | event;
  }
};

class MultiPMU : public Starter {
private:
  std::vector<PMU> pmus;

public:
  MultiPMU(const std::vector<std::filesystem::path> pmu_paths) {
    for (auto &path : pmu_paths) {
      pmus.emplace_back(path);
    }
  }

  HandlePtr new_process(int pid, const std::vector<uint64_t> &events) override {
    std::vector<std::pair<int, std::map<uint64_t, std::string>>> pmu_events;

    for (auto &pmu : pmus) {
      int perf_fd = -1;
      std::map<uint64_t, std::string> event_ids;

      for (auto &event : events) {
         if (auto id = start_perf(PERF_TYPE_HARDWARE, pmu.perf_event_type(event), pid, perf_fd)) {
          LOGGER->debug(" -> [%s] Registered event %s for client %d\n", pmu.name().c_str(),
                  EventNames.at(event).c_str(), pid);

          event_ids.insert({id.value(), EventNames.at(event)});
        } else {
          LOGGER->warning(" -> [%s] Failed to register event %s for client %d\n", pmu.name().c_str(),
                  EventNames.at(event).c_str(), pid);
        }
      }
      if (perf_fd != -1) {
        LOGGER->debug("Successfully started perf monitoring for client %d on PMU %s--> fd %d\n", pid, pmu.name().c_str(), perf_fd);
        pmu_events.push_back(std::make_pair(perf_fd, event_ids));
      }
    }

    if (pmu_events.size() != 0) {
      return std::make_unique<MultiPMUHandle>(pmu_events);
    } else {
      LOGGER->warning("Failed to setup perf monitoring for client %d\n", pid);
      return {};
    }
  }
};


PerfManager::PerfManager() : starter{nullptr}
{
  /*
   * Figure out if we are running on a heterogeneous system, because then we need a different perf starter type:
   * If there is only a /sys/devices/cpu/ directory in the sysfs, we have only one PMU, but if there are
   * multiple directories of the form /sys/devices/cpu_* in the sysfs, we have to simultaniously drive multiple PMUs.
   */
  if (fs::exists("/sys/devices/cpu")) {
    starter = std::make_unique<SinglePMU>();
  } else {
    std::vector<fs::path> pmus;
    for (const auto &p : fs::directory_iterator("/sys/devices")) {
      if (string_util::starts_with(p.path().filename().string(), "cpu"))
        pmus.push_back(p);
    }

    starter = std::make_unique<MultiPMU>(pmus);
  }
}

std::optional<HandlePtr> PerfManager::open(int pid)
{
  try {
    auto handle = starter->new_process(pid, EventList);

    return std::move(handle);
  } catch (std::exception &e) {
    LOGGER->warning("Registering perf handle failed for PID %d failed with: %s\n", pid, e.what());
  }

  return std::nullopt;
}

} /* namespace perf */
} /* namespace tetris */
