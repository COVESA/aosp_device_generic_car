#ifndef android_hardware_automotive_vehicle_sockets_ValueConverter_H_
#define android_hardware_automotive_vehicle_sockets_ValueConverter_H_

#include <aidl/android/hardware/automotive/vehicle/VehiclePropValue.h>

#include <string>
#include <json/json.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

using ::aidl::android::hardware::automotive::vehicle::VehiclePropValue;
class ValueConverter {
  public:
    ValueConverter() = delete;

  public:
    static bool toString(const VehiclePropValue& source, std::string& value);
    static bool fromString(const std::string& source, VehiclePropValue& value);
};
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_sockets_ValueConverter_H_