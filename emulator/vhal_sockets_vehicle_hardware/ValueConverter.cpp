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
#define LOG_TAG "ValueConverter"
#include <stdexcept>
#include "ValueConverter.h"
#include "CommonUtils.h"
#include <aidl/android/hardware/automotive/vehicle/VehiclePropertyType.h>
#include <VehicleUtils.h>
#include <android-base/parseint.h>
#include <android-base/parsedouble.h>

using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyType;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

static const std::string STR_BOOL_TRUE("true");
static const std::string STR_BOOL_FALSE("false");

bool ValueConverter::toString(const VehiclePropValue& source, std::string& value) {

  switch (source.prop & toInt(VehiclePropertyType::MASK)) {
    case toInt(VehiclePropertyType::STRING):
      value = source.value.stringValue;
      return true;
      break;
    case toInt(VehiclePropertyType::BOOLEAN):
      if (!source.value.int32Values.empty()) {
        value = source.value.int32Values[0] != 0 ? STR_BOOL_TRUE : STR_BOOL_FALSE;
        return true;
      }
      break;
    case toInt(VehiclePropertyType::INT32):
      if (!source.value.int32Values.empty()) {
        value = std::to_string(source.value.int32Values[0]);
        return true;
      }
      break;
    case toInt(VehiclePropertyType::INT32_VEC):
      return false;
    case toInt(VehiclePropertyType::INT64):
      if (!source.value.int64Values.empty()) {
        value = std::to_string(source.value.int64Values[0]);
        return true;
      }
      break;
    case toInt(VehiclePropertyType::INT64_VEC):
      return false;
    case toInt(VehiclePropertyType::FLOAT):
      if (!source.value.floatValues.empty()) {
        value = std::to_string(source.value.floatValues[0]);
        return true;
      }
      break;
    case toInt(VehiclePropertyType::FLOAT_VEC):
      return false;
    case toInt(VehiclePropertyType::BYTES):
      if (!source.value.byteValues.empty()) {
        value = std::to_string(source.value.byteValues[0]);
        return true;
      }
      break;
    case toInt(VehiclePropertyType::MIXED):
      break;

    default:
      return false;
  }
  return false;
}

bool ValueConverter::fromString(const std::string &source, VehiclePropValue &value) {
  ALOGI("ValueConverter::fromSource [%s] with length %zu", source.c_str(), source.length());
  switch (value.prop & toInt(VehiclePropertyType::MASK)) {
    case toInt(VehiclePropertyType::STRING):
      ALOGI("ValueConverter::fromString : VehiclePropertyType::STRING: source %s",
            source.c_str());
      value.value.stringValue = source;
      return true;
    case toInt(VehiclePropertyType::BOOLEAN):
      ALOGI("ValueConverter CASE BOOLEAN");
      if (source.compare(STR_BOOL_TRUE) == 0 || source.compare(STR_BOOL_FALSE) == 0) {
        value.value.int32Values.push_back(source.compare(STR_BOOL_TRUE) ? 0 : 1);
        return true;
      }
      break;
    case toInt(VehiclePropertyType::INT32): {
      ALOGI("ValueConverter CASE INT32");
      int32_t val;
      ALOGI("ValueConverter::fromString : toInt(VehiclePropertyType::INT32) : source %s",
            source.c_str());
      if (::android::base::ParseInt(source.c_str(), &val)) {
        ALOGI("ValueConverter::fromString : toInt(VehiclePropertyType::INT32) : val %d", val);
        value.value.int32Values.push_back(val);
        return true;
      }
      break;
    }
    case toInt(VehiclePropertyType::INT32_VEC):
      return false;
    case toInt(VehiclePropertyType::INT64): {
      int64_t val;
      ALOGI("ValueConverter::fromString : toInt(VehiclePropertyType::INT64) : source %s",
            source.c_str());
      if (::android::base::ParseInt(source.c_str(), &val)) {
        ALOGI("ValueConverter::fromString : toInt(VehiclePropertyType::INT64) : val %ld", val);
        value.value.int64Values.push_back(val);
        return true;
      }
      break;
    }
    case toInt(VehiclePropertyType::INT64_VEC):
      return false;
    case toInt(VehiclePropertyType::FLOAT): {
      float val;
      ALOGI("ValueConverter::fromString : toInt(VehiclePropertyType::FLOAT) : source %s",
            source.c_str());
      if (::android::base::ParseFloat(source.c_str(), &val)) {
        ALOGI("ValueConverter::fromString : toInt(VehiclePropertyType::FLOAT) : val %f", val);
        value.value.floatValues.push_back(val);
        return true;
      }
      break;
    }
    case toInt(VehiclePropertyType::FLOAT_VEC):
      return false;
    case toInt(VehiclePropertyType::BYTES):
      return false;
    case toInt(VehiclePropertyType::MIXED):
      return false;
    default:
      return false;
  }
  return false;
}

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
