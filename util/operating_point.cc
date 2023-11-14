#include "util/operating_point.h"

#include "util/platform/platform.h"

namespace tetris {

OperatingPoint::OperatingPoint(
    const Platform &platform,
    const ClientMessage::OperatingPointsInfo::OPData &op)
    : name{}, characteristics{}, cpus{}, cores_count{} {
  name = op.identifier();

  for (int i = 0; i < op.characteristics_size(); ++i) {
    auto c = op.characteristics(i);
    characteristics[c.name()] = c.value();
  }

  for (int i = 0; i < op.cpu_ids_size(); ++i) {
    cpus.Set(op.cpu_ids(i));
  }

  // Initialize cores_count
  cores_count = platform.GetCoreCountPerType(platform.ToCPUCoreSet(cpus));
}

} /* namespace tetris */
