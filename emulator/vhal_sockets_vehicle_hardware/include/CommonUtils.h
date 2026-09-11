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
#ifndef android_hardware_automotive_vehicle_sockets_CommonUtils_H_
#define android_hardware_automotive_vehicle_sockets_CommonUtils_H_

#include <string>
#include <vector>
#include <sstream>
#include <random>
#include <iomanip>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

class StringUtils {
  public:
    template<typename ... Args>
    static bool format(std::string& output, const std::string& format, Args ... args )
    {
      int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
      if( size_s <= 0 ) { return false; }
      auto size = static_cast<size_t>( size_s );
      std::unique_ptr<char[]> buf( new char[ size ] );
      std::snprintf( buf.get(), size, format.c_str(), args ... );
      output = std::string( buf.get(), buf.get() + size - 1 );
      return true;
    }

    static std::vector<std::string> split(const std::string& value, const std::string& delimiter) {
      std::vector<std::string> res;
      std::size_t startPos = 0;
      while(startPos < value.size()){
          int pos = value.find(delimiter, startPos);
          if(pos > -1) {
            res.push_back(value.substr(startPos, pos - startPos));
            startPos = pos + delimiter.size();
          } else {
            res.push_back(value.substr(startPos));
            break;
          }
      }
      return res;
    }

    static std::string join(const std::vector<std::string>& data, const std::string& connector) {
      std::string res;
      for (std::size_t i = 0; i < data.size(); i++) {
        if (i) {
          res.append(connector);
        }
        res.append(data[i]);
      }
      return res;
    }

    static std::string replace(const std::string& data, const std::string& oldValue, const std::string& newValue) {
      std::vector<std::string> splitted = split(data, oldValue);
      return join(splitted, newValue);
    }

    static std::string trim(const std::string& s) {
      size_t start = s.find_first_not_of(" \t\n\r");
      if (start == std::string::npos) return "";
      size_t end = s.find_last_not_of(" \t\n\r");
      return s.substr(start, end - start + 1);
    }
};

class UuidUtils {
  public:
    /** Generate UUID v4 */
    static std::string generateUuid() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 255);

        unsigned char bytes[16];
        for (auto &b : bytes) {
            b = static_cast<unsigned char>(dis(gen));
        }

        // Set version to 4 -----
        bytes[6] = (bytes[6] & 0x0F) | 0x40;

        // Set variant to RFC 4122
        bytes[8] = (bytes[8] & 0x3F) | 0x80;

        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        for (int i = 0; i < 16; i++) {
            ss << std::setw(2) << static_cast<int>(bytes[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9)
                ss << "-";
        }

        return ss.str();
    }
};


}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // android_hardware_automotive_vehicle_sockets_CommonUtils_H_
