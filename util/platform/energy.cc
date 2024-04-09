#include "util/platform/energy.h"

#include "util/debug_util.h"

#include <fstream>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/perf_event.h>


namespace tetris {

PerfMeasure::PerfMeasure() : _running{false}, _fd{-1}, _scale{0}
{
  setup();
}

PerfMeasure::~PerfMeasure()
{
  shutdown();
}

void PerfMeasure::setup()
{
  if (_running)
    return;

  /* Get the event type */
  FILE *etype_file = fopen("/sys/bus/event_source/devices/power/type", "r");
  int event_type =  0;
  if (fscanf(etype_file, "%d", &event_type) != 1) {
    LOGGER->warning("Failed to parse perf event type\n");
    fclose(etype_file);
    return;
  }


  /* Get the event sub type for PKG energy */
  FILE *stype_file = fopen("/sys/bus/event_source/devices/power/events/energy-pkg", "r");
  int event_sub_type = 0;
  if (fscanf(stype_file, "event=%x", &event_sub_type) != 1) {
    LOGGER->warning("Failed to parse the perf sub event type\n");
    fclose(stype_file);
    return;
  }
  fclose(stype_file);

  /* Get the scale for the values */
  FILE *scale_file = fopen("/sys/bus/event_source/devices/power/events/energy-pkg.scale", "r");
  double scale = 0;
  if (fscanf(scale_file, "%le", &scale) != 1) {
    LOGGER->warning("Failed to parse perf value scale\n");
    fclose(scale_file);
    return;
  }

  _scale = scale * 1000000.0;
  LOGGER->debug("Perf energy values are scaled by %le --> for uJ we use %le\n", scale, _scale);
  
  /* Now initialize the perf event for the RAPL counters */
  struct perf_event_attr pea;
  memset(&pea, 0, sizeof(pea));

  pea.size = sizeof(struct perf_event_attr);
  pea.type = event_type;
  pea.config = event_sub_type;
  pea.disabled = 1;
  pea.exclude_kernel = 0;

  int tmp_fd = syscall(__NR_perf_event_open, &pea, -1, 0, -1, 0);
  if (tmp_fd == -1) {
    LOGGER->error("Can't start perf measurements!\n");
    return;
  } else {
    _fd = tmp_fd;

    if(ioctl(_fd, PERF_EVENT_IOC_RESET, 0) < 0) {
      LOGGER->error("Failed to reset perf measurements!\n");
    }
    if(ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
      LOGGER->error("Failed to enable perf measurements!\n");
      return;
    }
  }

  _running = true;
}

void PerfMeasure::shutdown()
{
  if (!_running)
    return;

  close(_fd);
  _fd = -1;
  _running = false;
}

uint64_t PerfMeasure::read()
{
  if (!_running)
      return 0;

  unsigned long val;
  if (::read(_fd, &val, sizeof(val)) == -1) {
    LOGGER->error("Failed to read RAPL values from perf\n");
    return 0;
  }

  /* Scale the values to uJ */
  return val * _scale;
}

PowercapMeasure::PowercapMeasure() :
  _running{false}, _last{0}, _total{0}
{
  setup();
}

void PowercapMeasure::setup()
{
  if (_running)
    return;

  std::ifstream pfile{"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj"};
  pfile >> _last;
  _running = true;
}

uint64_t PowercapMeasure::read()
{
  uint64_t cur;

  if (_running) {
      std::ifstream pfile{"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj"};
      pfile >> cur;

      if (cur < _last) {
          std::ifstream max_file{"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/max_energy_range_uj"};
          uint64_t max_value;
          max_file >> max_value;

          _total += max_value - _last + cur;
      } else {
          _total += cur - _last;
      }

      _last = cur;
  }

  return _total;
}

OdroidMeasure::OdroidMeasure() : _running{false}
{
  setup();
}

OdroidMeasure::~OdroidMeasure()
{
  shutdown();
}

void OdroidMeasure::setup()
{
  if (_running)
      return;

  int i = 0;
  for (const auto &s : sensors) {
    std::ofstream enable{s + "/enable"};
    enable << "1";
    std::ifstream joules{s + "/sensor_J"};
    joules >> _last_values[i];
    i++;
  }

  _running = true;
}

void OdroidMeasure::shutdown()
{
  if (!_running)
    return;

  for (const auto &s : sensors) {
    std::ofstream enable{s + "/enable"};
    enable << "0";
  }

  _running = false;
}

uint64_t OdroidMeasure::read()
{
    uint64_t result = 0;

    if (_running) {
        std::array<float, 3> cur_values;
        int i = 0;
        for (const auto &s : sensors) {
            std::ifstream joules{s + "/sensor_J"};
            joules >> cur_values[i];
            i++;
        }

        result += (cur_values[0] - _last_values[0]) * 1000000;
        result += (cur_values[1] - _last_values[1]) * 1000000;
        result += (cur_values[2] - _last_values[2]) * 1000000;
    }

    return result;
}

} /* namespace tetris */
