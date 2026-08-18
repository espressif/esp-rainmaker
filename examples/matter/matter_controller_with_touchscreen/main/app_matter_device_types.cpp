#include <app_matter_device_types.h>

static constexpr uint32_t kOnOffLightDeviceTypeId = 0x0100;
static constexpr uint32_t kOnOffLightSwitchDeviceTypeId = 0x0103;
static constexpr uint32_t kOnOffPluginUnitDeviceTypeId = 0x010A;
static constexpr uint32_t kOnOffLightDeviceTypeIdV2 = 0x010D;

static matter_device_type_t map_device_type(uint32_t device_type_id)
{
    switch (device_type_id) {
    case kOnOffLightDeviceTypeId:
    case kOnOffLightDeviceTypeIdV2:
        return MATTER_DEVICE_TYPE_LIGHT;
    case kOnOffPluginUnitDeviceTypeId:
        return MATTER_DEVICE_TYPE_PLUG;
    case kOnOffLightSwitchDeviceTypeId:
        return MATTER_DEVICE_TYPE_SWITCH;
    default:
        return MATTER_DEVICE_TYPE_UNKNOWN;
    }
}

matter_device_type_t matter_device_type_from_endpoint(const endpoint_entry_t *endpoint)
{
    if (!endpoint) {
        return MATTER_DEVICE_TYPE_UNKNOWN;
    }
    for (uint8_t i = 0; i < endpoint->device_type_count; ++i) {
        matter_device_type_t type = map_device_type(endpoint->device_type_list[i]);
        if (type != MATTER_DEVICE_TYPE_UNKNOWN) {
            return type;
        }
    }
    return MATTER_DEVICE_TYPE_UNKNOWN;
}

const char *matter_device_type_label(matter_device_type_t type)
{
    switch (type) {
    case MATTER_DEVICE_TYPE_LIGHT:
        return "Light";
    case MATTER_DEVICE_TYPE_PLUG:
        return "Plug";
    case MATTER_DEVICE_TYPE_SWITCH:
        return "Switch";
    default:
        return "Unknown";
    }
}

bool matter_device_type_is_onoff(matter_device_type_t type)
{
    return type == MATTER_DEVICE_TYPE_LIGHT || type == MATTER_DEVICE_TYPE_PLUG || type == MATTER_DEVICE_TYPE_SWITCH;
}
