#ifndef __ENERGY_H__
#define __ENERGY_H__

#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace tetris {

class Measure
{
public:
  virtual ~Measure() = default;
  virtual uint64_t read() = 0;

private:
  virtual void setup() = 0;
  virtual void shutdown() = 0;
};

class PerfMeasure : public Measure
{
private:
  bool _running;
  int _fd;

  double _scale;

  void setup();
  void shutdown();
public:
  PerfMeasure();
  ~PerfMeasure();

  uint64_t read();
};

class PowercapMeasure : public Measure
{
private:
  bool _running;
  uint64_t _last;
  uint64_t _total;

  void setup();
  void shutdown() { _running = false; }

public:
  PowercapMeasure();

  uint64_t read();
};

class OdroidMeasure : public Measure
{
   private:
    bool _running;
    std::array<double, 3> _last_values;

    std::array<std::string, 3> sensors = {
        "/sys/bus/i2c/devices/0-0040", /* big */
        "/sys/bus/i2c/devices/0-0041", /* dram */
        "/sys/bus/i2c/devices/0-0045"  /* little */
    };

    void setup();
    void shutdown();

   public:
    OdroidMeasure();
    ~OdroidMeasure();

    uint64_t read();
};

class NoMeasure : public Measure
{
private:
  void setup() {}
  void shutdown() {}
public:
  uint64_t read() { return 0; }
};

} /* namespace tetris */

#endif /* __ENERGY_H__ */
