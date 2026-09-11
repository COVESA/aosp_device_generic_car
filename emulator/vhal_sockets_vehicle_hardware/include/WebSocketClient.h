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
#ifndef android_hardware_automotive_vehicle_sockets_WebSocketClient_H_
#define android_hardware_automotive_vehicle_sockets_WebSocketClient_H_

#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <vector>

#include <easywsclient.hpp>

#include "BaseClient.h"

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {


class WebSocketResponse : public Response {
  public:
    WebSocketResponse(bool status, const std::string & body) : Response(status, body) {}
};

class WebSocketClient : public BaseClient {

  static const int TIMEOUT_SECONDS = 3;
  static const int TIMEOUT_RECONNECT_SEC = 3;
  using PendingRequestJson = PendingRequest<Json::Value>;


public:
    WebSocketClient(const std::string& serverIp, const int serverPort);
    ~WebSocketClient();

  public:
    Response* sendGetValueSync(const std::string& propertyName) override;
    Response* sendSetValueSync(const std::string& propertyName, const std::string& value) override;
    Response* sendSubscribeValueSync(const std::string& propertyName) override;
    Response* sendUnsubscribeValueSync(const std::string& propertyName);

  private:
    bool isConnected() const;
    void awaitResponse();
    void handleResponse(const std::string& msg);
    bool isFromSubscription(const std::string& method);
    Response* parseWebsocketResponse(const Json::Value& responseJson);
    bool parseResponse(const std::string& data, Json::Value& responseJson);
    void readSubscription(const Json::Value& responseJson);
    Response* sendValueSync(const Json::Value& request, int timeoutSeconds = TIMEOUT_SECONDS);
    bool connect();
    void resubscribeAll();
    void closeSubscriptions() override;
    void close();

  private:
    std::unique_ptr<easywsclient::WebSocket> mClient;
    std::thread mWorkerThread;
    std::mutex mPendingLock;
    std::unordered_map<std::string, std::shared_ptr<PendingRequestJson>> mPending;
    std::thread mInitThread;
    std::atomic<bool> mRunning{false};
    std::vector<std::string> mSubscribedPaths;
};

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_sockets_WebSocketClient_H_