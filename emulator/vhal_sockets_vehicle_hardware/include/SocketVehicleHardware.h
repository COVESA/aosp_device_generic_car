#ifndef android_hardware_automotive_vehicle_SocketVehicleHardware_H_
#define android_hardware_automotive_vehicle_SocketVehicleHardware_H_

// #ifndef target_platform
// #include "default/IVehicleHardware.h"
// #endif

#include <map>
#include <IVehicleHardware.h>
#include "ConfigProvider.h"
#include "ValueRequester.h"

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

using ::aidl::android::hardware::automotive::vehicle::VehiclePropValue;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropConfig;
using ::aidl::android::hardware::automotive::vehicle::StatusCode;
using ::aidl::android::hardware::automotive::vehicle::SetValueRequest;
using ::aidl::android::hardware::automotive::vehicle::GetValueRequest;

class SocketVehicleHardware : public IVehicleHardware {
  private:
    template<typename T>
    struct PendingRequest {
      bool isPending;
      T result;
    };

    template<typename T>
    struct PendingCallback {
      std::shared_ptr<const std::function<void(
                std::vector<T>)>> callback;
      std::unordered_map<long, PendingRequest<T>> requests;
    };

//IVehicleHardware impl
  public:
    SocketVehicleHardware();
    ~SocketVehicleHardware();
  public:
    std::vector<VehiclePropConfig> getAllPropertyConfigs() const override;

    StatusCode setValues(
            std::shared_ptr<const SetValuesCallback> callback,
            const std::vector<SetValueRequest>& requests) override;

    StatusCode getValues(
            std::shared_ptr<const GetValuesCallback> callback,
            const std::vector<GetValueRequest>& requests) const override;

    StatusCode updateSampleRate(
                [[maybe_unused]] int32_t propId, [[maybe_unused]] int32_t areaId,
                [[maybe_unused]] float sampleRate) {
      ALOGI("updateSampleRate");
      return StatusCode::OK;
    }

    DumpResult dump(const std::vector<std::string>& options) override;

    StatusCode checkHealth() override {
      // TODO: figure out how to check the connection
      return StatusCode::OK;
    }

    // Register a callback that would be called when there is a property change event from vehicle.
    void registerOnPropertyChangeEvent(
            std::unique_ptr<const PropertyChangeCallback> callback) {
              mPropertyChangeEventCallback = std::move(callback);
            }

    void registerOnPropertySetErrorEvent(
            std::unique_ptr<const PropertySetErrorCallback> callback) {
              propertySetErrorEventCallback = std::move(callback);
            }
//IVehicleHardware impl end
  private:
    std::shared_ptr<ConfigProvider> mConfigProvider;
    std::unique_ptr<ValueRequester> mValueRequester;

    std::unique_ptr<const PropertyChangeCallback> mPropertyChangeEventCallback;
    std::unique_ptr<const PropertySetErrorCallback> propertySetErrorEventCallback;
    bool mSubscribed = false;

  private:
    void init();
    void reload() {}
    void initSubscriptions();
    void handlePropertyChangeEvent(std::vector<VehiclePropValue> properties) const;
    void handleSubscriptionStatusChange(std::unordered_map<std::string, bool> statuses) const;
};

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
#endif // android_hardware_automotive_vehicle_SocketVehicleHardware_H_
