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
#ifndef android_hardware_automotive_vehicle_sockets_ValueRequester_H_
#define android_hardware_automotive_vehicle_sockets_ValueRequester_H_

#include <memory>

// #ifndef target_platform
// #include "default/AllDefaults.h"
// #endif

#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "ConfigProvider.h"
#include "BaseClient.h"
#include "ValueRequestType.h"

#include <aidl/android/hardware/automotive/vehicle/SetValueRequest.h>
#include <aidl/android/hardware/automotive/vehicle/SetValueResult.h>
#include <aidl/android/hardware/automotive/vehicle/GetValueRequest.h>
#include <aidl/android/hardware/automotive/vehicle/GetValueResult.h>
#include <aidl/android/hardware/automotive/vehicle/VehiclePropValue.h>

using ::aidl::android::hardware::automotive::vehicle::SetValueRequest;
using ::aidl::android::hardware::automotive::vehicle::SetValueResult;
using ::aidl::android::hardware::automotive::vehicle::GetValueRequest;
using ::aidl::android::hardware::automotive::vehicle::GetValueResult;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropValue;

using GetValuesCallback = std::function<void(
            std::vector<aidl::android::hardware::automotive::vehicle::GetValueResult>)>;
using SetValuesCallback = std::function<void(
            std::vector<aidl::android::hardware::automotive::vehicle::SetValueResult>)>;
using PropertyChangeCallback = std::function<void(
            std::vector<aidl::android::hardware::automotive::vehicle::VehiclePropValue>)>;
using SubscribeValueCallback = std::function<void(
            std::unordered_map<std::string, bool>)>;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

  struct SubscribeValueRequest {
    std::string propertyName;
  };


  struct SubscribeValuesCallback {
    std::shared_ptr<const std::function<void(std::unordered_map<std::string,bool>)>> subscriptionStatusCallback;
    std::shared_ptr<const PropertyChangeCallback> propertyChangeCallback;
  };

  class ValueRequester {
    private:
      class Request {
        public:
          Request(ValueRequestType type) : mType(type) {}

        public:
          ValueRequestType getType() const { return mType; };

        private:
          const ValueRequestType mType;
      };

      template <typename R, typename C>
      class TypedRequest : public Request {
        public:
          TypedRequest(ValueRequestType type, std::shared_ptr<const C> callback, const std::vector<R> requests)
            : Request(type), mCallback(callback), mRequests(requests) {}
        public:
          const std::shared_ptr<const C> callback() const { return mCallback; }
          const std::vector<R> requests() const { return mRequests; }
        private:
          const std::shared_ptr<const C> mCallback;
          const std::vector<R> mRequests;
      };
    public:
      ValueRequester(
        std::shared_ptr<ConfigProvider> mConfigProvider,
        int threadPoolSize = 2);
      ~ValueRequester();

    public:
      void registerSetValues(
        std::shared_ptr<const SetValuesCallback> callback,
        const std::vector<SetValueRequest> requests);

      void registerGetValues(
        std::shared_ptr<const GetValuesCallback> callback,
        const std::vector<GetValueRequest> requests);

      void registerSubscribeValues(
        std::shared_ptr<const SubscribeValueCallback> callback,
        const std::vector<SubscribeValueRequest> requests);

      void registerPropertyChangeCallback(std::shared_ptr<PropertyChangeCallback> propertyChangeCallback) {
        mPropertyChangeCallback = propertyChangeCallback;

        auto subscriptionCallback = std::make_shared<std::function<void(std::vector<SocketEvent>)>>([this](std::vector<SocketEvent> properties) {this->handleSubscriptionEvents(properties);});
        for (auto& client : mClients) {
          client->registerSubscriptionCallback(subscriptionCallback);
        }
      }

    private:
      const std::shared_ptr<ConfigProvider> mConfigProvider;
      std::vector<std::unique_ptr<BaseClient>> mClients;
      bool mIsRequestsThreadPoolRunning = true;
      std::vector<std::thread> mRequestsThreadPool;
      std::mutex mRequestLock;
      std::queue<Request*> mRequests;
      std::shared_ptr<PropertyChangeCallback> mPropertyChangeCallback;
      bool mSubscriptionAllowed = false;
      std::queue<Request*> mRequestsCache;

    private:
      void registerRequest(Request* request);
      void runRequestsExecutor(int executorId);
      void executeSetRequest(TypedRequest<SetValueRequest, SetValuesCallback>* request);
      void executeGetRequest(TypedRequest<GetValueRequest, GetValuesCallback>* request);
      void executeSubscribeRequest(TypedRequest<SubscribeValueRequest, SubscribeValueCallback>* request);
      void handleSubscriptionEvents(const std::vector<SocketEvent> events);
      void createClients();
  };
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_sockets_ValueRequester_H_