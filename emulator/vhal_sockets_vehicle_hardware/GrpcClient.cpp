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
#define LOG_TAG "GrpcClient"

#include "GrpcClient.h"
#include "utils/Log.h"

#include <android-base/parsedouble.h>
#include <android-base/parseint.h>

#include <algorithm>
#include <chrono>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

static const std::string STR_BOOL_TRUE("true");
static const std::string STR_BOOL_FALSE("false");

using ValueCase = kuksa::val::v1::Datapoint::ValueCase;

GrpcClient::GrpcClient(const std::string& serverIp, const int serverPort)
  : BaseClient(serverIp, serverPort) {

  const std::string target = "dns:///" + mServerIp + ":" + std::to_string(mServerPort);
  ALOGI("Connecting to Kuksa databroker %s", target.c_str());

  mChannel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  mStub = kuksa::val::v1::VAL::NewStub(mChannel);
  mRunning = true;

  try {
    mSubscriptionThread = std::thread(&GrpcClient::runSubscriptions, this);
  } catch (const std::system_error& e) {
    ALOGE("Failed to start subscription thread: %s", e.what());
  }
}

GrpcClient::~GrpcClient() {
  GrpcClient::closeSubscriptions();
}

void GrpcClient::closeSubscriptions() {
  mRunning = false;
  cancelSubscriptionStream();
  if (mSubscriptionThread.joinable()) {
    mSubscriptionThread.join();
  }
}

Response* GrpcClient::sendGetValueSync(const std::string& propertyName) {
  kuksa::val::v1::GetRequest request;
  auto* entry = request.add_entries();
  entry->set_path(propertyName);
  entry->set_view(kuksa::val::v1::VIEW_CURRENT_VALUE);
  entry->add_fields(kuksa::val::v1::FIELD_VALUE);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(TIMEOUT_SECONDS));

  kuksa::val::v1::GetResponse response;
  const auto status = mStub->Get(&context, request, &response);
  if (!status.ok()) {
    ALOGE("Get %s failed: %s", propertyName.c_str(), status.error_message().c_str());
    return new GrpcResponse(false, status.error_message());
  }

  if (response.entries().empty()) {
    ALOGE("Get %s returned no entry", propertyName.c_str());
    return new GrpcResponse(false, STR_RESPONSE_BODY_EMPTY);
  }

  const auto& datapoint = response.entries(0).value();
  if (datapoint.value_case() == ValueCase::VALUE_NOT_SET) {
    ALOGE("Get %s returned no value", propertyName.c_str());
    return new GrpcResponse(false, STR_RESPONSE_BODY_EMPTY);
  }

  rememberValueType(propertyName, datapoint);
  return new GrpcResponse(true, toString(datapoint));
}

Response* GrpcClient::sendSetValueSync(const std::string& propertyName, const std::string& value) {
  kuksa::val::v1::Datapoint datapoint;
  if (!toDatapoint(propertyName, value, datapoint)) {
    ALOGE("Set %s failed: cannot convert value %s", propertyName.c_str(), value.c_str());
    return new GrpcResponse(false, STR_RESPONSE_BODY_EMPTY);
  }

  kuksa::val::v1::SetRequest request;
  auto* update = request.add_updates();
  auto* entry = update->mutable_entry();
  entry->set_path(propertyName);
  *entry->mutable_value() = datapoint;
  update->add_fields(kuksa::val::v1::FIELD_VALUE);

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(TIMEOUT_SECONDS));

  kuksa::val::v1::SetResponse response;
  const auto status = mStub->Set(&context, request, &response);
  if (!status.ok()) {
    ALOGE("Set %s failed: %s", propertyName.c_str(), status.error_message().c_str());
    return new GrpcResponse(false, status.error_message());
  }

  if (response.error().code() != 0 || !response.errors().empty()) {
    ALOGE("Set %s rejected by broker: %s", propertyName.c_str(), response.error().message().c_str());
    return new GrpcResponse(false, response.error().message());
  }

  return new GrpcResponse(true, value);
}

Response* GrpcClient::sendSubscribeValueSync(const std::string& propertyName) {
  {
    std::lock_guard<std::mutex> lock(mSubscriptionLock);
    for (const auto& path : mSubscribedPaths) {
      if (path == propertyName) {
        return new GrpcResponse(true, STR_RESPONSE_BODY_EMPTY);
      }
    }
    mSubscribedPaths.push_back(propertyName);
  }

  ALOGI("Subscribing to %s", propertyName.c_str());
  mSubscriptionsChanged = true;
  mPathsValidated = false;
  cancelSubscriptionStream();

  return new GrpcResponse(true, STR_RESPONSE_BODY_EMPTY);
}

