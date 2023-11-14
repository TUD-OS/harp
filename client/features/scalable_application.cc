#include "scalable_application.h"

#include "client/client.h"

namespace tetris {

ScalableApplication::ScalableApplication(std::function<bool (int)> scale_cb) :
    _logger{debug::Logger::get()}, _scale_cb{scale_cb}
{}

void ScalableApplication::mapping_update(const MappingUpdate &mapping, const ConversionMap& /*unused*/)
{
    LOGGER->debug(" > Updating application scaling\n");

    _active_mapping = std::make_unique<Mapping>(mapping);

    LOGGER->debug(" -> Using mapping %s\n", _active_mapping->name.c_str());
    LOGGER->debug(" -* New scaling factor: %d\n", _active_mapping->cpus.Size());

    if (!_scale_cb(_active_mapping->cpus.Size()))
        LOGGER->warning(" --* Failed to resize to the required scaling factor\n");
}

bool ScalableApplication::extend_mapping(MappingsInfo &mappings)
{
    return false;
}

}
