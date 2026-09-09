#ifndef android_hardware_automotive_vehicle_aidl_impl_hardware_include_ConfigProvider_H_
#define android_hardware_automotive_vehicle_aidl_impl_hardware_include_ConfigProvider_H_

#include <unordered_map>
#include <string>
#include <vector>
#include <json/json.h>

#include "DataStorageProtocolType.h"

#include <VehicleUtils.h>
#include <aidl/android/hardware/automotive/vehicle/VehiclePropConfig.h>
#include <aidl/android/hardware/automotive/vehicle/VehicleAreaConfig.h>
#include <aidl/android/hardware/automotive/vehicle/VehiclePropertyAccess.h>
#include <aidl/android/hardware/automotive/vehicle/VehiclePropertyChangeMode.h>

using ::aidl::android::hardware::automotive::vehicle::VehiclePropConfig;
using ::aidl::android::hardware::automotive::vehicle::VehicleAreaConfig;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyAccess;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyChangeMode;

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

struct ConnectionConfig {
  std::string name;
  DataStorageProtocolType protocol;
  std::string serverIp;
  int serverPort;
};

class ConfigProvider {

  public:
    ConfigProvider();

  public:
    const std::vector<VehiclePropConfig>& getProperties() const {
      return mProperties;
    }
    const std::vector<std::string> getMappedNames() const;
    bool getMappedSource(int propertyId, int areaId, std::string& source) const;
    bool getMappedProperty(const std::string& source, int& propertyId, int& areaId) const;
    const std::vector<ConnectionConfig>& getConnections() const { return mConnections; }
    bool getValueInfo(int propertyId, int areaId, std::string& source) const;
    std::string getDataStorageProtocolTypeStr(DataStorageProtocolType protocolType) const;
    Json::Value getFormula(int propertyId) const;


  private:
    std::vector<VehiclePropConfig> mProperties;
    std::unordered_map<PropIdAreaId, std::string, PropIdAreaIdHash> mMappedValues;
    std::unordered_map<std::string, PropIdAreaId> mMappedSources;
    std::vector<ConnectionConfig> mConnections;
    std::unordered_map<int, Json::Value> mPropertyFormulas;


  private:
    void init();
    bool tryLoadVhalProperties();
    bool tryLoadAppConfig();
    bool tryLoadJsonFile(const std::string& path, Json::Value& root);
    bool parseConnection(const Json::Value& jsonConnection, ConnectionConfig& connection);

};

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
#endif // android_hardware_automotive_vehicle_aidl_impl_hardware_include_ConfigProvider_H_