std::vector<std::string> GrpcClient::getSubscribedPaths() {
  std::lock_guard<std::mutex> lock(mSubscriptionLock);
  return mSubscribedPaths;
}

void GrpcClient::cancelSubscriptionStream() {
  std::lock_guard<std::mutex> lock(mStreamLock);
  if (mStreamContext != nullptr) {
    mStreamContext->TryCancel();
  }
}

void GrpcClient::runSubscriptions() {
  ALOGI("Subscription thread started");
  while (mRunning) {
    mSubscriptionsChanged = false;
    std::vector<std::string> paths = getSubscribedPaths();
    if (paths.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(TIMEOUT_RECONNECT_SEC));
      continue;
    }

    if (!mPathsValidated) {
      if (validateSubscribedPaths(paths)) {
        mPathsValidated = true;
      } else if (mRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(TIMEOUT_RECONNECT_SEC));
      }
      continue;
    }

    const bool pruned = readSubscriptionStream(paths);

    if (mRunning && !mSubscriptionsChanged && !pruned) {
      // Stream ended because of an error, back off before reconnecting.
      std::this_thread::sleep_for(std::chrono::seconds(TIMEOUT_RECONNECT_SEC));
    }
  }
  ALOGI("Subscription thread exiting");
}

// Asks the broker about every path in one batched Get. Unlike Subscribe, Get reports failures per
// entry, so paths the broker does not know can be identified and removed in a single round trip.
// Returns false when the broker could not be reached at all.
bool GrpcClient::validateSubscribedPaths(const std::vector<std::string>& paths) {
  size_t dropped = 0;
  for (size_t offset = 0; offset < paths.size() && mRunning; offset += VALIDATE_BATCH_SIZE) {
    const size_t end = std::min(offset + VALIDATE_BATCH_SIZE, paths.size());

    kuksa::val::v1::GetRequest request;
    for (size_t i = offset; i < end; i++) {
      auto* entry = request.add_entries();
      entry->set_path(paths[i]);
      entry->set_view(kuksa::val::v1::VIEW_CURRENT_VALUE);
      entry->add_fields(kuksa::val::v1::FIELD_VALUE);
    }

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(VALIDATE_TIMEOUT_SECONDS));

    kuksa::val::v1::GetResponse response;
    const auto status = mStub->Get(&context, request, &response);
    if (!status.ok()) {
      ALOGE("Validating subscription paths failed: %s", status.error_message().c_str());
      return false;
    }

    for (const auto& error : response.errors()) {
      if (dropSubscribedPath(error.path())) {
        dropped++;
      }
    }
  }

  if (dropped > 0) {
    ALOGI("Dropped %zu path(s) unknown to the broker, %zu remaining",
          dropped, getSubscribedPaths().size());
  }
  return true;
}

bool GrpcClient::dropSubscribedPath(const std::string& path) {
  std::lock_guard<std::mutex> lock(mSubscriptionLock);
  auto it = std::find(mSubscribedPaths.begin(), mSubscribedPaths.end(), path);
  if (it == mSubscribedPaths.end()) {
    return false;
  }
  mSubscribedPaths.erase(it);
  return true;
}

// Returns true when a path rejected by the broker was removed and the stream should be reopened
// immediately instead of backing off.
bool GrpcClient::readSubscriptionStream(const std::vector<std::string>& paths) {
  kuksa::val::v1::SubscribeRequest request;
  for (const auto& path : paths) {
    auto* entry = request.add_entries();
    entry->set_path(path);
    entry->set_view(kuksa::val::v1::VIEW_CURRENT_VALUE);
    entry->add_fields(kuksa::val::v1::FIELD_VALUE);
  }

  grpc::ClientContext context;
  {
    std::lock_guard<std::mutex> lock(mStreamLock);
    mStreamContext = &context;
  }

  auto stream = mStub->Subscribe(&context, request);
  ALOGI("Subscribed to %zu paths", paths.size());

  kuksa::val::v1::SubscribeResponse response;
  while (mRunning && !mSubscriptionsChanged && stream->Read(&response)) {
    std::vector<SocketEvent> events;
    for (const auto& update : response.updates()) {
      const auto& datapoint = update.entry().value();
      if (datapoint.value_case() == ValueCase::VALUE_NOT_SET) {
        continue;
      }
      rememberValueType(update.entry().path(), datapoint);
      events.push_back({
        .propertyName = update.entry().path(),
        .data = toString(datapoint)
      });
    }

    if (!events.empty() && mSubscriptionCallback != nullptr) {
      (*mSubscriptionCallback)(events);
    }
  }

  {
    std::lock_guard<std::mutex> lock(mStreamLock);
    mStreamContext = nullptr;
  }

  const auto status = stream->Finish();
  if (status.ok() || !mRunning || mSubscriptionsChanged) {
    return false;
  }

  // The broker fails the whole stream on the first unknown path and names it in the error message.
  // Drop that path and retry, so a single bad path cannot block updates for all the others.
  static const std::string PATH_MARKER("Path: ");
  const std::string message = status.error_message();
  const size_t pos = message.rfind(PATH_MARKER);
  if (pos != std::string::npos) {
    const std::string path = message.substr(pos + PATH_MARKER.length());
    if (dropSubscribedPath(path)) {
      ALOGI("Dropping path rejected by broker: %s", path.c_str());
      return true;
    }
  }

  ALOGE("Subscription stream ended: %s", message.c_str());
  return false;
}

