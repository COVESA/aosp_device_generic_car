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
#ifndef android_hardware_automotive_vehicle_sockets_GrpcClient_H_
#define android_hardware_automotive_vehicle_sockets_GrpcClient_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "BaseClient.h"
#include "kuksa/val/v1/val.grpc.pb.h"

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

class GrpcResponse : public Response {
  public:
    GrpcResponse(bool status, const std::string& body) : Response(status, body) {}
};

class GrpcClient : public BaseClient {

  static const int TIMEOUT_SECONDS = 3;
  static const int TIMEOUT_RECONNECT_SEC = 3;
  static const int VALIDATE_TIMEOUT_SECONDS = 10;
  static const size_t VALIDATE_BATCH_SIZE = 250;

  public:
    GrpcClient(const std::string& serverIp, const int serverPort);
    ~GrpcClient();

  public:
    Response* sendGetValueSync(const std::string& propertyName) override;
    Response* sendSetValueSync(const std::string& propertyName, const std::string& value) override;
    Response* sendSubscribeValueSync(const std::string& propertyName) override;

  private:
    void closeSubscriptions() override;
    void runSubscriptions();
    bool readSubscriptionStream(const std::vector<std::string>& paths);
    bool validateSubscribedPaths(const std::vector<std::string>& paths);
    bool dropSubscribedPath(const std::string& path);
    void cancelSubscriptionStream();
    std::vector<std::string> getSubscribedPaths();
    std::string toString(const kuksa::val::v1::Datapoint& datapoint) const;
    bool toDatapoint(const std::string& propertyName, const std::string& value,
                     kuksa::val::v1::Datapoint& datapoint);
    void rememberValueType(const std::string& propertyName,
                           const kuksa::val::v1::Datapoint& datapoint);

  private:
    std::shared_ptr<grpc::Channel> mChannel;
    std::unique_ptr<kuksa::val::v1::VAL::Stub> mStub;
    std::thread mSubscriptionThread;
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mSubscriptionsChanged{false};
    std::atomic<bool> mPathsValidated{false};
    std::vector<std::string> mSubscribedPaths;
    std::mutex mStreamLock;
    grpc::ClientContext* mStreamContext = nullptr;
    std::mutex mValueTypeLock;
    std::unordered_map<std::string, kuksa::val::v1::Datapoint::ValueCase> mValueTypes;
};

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_sockets_GrpcClient_H_





