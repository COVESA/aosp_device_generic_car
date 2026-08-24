#define LOG_TAG "ConfigProvider"

#include "ConfigProvider.h"
#include <fstream>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

static const std::string STR_PROPERTY_ID("propertyId");
static const std::string STR_ACCESS("access");
static const std::string STR_CHANGE_MODE("changeMode");
static const std::string STR_AREA_TYPE("areaType");
static const std::string STR_SOURCES("sources");
static const std::string STR_PROTOCOL_TYPE("protocol");
static const std::string STR_SERVER_IP("serverIp");
static const std::string STR_SERVER_PORT("serverPort");
static const std::string STR_PROPERTIES("properties");
static const std::string STR_STANDARDS("standards");
static const std::string STR_VENDORS("vendors");
static const std::string STR_FORMULA("formula");

static const std::string CONFIG_DIR("/vendor/etc/vhalmap/");
static const std::string CONFIG_MAP("value_map.json");
static const std::string CONFIG_APP("app_config.json");

const std::string STR_PROTOCOL_TYPE_WS("websocket");
const std::unordered_map<std::string, DataStorageProtocolType> STR_TO_PROTOCOL_TYPE = {
      {STR_PROTOCOL_TYPE_WS, DataStorageProtocolType::WEBSOCKET},
  };


ConfigProvider::ConfigProvider() {
  // TODO: Copy file loaders from MappingProvider
  init();
}

void ConfigProvider::init() {

  tryLoadAppConfig();
  tryLoadVhalProperties();
}

bool ConfigProvider::tryLoadVhalProperties() {
  ALOGI("Trying to load VHAL properties...");

  Json::Value rootNode;
  std::string path(CONFIG_DIR);
  path.append(CONFIG_MAP);

  if(!tryLoadJsonFile(path, rootNode)) {
    ALOGE("Failed to load VHAL properties.");
    return false;
  }

  // Iterate through array of configuration objects
  for(Json::Value::ArrayIndex i = 0; i < rootNode.size(); i++) {
    Json::Value configObject = rootNode[i];

    // Parse the value map
    int propertyId = configObject[STR_PROPERTY_ID].asInt();
    Json::Value sources = configObject[STR_SOURCES];
    std::vector<VehicleAreaConfig> areaConfigs;

    for (const auto& source: sources.getMemberNames()) {
      auto areaId = sources[source].asInt();
      PropIdAreaId propIdAreaId {
        .propId = propertyId,
        .areaId = areaId,
      };

      mMappedValues[propIdAreaId] = source;
      ALOGI("VHAL properties mapped: %x(%d) %x %s", propertyId, propertyId, areaId, source.c_str());

      VehicleAreaConfig areaConfig = {
        .areaId = areaId
      };
        
      areaConfigs.push_back(areaConfig);
    }

    // Parse the properties
    int access = configObject[STR_ACCESS].asInt();
    int changeMode = configObject[STR_CHANGE_MODE].asInt();
    VehiclePropConfig config = {
      .prop = propertyId,
      .access = static_cast<VehiclePropertyAccess>(access),
      .changeMode = static_cast<VehiclePropertyChangeMode>(changeMode),
      .areaConfigs = areaConfigs,
    };
    ALOGI("MapperConfig config loaded property: %x(%d)", propertyId, propertyId);

    mProperties.push_back(config);

    if(!configObject[STR_FORMULA].empty()){
      mPropertyFormulas[propertyId] = configObject[STR_FORMULA];
    }

  }
  return true;
}

bool ConfigProvider::getMappedSource(int propertyId, int areaId, std::string& source) const {
  PropIdAreaId propIdAreaId {
    .propId = propertyId,
    .areaId = areaId,
  };

  if(auto valueSource = mMappedValues.find(propIdAreaId); valueSource != mMappedValues.end()) {
    source = valueSource->second;
    return true;
  }

  return false;
}

bool ConfigProvider::getValueInfo(int propertyId, int areaId, std::string& source) const {
  ALOGI("ConfigProvider::getValueInfo(): propertyId %d, areaId %d, source %s", propertyId, areaId, source.c_str());
  return getMappedSource(propertyId, areaId, source);
}

bool ConfigProvider::getMappedProperty(const std::string& source, int& propertyId, int& areaId) const {
  for (auto& it : mMappedValues) {
    if (it.second.compare(source) == 0) {
        propertyId = it.first.propId;
        areaId = it.first.areaId;
        return true;
    }
  }
  return false;
}

const std::vector<std::string> ConfigProvider::getMappedNames() const {
  std::vector<std::string> names;
  for(auto kv : mMappedValues) {
    names.push_back(kv.second);
  }
  return names;
}

bool ConfigProvider::tryLoadAppConfig() {
  ALOGI("Try load server config");

  Json::Value jsonRoot;
  std::string path(CONFIG_DIR);
  path.append(CONFIG_APP);

  if(!tryLoadJsonFile(path, jsonRoot)) {
    ALOGE("Failed to load server config");
    return false;
  }

  Json::Value jsonServerIp = jsonRoot[STR_SERVER_IP];
  if (jsonServerIp.empty()) {
    ALOGE("Failed to load server ip");
    return false;
  }
  mServerIp = jsonServerIp.asString();

  Json::Value jsonServerPort = jsonRoot[STR_SERVER_PORT];
  if (jsonServerPort.empty()) {
    ALOGE("Failed to load server port");
    return false;
  }
  mServerPort = jsonServerPort.asInt();

  Json::Value jsonProtocolType = jsonRoot[STR_PROTOCOL_TYPE];
  if (jsonProtocolType.empty()) {
    ALOGE("Failed to load service type");
    return false;
  }
  std::string strProtocolType = jsonProtocolType.asString();
  if (!setProtocolType(strProtocolType)) {
    ALOGE("Wrong service type %s, should be one of %s",
           strProtocolType.c_str(), STR_PROTOCOL_TYPE_WS.c_str());
    return false;
  }

  return true;
}

bool ConfigProvider::tryLoadJsonFile(const std::string& path, Json::Value& root) {
  ALOGI("Try load file %s", path.c_str());
  std::ifstream ifs(path.c_str());
  Json::CharReaderBuilder jsonBuilder;
  std::string errorMessage;
  if (!Json::parseFromStream(jsonBuilder, ifs, &root, &errorMessage)) {
      ALOGE("%s: Failed to load JSON file. Error: %s", __func__, errorMessage.c_str());
      return false;
  }
  ALOGI("File %s loaded", path.c_str());
  return true;
}

bool ConfigProvider::setProtocolType(const std::string& strProtocolType) {
    auto it = STR_TO_PROTOCOL_TYPE.find(strProtocolType);
    if (it != STR_TO_PROTOCOL_TYPE.end()) {
      mProtocolType = it->second;
      return true;
    } else {
      return false;
    }
}

std::string ConfigProvider::getDataStorageProtocolTypeStr() const{
  std::string protocolStr{"unknown"};
  for (std::unordered_map<std::string, DataStorageProtocolType>::const_iterator it = STR_TO_PROTOCOL_TYPE.begin();
              it != STR_TO_PROTOCOL_TYPE.end(); ++it) {
    if (it->second == mProtocolType) {
      protocolStr = it->first;
      break;
    }
  }
  return protocolStr;
}

Json::Value ConfigProvider::getFormula(int propertyId) const {
  return mPropertyFormulas.find(propertyId) != mPropertyFormulas.end() ? mPropertyFormulas.find(propertyId)->second : "";
}

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android