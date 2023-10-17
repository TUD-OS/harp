#ifndef __OPERATING_POINT_H__
#define __OPERATING_POINT_H__

#pragma once

#include "util/platform/cpu_sets.h"
#include "proto/tetris.pb.h"

#include <string>
#include <stdexcept>
#include <map>


namespace tetris {


class OperatingPoint {
   public:
    std::string name;
    std::map<std::string, double> characteristics;
    CPUThreadSet cpus;

   public:
    OperatingPoint() :
        name{"default"}, characteristics{}, cpus{}
    {}

    OperatingPoint(const std::string &name, const std::map<std::string, double> &characteristics, const CPUThreadSet &cpus) :
        name{name}, characteristics{characteristics}, cpus{cpus}
    {}

    OperatingPoint(const OperatingPoint &old, const std::map<int, int> &conv_map);

    OperatingPoint(const ClientMessage::OperatingPointsInfo::OPData &op);

    double characteristic(const std::string& criteria) const
    {
        if (characteristics.find(criteria) != characteristics.end())
            return characteristics.at(criteria);

        throw std::runtime_error("Unknown characteristic criteria.");
    }

    OperatingPoint convert(const std::map<int, int> &conv_map) const
    {
        return OperatingPoint{*this, conv_map};
    }
};

class OperatingPointAllocation
{
   public:
    OperatingPoint base;
    std::map<int, int> cpu_allocation;

   public:
    OperatingPointAllocation() :
        base{}, cpu_allocation{}
    {}

    OperatingPointAllocation(const OperatingPoint &base, const std::map<int, int> &cpu_allocation) :
        base{base}, cpu_allocation{cpu_allocation}
    {}

    OperatingPoint instantiate() const
    {
        return base.convert(cpu_allocation);
    }
};

} /* namespace tetris */

#endif // __OPERATING_POINT_H__
