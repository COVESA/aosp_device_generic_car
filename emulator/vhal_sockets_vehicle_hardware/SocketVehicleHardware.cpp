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
#define LOG_TAG "SocketVehicleHardware"
#include <string>
#include <vector>

#include <fstream>
#include <android-base/strings.h>

#include <dirent.h>

#include "utils/Log.h"
#include "SocketVehicleHardware.h"

using ::android::base::EqualsIgnoreCase;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

SocketVehicleHardware::SocketVehicleHardware() {
  mConfigProvider = std::make_shared<ConfigProvider>();
  mValueRequester = std::make_unique<ValueRequester>(mConfigProvider, 4);

  mValueRequester->registerPropertyChangeCallback(std::make_shared<PropertyChangeCallback>([this](std::vector<aidl::android::hardware::automotive::vehicle::VehiclePropValue> properties) {this->handlePropertyChangeEvent(properties);}));
  initSubscriptions();
}

SocketVehicleHardware::~SocketVehicleHardware() {}

std::vector<VehiclePropConfig> SocketVehicleHardware::getAllPropertyConfigs() const {
  ALOGI("SocketVehicleHardware::getAllPropertyConfigs(): ");
  const std::vector<VehiclePropConfig> & properties = mConfigProvider->getProperties();
  for (const auto& property : properties) {
    ALOGI("    prop : %s, access: %d, changeMode: %d", std::to_string(property.prop).c_str(), property.access, property.changeMode);
  }
  return mConfigProvider->getProperties();
}


StatusCode SocketVehicleHardware::setValues(
  std::shared_ptr<const SetValuesCallback> callback,
  const std::vector<SetValueRequest>& requests) {

    mValueRequester->registerSetValues(callback, requests);
    return StatusCode::OK;
}

StatusCode SocketVehicleHardware::getValues(
  std::shared_ptr<const GetValuesCallback> callback,
  const std::vector<GetValueRequest>& requests) const {
    mValueRequester->registerGetValues(callback, requests);
    return StatusCode::OK;
}

void loadFile(const std::string& path, std::string& data) {
  std::ifstream inputFile(path);

  if (inputFile.is_open()) {
      std::string fileContent((std::istreambuf_iterator<char>(inputFile)),
                                std::istreambuf_iterator<char>());
      data = fileContent;
      inputFile.close();
  } else {
      data = "ERROR Unable to open file " + path;
  }
}

DumpResult SocketVehicleHardware::dump(const std::vector<std::string>& options) {
  DumpResult result;
  result.callerShouldDumpState = false;
  if (options.size() == 0) {
        // We only want caller to dump default state when there is no options.
    result.callerShouldDumpState = true;
    result.buffer = "version 0.1.0";
    return result;
  }

  std::string option = options[0];
  if(options.size() > 1 && EqualsIgnoreCase(option, "--lfile")) {
    std::string path = options[1];
    loadFile(path, result.buffer);
  } else if(EqualsIgnoreCase(option, "--config")) {
    std::string data("config:\n");
    for (const auto& connection : mConfigProvider->getConnections()) {
      data.append("Server ");
      data.append(connection.name);
      data.append(" :");
      data.append(connection.serverIp);
      data.append(" : ");
      data.append(std::to_string(connection.serverPort));
      data.append("\n");
    }
    result.buffer = data;
  } else if(EqualsIgnoreCase(option, "--properties")) {
    std::string data("Properties:\n");
    const std::vector<VehiclePropConfig> & properties = mConfigProvider->getProperties();
    for (const auto& property : properties) {
      data.append(std::to_string(property.prop));
      std::string propertyName;
      if (mConfigProvider->getMappedSource(property.prop, 0, propertyName)) {
        data.append("\t");
        data.append(propertyName);
      }
      data.append("\n");
    }
    result.buffer = data;
  } else if(EqualsIgnoreCase(option, "--protocol")) {
    std::string data("protocol: ");
    for (const auto& connection : mConfigProvider->getConnections()) {
      data.append(connection.name);
      data.append(" = ");
      data.append(mConfigProvider->getDataStorageProtocolTypeStr(connection.protocol));
      data.append("\n");
    }
    result.buffer = data;
  } else {
    result.buffer = "Unknown commands";
  }
  return result;
}

void SocketVehicleHardware::handlePropertyChangeEvent(
  std::vector<VehiclePropValue> properties) const {
    ALOGI("PropertyChangeEvent from Client triggered");
  if (mPropertyChangeEventCallback != nullptr) {
    ALOGI("PropertyChangeEvent from Client delivered");
    (*mPropertyChangeEventCallback)(properties);
  }
}

void SocketVehicleHardware::initSubscriptions() {
  if (!mSubscribed) {
        mSubscribed = true;
        const std::vector<std::string> mappedNames = mConfigProvider->getMappedNames();

        std::vector<SubscribeValueRequest> subscriptions;
        for (auto& name : mappedNames) {
          if (name.length() > 0) {
            subscriptions.push_back({.propertyName = name});
          }
        }
        mValueRequester->registerSubscribeValues(
          std::make_shared<const SubscribeValueCallback>([this](std::unordered_map<std::string, bool> responses) {this->handleSubscriptionStatusChange(responses);}),
          subscriptions);
    }
}

void SocketVehicleHardware::handleSubscriptionStatusChange(std::unordered_map<std::string, bool> /*statuses*/) const {
    ALOGI("SubscriptionStatusChange from Client triggered");
}


}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
