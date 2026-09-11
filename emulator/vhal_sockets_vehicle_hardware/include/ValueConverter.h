/*
 * Copyright (C) 2026 BMW Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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