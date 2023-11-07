#include "util/operating_point.h"

namespace tetris {

OperatingPoint::OperatingPoint(
    const ClientMessage::OperatingPointsInfo::OPData &op)
    : name{}, characteristics{}, cpus{}, cores_count{} {
  throw std::runtime_error(
      "Need to initialize cores_count. Either add an extra field in the "
      "Protobuf interface, or pass a platform object in this ctor");
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
