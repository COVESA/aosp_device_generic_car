#define LOG_TAG "ValueRequester"
#include <chrono>

#include "ValueConverter.h"
#include "ValueRequester.h"
#include "WebSocketClient.h"
#include "utils/Log.h"

#include <aidl/android/hardware/automotive/vehicle/StatusCode.h>
#include <aidl/android/hardware/automotive/vehicle/VehiclePropertyStatus.h>
#include <utils/SystemClock.h>

using ::aidl::android::hardware::automotive::vehicle::SetValueResult;
using ::aidl::android::hardware::automotive::vehicle::StatusCode;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyStatus;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {


static const int THREAD_POOL_MIN_SIZE = 1;
static const int THREAD_POOL_DELAY = 1000;

ValueRequester::ValueRequester(
  std::shared_ptr<ConfigProvider> configProvider,
  int threadPoolSize)
  : mConfigProvider(configProvider) {
  mClient = getClient();
  mIsRequestsThreadPoolRunning = true;
  int threadsCount = threadPoolSize < THREAD_POOL_MIN_SIZE ? THREAD_POOL_MIN_SIZE : threadPoolSize;
  for (int thread = 0; thread < threadsCount; thread++) {
    mRequestsThreadPool.push_back(std::thread(&ValueRequester::runRequestsExecutor, this, thread));
  }
}

ValueRequester::~ValueRequester() {
  mIsRequestsThreadPoolRunning = false;
  for (std::thread& thread: mRequestsThreadPool) {
    if (thread.joinable())
      thread.join();
  }
  mRequestsThreadPool.clear();
}

void ValueRequester::registerSetValues(
  std::shared_ptr<const SetValuesCallback> callback,
  const std::vector<SetValueRequest> requests) {
  ALOGI("registerSetValues");
  auto request= std::make_unique<TypedRequest<SetValueRequest, SetValuesCallback>>(ValueRequestType::Set, callback, requests);
  registerRequest(request.release());
}


void ValueRequester::registerGetValues(
  std::shared_ptr<const GetValuesCallback> callback,
  const std::vector<GetValueRequest> requests) {
  ALOGI("registerGetValues");
  if (!mSubscriptionAllowed) {
    ALOGI("registerSubscribeValues from cache");
    mSubscriptionAllowed = true;
    while(!mRequestsCache.empty()) {
      Request* subRequest = mRequestsCache.front();
      mRequestsCache.pop();
      registerRequest(subRequest);
    }
  }

  auto request = std::make_unique<TypedRequest<GetValueRequest, GetValuesCallback>>(ValueRequestType::Get, callback, requests);
  registerRequest(request.release());
}

void ValueRequester::registerSubscribeValues(
  std::shared_ptr<const SubscribeValueCallback> callback,
  const std::vector<SubscribeValueRequest> requests) {
  ALOGI("registerSubscribeValues");

  auto request = std::make_unique<TypedRequest<SubscribeValueRequest, SubscribeValueCallback>>(ValueRequestType::Subscribe, callback, requests);
  if (mSubscriptionAllowed) {
    registerRequest(request.release());
  } else {
    mRequestsCache.push(request.release());
  }
}

void ValueRequester::registerRequest(Request* request) {
  std::lock_guard<std::mutex> guard(mRequestLock);
  ALOGI("registerRequest");

  mRequests.push(request);
}

void ValueRequester::runRequestsExecutor(int /*executorId*/) {
  ALOGI("runRequestsExecutor");
  while (mIsRequestsThreadPoolRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_POOL_DELAY));

    Request *request = nullptr;
    mRequestLock.lock();
    if (!mRequests.empty()) {
      request = mRequests.front();
      mRequests.pop();
    }
    mRequestLock.unlock();

    if (request != nullptr) {
      switch (request->getType()) {
        case ValueRequestType::Get:
          ALOGI("Get Value request found");
          executeGetRequest((TypedRequest<GetValueRequest, GetValuesCallback> *) request);
          break;
        case ValueRequestType::Set:
          ALOGI("Set Value request found");
          executeSetRequest((TypedRequest<SetValueRequest, SetValuesCallback> *) request);
          break;
        case ValueRequestType::Subscribe:
          ALOGI("Subscribe Value request found");
          executeSubscribeRequest(
                  (TypedRequest<SubscribeValueRequest, SubscribeValueCallback> *) request);
          break;
        default:
          ALOGE("Unknown request type");
      }
      delete request;
    }
  }
}