std::string GrpcClient::toString(const kuksa::val::v1::Datapoint& datapoint) const {
  switch (datapoint.value_case()) {
    case ValueCase::kString:
      return datapoint.string();
    case ValueCase::kBool:
      return datapoint.bool_() ? STR_BOOL_TRUE : STR_BOOL_FALSE;
    case ValueCase::kInt32:
      return std::to_string(datapoint.int32());
    case ValueCase::kInt64:
      return std::to_string(datapoint.int64());
    case ValueCase::kUint32:
      return std::to_string(datapoint.uint32());
    case ValueCase::kUint64:
      return std::to_string(datapoint.uint64());
    case ValueCase::kFloat:
      return std::to_string(datapoint.float_());
    case ValueCase::kDouble:
      return std::to_string(datapoint.double_());
    default:
      return STR_RESPONSE_BODY_EMPTY;
  }
}

void GrpcClient::rememberValueType(const std::string& propertyName,
                                   const kuksa::val::v1::Datapoint& datapoint) {
  std::lock_guard<std::mutex> lock(mValueTypeLock);
  mValueTypes[propertyName] = datapoint.value_case();
}

bool GrpcClient::toDatapoint(const std::string& propertyName, const std::string& value,
                             kuksa::val::v1::Datapoint& datapoint) {
  ValueCase valueCase = ValueCase::VALUE_NOT_SET;
  {
    std::lock_guard<std::mutex> lock(mValueTypeLock);
    if (auto it = mValueTypes.find(propertyName); it != mValueTypes.end()) {
      valueCase = it->second;
    }
  }

  // The value type of a path is only known once the broker reported it via Get or Subscribe, fall
  // back to the textual representation otherwise.
  if (valueCase == ValueCase::VALUE_NOT_SET) {
    if (value == STR_BOOL_TRUE || value == STR_BOOL_FALSE) {
      valueCase = ValueCase::kBool;
    } else if (int64_t parsed; ::android::base::ParseInt(value, &parsed)) {
      valueCase = ValueCase::kInt64;
    } else if (double parsed; ::android::base::ParseDouble(value.c_str(), &parsed)) {
      valueCase = ValueCase::kDouble;
    } else {
      valueCase = ValueCase::kString;
    }
  }

  switch (valueCase) {
    case ValueCase::kString:
      datapoint.set_string(value);
      return true;
    case ValueCase::kBool:
      datapoint.set_bool_(value == STR_BOOL_TRUE);
      return true;
    case ValueCase::kInt32: {
      int32_t parsed;
      if (!::android::base::ParseInt(value, &parsed)) return false;
      datapoint.set_int32(parsed);
      return true;
    }
    case ValueCase::kInt64: {
      int64_t parsed;
      if (!::android::base::ParseInt(value, &parsed)) return false;
      datapoint.set_int64(parsed);
      return true;
    }
    case ValueCase::kUint32: {
      uint32_t parsed;
      if (!::android::base::ParseUint(value, &parsed)) return false;
      datapoint.set_uint32(parsed);
      return true;
    }
    case ValueCase::kUint64: {
      uint64_t parsed;
      if (!::android::base::ParseUint(value, &parsed)) return false;
      datapoint.set_uint64(parsed);
      return true;
    }
    case ValueCase::kFloat: {
      float parsed;
      if (!::android::base::ParseFloat(value.c_str(), &parsed)) return false;
      datapoint.set_float_(parsed);
      return true;
    }
    case ValueCase::kDouble: {
      double parsed;
      if (!::android::base::ParseDouble(value.c_str(), &parsed)) return false;
      datapoint.set_double_(parsed);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android





