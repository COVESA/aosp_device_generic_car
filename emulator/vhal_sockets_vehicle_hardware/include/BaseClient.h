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
#ifndef android_hardware_automotive_vehicle_sockets_BaseClient_H_
#define android_hardware_automotive_vehicle_sockets_BaseClient_H_

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <json/json.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

static const std::string STR_RESPONSE_BODY_EMPTY("");

class Response {
  public:
    Response(const bool status, const std::string & body) : mStatus(status), mBody(body) {}
    virtual ~Response() {}

  public:
    bool status() const { return mStatus; }
    const std::string& body() const { return mBody; }
  protected:
    const bool mStatus;
    const std::string mBody;
};

template<typename T>
struct PendingRequest {
    std::mutex mtx;
    std::condition_variable cv;
    T response;
    bool done = false;
};

struct SocketEvent {
  std::string propertyName;
  std::string data;
};

class BaseClient {

  public:
    BaseClient(const std::string& serverIp, const int serverPort) : mServerIp(serverIp), mServerPort(serverPort) {}
    virtual ~BaseClient() {}

  public:
    virtual Response* sendGetValueSync(const std::string& propertyName) = 0;
    virtual Response* sendSetValueSync(const std::string& propertyName, const std::string& value) = 0;
    virtual Response* sendSubscribeValueSync(const std::string& propertyName) = 0;
    void registerSubscriptionCallback(std::shared_ptr<std::function<void(std::vector<SocketEvent>)>> subscriptionCallback) {
      mSubscriptionCallback = subscriptionCallback;
    }

  protected:
    virtual void closeSubscriptions() = 0;


  protected:
    const std::string& mServerIp;
    const int mServerPort;
    std::shared_ptr<std::function<void(std::vector<SocketEvent>)>> mSubscriptionCallback = nullptr;
    std::mutex mSubscriptionLock;
};

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_sockets_BaseClient_H_