void ValueRequester::executeSetRequest(TypedRequest<SetValueRequest, SetValuesCallback>* request) {
  ALOGI("executeSetRequest");
  std::vector<SetValueResult> results;
  std::vector<VehiclePropValue> updatedPropValues;
  for(auto& req : request->requests()) {
    SetValueResult result;
    result.status = StatusCode::NOT_AVAILABLE;
    result.requestId = req.requestId;
    std::string mappedSource;
    if (mConfigProvider->getValueInfo(req.value.prop, req.value.areaId, mappedSource)){
      if (mappedSource.length() > 0) {
        std::string strValue;
        if (ValueConverter::toString(req.value, strValue)) {
          Response* response = mClient->sendSetValueSync(mappedSource, strValue);
          if (response->status()) {
            ALOGI("Set property value successfully");
            result.status = StatusCode::OK;
          }
          delete response;
        }
      }
    }
    results.push_back(result);
  }
  (*request->callback())(std::move(results));
}

void ValueRequester::executeGetRequest(TypedRequest<GetValueRequest, GetValuesCallback>* request) {
  ALOGI("executeGetRequest");
  std::vector<GetValueResult> results;
  for (auto& req : request->requests()) {
    int64_t timestamp = elapsedRealtimeNano();

    GetValueResult result;
    result.requestId = req.requestId;

    result.status = StatusCode::NOT_AVAILABLE;
    std::string mappedSource;
    if (mConfigProvider->getValueInfo(req.prop.prop, req.prop.areaId, mappedSource)) {

      if (mappedSource.length() > 0) {
        Response* response = mClient->sendGetValueSync(mappedSource);
        if (response->status()) {
          VehiclePropValue propValue;
          propValue.areaId = req.prop.areaId;
          propValue.prop = req.prop.prop;
          propValue.timestamp = timestamp;
          if (ValueConverter::fromString(response->body(), propValue)) {
            ALOGI("Get value property ok");

            propValue.status = VehiclePropertyStatus::AVAILABLE;
            result.prop = propValue;
            result.status = StatusCode::OK;
          } else {
            ALOGE("Get value property cannot map");
          }
        } else {
          ALOGE("Get value property cannot map");
        }
        delete response;
      } else {
        ALOGI("Get value property not mapped");
      }
    } else {
      ALOGI("Get value property not available");
      result.status = StatusCode::NOT_AVAILABLE;
    }
    results.push_back(result);
  }

  ALOGI("Get value callback");
  (*request->callback())(results);
}

void ValueRequester::executeSubscribeRequest(TypedRequest<SubscribeValueRequest, SubscribeValueCallback>* request) {
  ALOGI("executeSubscribeRequest");

  std::unordered_map<std::string, bool> result;
  std::vector<SubscribeValueRequest> unsubscribed;
  for (auto& req : request->requests()) {
    Response* response = mClient->sendSubscribeValueSync(req.propertyName);
    result[req.propertyName] = response->status();
    if (!response->status()) {
      unsubscribed.push_back({.propertyName = req.propertyName});
    }
    delete response;
  }

  (*request->callback())(result);

  if (unsubscribed.size() > 0) {
    auto subRequest = std::make_unique<TypedRequest<SubscribeValueRequest, SubscribeValueCallback>>(ValueRequestType::Subscribe, request->callback(), unsubscribed);
    registerRequest(subRequest.release());
  }
}

bool ValueRequester::isMappedValue(const VehiclePropValue& value, std::string& propertyName) {
  return mConfigProvider->getMappedSource(value.prop, value.areaId, propertyName);
}

void ValueRequester::handleSubscriptionEvents(const std::vector<SocketEvent> events){
  if (mPropertyChangeCallback) {
    std::vector<VehiclePropValue> results;
    for (auto& event : events) {
      VehiclePropValue propValue;
      propValue.timestamp =  elapsedRealtimeNano();

      mConfigProvider->getMappedProperty(event.propertyName, propValue.prop, propValue.areaId);
      bool found = ValueConverter::fromString(event.data, propValue);
      if (!found) {
        ALOGE("failed to convert property value");
        propValue.status = VehiclePropertyStatus::ERROR;
      } else {
        propValue.status = VehiclePropertyStatus::AVAILABLE;
      }
      results.push_back(propValue);
    }
    (*mPropertyChangeCallback)(results);
  }
}

std::unique_ptr<BaseClient> ValueRequester::getClient() const {
  std::unique_ptr<BaseClient> client;
  switch(mConfigProvider->getDataStorageProtocolType()) {
    case DataStorageProtocolType::WEBSOCKET:
    default:
      client = std::make_unique<WebSocketClient>(mConfigProvider->getServerIp(), mConfigProvider->getServerPort());
      ALOGI("Service type websocket is activated");
      break;
  }
  return client;
}

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
