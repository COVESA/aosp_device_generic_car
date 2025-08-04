#define LOG_TAG "WebSocketClient"

#include "WebSocketClient.h"
#include "CommonUtils.h"
#include "utils/Log.h"

#include <chrono>
using Clock = std::chrono::high_resolution_clock;
using Timepoint = std::chrono::time_point<Clock>;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {


static const std::string STR_WS_VALUE_GET("get");
static const std::string STR_WS_VALUE_SET("set");
static const std::string STR_WS_VALUE_EVENT_SUB("event/sub");
static const std::string STR_WS_VALUE_EVENT_UNSUB("event/unsub");
static const std::string STR_WS_VALUE_STATUS_SUCCESS("success");
static const std::string STR_WS_VALUE_STATUS_ERROR("error");
static const std::string STR_WS_VALUE_STATUS("status");
static const std::string STR_WS_VALUE_RESULT("result");
static const std::string STR_WS_VALUE_PROPERTY("property");
static const std::string STR_WS_VALUE_METHOD("method");
static const std::string STR_WS_VALUE_REQUEST_ID("requestId");
static const std::string STR_WS_VALUE_VALUE("value");

WebSocketClient::WebSocketClient(const std::string& serverIp, const int serverPort)
  : BaseClient(serverIp, serverPort) {

  ALOGI("Constructor starting");
  mRunning = true;

  // currently WebSocketClient connects to a dummy Server ./external/easywsclient/example-server-vhal.js
  // running on host with url "ws://localhost:8126/vhal-mock"
  ALOGI("Starting init thread");
  try {
    mInitThread = std::thread([this] () {
        ALOGI("Init thread started");
        while (mRunning) {
          if (!isConnected()) {
            ALOGI("Attempting connect...");
            if (connect()) {
              if (mWorkerThread.joinable()) {
                mWorkerThread.join();
              }
              mWorkerThread = std::thread(&WebSocketClient::awaitResponse, this);
            } else {
              ALOGE("Connect failed, retrying in %d s", TIMEOUT_RECONNECT_SEC);
            }
          }
          std::this_thread::sleep_for(std::chrono::seconds(TIMEOUT_RECONNECT_SEC));
        }
        ALOGI("Init thread exiting");
    });
  } catch (const std::system_error &e) {
    ALOGE("Failed to start init thread: %s", e.what());
  }
  ALOGI("Constructor terminating");
}

WebSocketClient::~WebSocketClient() {
  ALOGI("Destructor starting");
  mRunning = false;
  if (mInitThread.joinable()) {
    ALOGI("Trying to join init thread");
    try {
      mInitThread.join();
    } catch (const std::system_error &e) {
      ALOGE("Failed to join init thread: %s", e.what());
    }
  } else {
    ALOGI("Init thread not joinable");
  }
  ALOGI("Closing subscriptions");
  closeSubscriptions();
  if (mWorkerThread.joinable()) {
    mWorkerThread.join();
  }
  ALOGI("Closing");
  close();
}

bool WebSocketClient::isConnected() const {
  return mClient && mClient->getReadyState() == easywsclient::WebSocket::OPEN;
}

bool WebSocketClient::connect() {
  std::string url = "ws://" + mServerIp + ":" + std::to_string(mServerPort) + "/vhal-mock";
  ALOGI("Waking up, trying to connect to %s", url.c_str());

  std::unique_ptr<easywsclient::WebSocket> ws(easywsclient::WebSocket::from_url(url));
  if (ws && ws->getReadyState() == easywsclient::WebSocket::OPEN){
    mClient = std::move(ws);
    ALOGI("Successfully connected to WebSocket server to url %s.", url.c_str());
    return true;
  }

  ALOGE("Could not connect to WebSocket server to url %s.", url.c_str());
  return false;
}

void WebSocketClient::awaitResponse() {
  ALOGI("Worker thread started - awaitResponse");
  while (isConnected()) {
    mClient->poll();
    mClient->dispatch([&](const std::string& msg) {
      ALOGI("awaitResponse: dispatch msg %s", msg.c_str());
      handleResponse(msg);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ALOGI("Worker thread exiting");
}

void WebSocketClient::handleResponse(const std::string& msg) {
  Timepoint tStart = Clock::now();
  auto startNs = std::chrono::duration_cast<std::chrono::nanoseconds>(tStart.time_since_epoch()).count();
  ALOGI("handleResponse: start, ns=%lld, msg=%s", startNs, msg.c_str());

  // Step 1: Parse server response
  Json::Value responseJson;
  if (!parseResponse(msg, responseJson)) {
    ALOGE("handleResponse: failed to parse response");
    return;
  }

  // Step 2: Validate response json
  if (!responseJson.isMember(STR_WS_VALUE_REQUEST_ID) ||
      !responseJson.isMember(STR_WS_VALUE_METHOD)) {
    ALOGE("handleResponse: missing required fields in response");
    return;
  }

  std::string method = responseJson[STR_WS_VALUE_METHOD].asString();
  std::string requestId = responseJson[STR_WS_VALUE_REQUEST_ID].asString();

  // Step 3: Handle subscriptions, synchronized responses
  if (isFromSubscription(method)) {
    Timepoint tSubStart = Clock::now();
    readSubscription(responseJson);
    Timepoint tSubEnd = Clock::now();

    auto subDuration = std::chrono::duration_cast<std::chrono::microseconds>(tSubEnd - tSubStart).count();
    ALOGI("handleResponse: subscription handled in %lld µs", subDuration);
  } else   {
    Timepoint tHandleResponseStart = Clock::now();
    std::shared_ptr<PendingRequestJson> req;
    {
      std::lock_guard<std::mutex> lock(mPendingLock);
      auto it = mPending.find(requestId);
      if (it != mPending.end()) {
        req = it->second;
      }
    }

    if (req) {
      {
        std::lock_guard<std::mutex> lk(req->mtx);
        req->response = responseJson;
        req->done = true;
      }
      req->cv.notify_one();
      Timepoint tHandleResponseEnd = Clock::now();
      auto handleResponseDuration = std::chrono::duration_cast<std::chrono::microseconds>(
              tHandleResponseEnd - tHandleResponseStart).count();
      ALOGI("handleResponse: sync response handled in %lld µs (requestId=%s)",
            handleResponseDuration, requestId.c_str());
    }
  }

  // --- Total duration ---
  Timepoint tEnd = Clock::now();
  auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
  ALOGI("handleResponse: total time = %lld ms, requestId = %s", totalDuration, requestId.c_str());
}

bool WebSocketClient::isFromSubscription(const std::string& method) {
  return method == STR_WS_VALUE_EVENT_SUB ||  method == STR_WS_VALUE_SET; // read subscription when value is set
}

void WebSocketClient::readSubscription(const Json::Value& responseJson) {
  if (!responseJson.isMember(STR_WS_VALUE_PROPERTY) ||
      !responseJson.isMember(STR_WS_VALUE_RESULT)) {
    return;
  }

  std::string propertyName = responseJson[STR_WS_VALUE_PROPERTY].asString();
  std::string body = responseJson[STR_WS_VALUE_RESULT].asString();

  ALOGI("readSubscription : propertyName %s, body %s", propertyName.c_str(), body.c_str());

  std::vector<SocketEvent> events;
  if(!propertyName.empty() &&!body.empty()) {
    events.push_back({
      .propertyName = propertyName,
      .data = body
    });
  }

  if (events.size() > 0) {
    (*mSubscriptionCallback)(events);
  }
}

Response* WebSocketClient::sendGetValueSync(const std::string& propertyName){
  Json::Value request;
  request[STR_WS_VALUE_PROPERTY] = propertyName;
  request[STR_WS_VALUE_METHOD] = STR_WS_VALUE_GET;
  request[STR_WS_VALUE_REQUEST_ID] = UuidUtils::generateUuid();
  return sendValueSync(request);
}

Response* WebSocketClient::sendSetValueSync(const std::string& propertyName, const std::string& value){
  Json::Value request;
  request[STR_WS_VALUE_PROPERTY] = propertyName;
  request[STR_WS_VALUE_METHOD] = STR_WS_VALUE_SET;
  request[STR_WS_VALUE_VALUE] = value;
  request[STR_WS_VALUE_REQUEST_ID] = UuidUtils::generateUuid();
  return sendValueSync(request);
}

Response* WebSocketClient::sendSubscribeValueSync(const std::string& propertyName){
  Json::Value request;
  request[STR_WS_VALUE_PROPERTY] = propertyName;
  request[STR_WS_VALUE_METHOD] = STR_WS_VALUE_EVENT_SUB;
  request[STR_WS_VALUE_REQUEST_ID] = UuidUtils::generateUuid();

  // Store subscription
  Subscription* subscription = new Subscription();
  subscription->client = mSubscriptions.size() + 1;
  subscription->propertyName = propertyName;
  {
    std::lock_guard<std::mutex> lock(mSubscriptionLock);
    mSubscriptions.push(subscription);
  }
  return sendValueSync(request);
}

Response* WebSocketClient::sendUnsubscribeValueSync(const std::string& propertyName){
  Json::Value request;
  request[STR_WS_VALUE_PROPERTY] = propertyName;
  request[STR_WS_VALUE_METHOD] = STR_WS_VALUE_EVENT_UNSUB;
  request[STR_WS_VALUE_REQUEST_ID] = UuidUtils::generateUuid();
  return sendValueSync(request);
}

Response* WebSocketClient::sendValueSync(const Json::Value& request, int timeoutSeconds) {
  Timepoint tStart = Clock::now();
  auto startNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
          tStart.time_since_epoch()).count();

  Json::StreamWriterBuilder writer;
  auto data = Json::writeString(writer, request);
  ALOGI("sendValueSync: start, ns=%lld, request=%s", startNs, data.c_str());

  if (!isConnected()) {
    ALOGW("sendValueSync: not connected");
    return new WebSocketResponse(false, "WebSocket not connected");
  }

  bool isSubscribeResponse = isFromSubscription(request[STR_WS_VALUE_METHOD].asString());
  std::string requestId = request[STR_WS_VALUE_REQUEST_ID].asString();

  // Step 1: Create a per-request synchronization object for Get requests
  auto pending = std::make_shared<PendingRequestJson>();
  {
    std::lock_guard<std::mutex> lock(mPendingLock);
    mPending[requestId] = pending;
  }

  // Step 2: Send request
  if (mClient) {
    Timepoint tSendStart = Clock::now();
    mClient->send(data);
    Timepoint tSendEnd = Clock::now();
    auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(
            tSendEnd - tSendStart).count();
    ALOGI("sendValueSync: request sent to server in %lld µs (requestId=%s)",
          sendDuration,  requestId.c_str());
  } else {
    ALOGE("sendValueSync: no client");
  }

  // Step 3: Wait for synchronous responses
  // Subscriptions are handled in readSubscription().
  Response* wsResponse = nullptr;
  if (!isSubscribeResponse) {
    Timepoint tWaitStart = Clock::now();

    ALOGI("sendValueSync: waiting for response...");
    std::unique_lock<std::mutex> lock(pending->mtx);
    bool gotResponse = pending->cv.wait_for(
            lock,
            std::chrono::seconds(timeoutSeconds),
            [&pending]() { return pending->done; }
    );

    Timepoint tWaitEnd = Clock::now();
    auto waitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            tWaitEnd - tWaitStart).count();
    ALOGI("sendValueSync: wait time = %lld ms (requestId=%s)", waitDuration, requestId.c_str());

    if (gotResponse) {
      wsResponse = parseWebsocketResponse(pending->response);
    } else {
      ALOGE("sendValueSync: timeout after %lld ms (requestId=%s)", waitDuration, requestId.c_str());
      wsResponse = new WebSocketResponse(false, "Timeout waiting for response");
    }
  } else {
    wsResponse = new WebSocketResponse(true, ""); // subscription responses handled elsewhere
  }

  // Step 4: Cleanup
  {
    std::lock_guard<std::mutex> g(mPendingLock);
    mPending.erase(requestId);
  }

  // --- Total duration ---
  Timepoint tEnd = Clock::now();
  auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
  ALOGI("sendValueSync: total time = %lld ms, response = %s, status = %d, requestId = %s\n\n",
        totalDuration, wsResponse->body().c_str(), wsResponse->status(), requestId.c_str());

  return wsResponse;
}

Response* WebSocketClient::parseWebsocketResponse(const Json::Value& responseJson) {
  bool status = false;
  std::string body = "WebSocket response not available";

  if (!responseJson.isMember(STR_WS_VALUE_STATUS) || !responseJson.isMember(STR_WS_VALUE_RESULT)) {
    ALOGE("parseWebsocketResponse: missing required fields");
  } else {
    status = responseJson[STR_WS_VALUE_STATUS].asString() == STR_WS_VALUE_STATUS_SUCCESS;
    body = responseJson[STR_WS_VALUE_RESULT].asString();
  }

  return new WebSocketResponse(status, body);
}

bool WebSocketClient::parseResponse(const std::string& data, Json::Value& responseJson) {
  ALOGI("parseResponse : data %s", data.c_str());

  if (data.length() == 0) {
    ALOGE("parseResponse: response data is empty");
    return false;
  }

  // Initialize output values
  responseJson.clear();

  // Parse JSON
  JSONCPP_STRING err;
  Json::CharReaderBuilder builder;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  const auto jsonLength = static_cast<int>(data.length());
  if (!reader->parse(data.c_str(), data.c_str() + jsonLength, &responseJson, &err)) {
    ALOGE("parseResponse: %s", err.c_str());
    return false;
  }

  return true;
}

void WebSocketClient::closeSubscriptions() {
  while (mSubscriptions.size() > 0) {
    Subscription* subscription = mSubscriptions.front();
    mSubscriptions.pop();
    Response* response = sendUnsubscribeValueSync(subscription->propertyName);
    delete response;
    delete subscription;
  }
}

void WebSocketClient::close() {
  if (mClient->getReadyState() != easywsclient::WebSocket::CLOSED) {
    mClient->close();
    mClient.reset();
  }
}


}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
