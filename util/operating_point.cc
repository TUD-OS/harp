#include "util/operating_point.h"

namespace tetris {

OperatingPoint::OperatingPoint(const OperatingPoint &old, const std::map<int,int> &conv_map)
    : name{old.name}, characteristics{old.characteristics}, cpus{}
{
    for (auto cpu : old.cpus) {
        if (conv_map.find(cpu) != conv_map.end())
            cpus.Set(conv_map.at(cpu));
        else
            cpus.Set(cpu);
    }
}

OperatingPoint::OperatingPoint(const ClientMessage::OperatingPointsInfo::OPData &op) :
    name{}, characteristics{}, cpus{}
{
    name = op.identifier();

    for (int i = 0; i < op.characteristics_size(); ++i) {
        auto c = op.characteristics(i);
        characteristics[c.name()] = c.value();
    }

    for (int i = 0; i < op.cpu_ids_size(); ++i) {
        cpus.Set(op.cpu_ids(i));
    }
}

} /* namespace tetris */
