#include "Decoder.h"
#include "logger.h"
#include <regex> // Regex
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

extern Logger logger;

// Parse the email body and extract all fields
EmailContent Decoder::parseEmail(const std::string& bodyText) {
    EmailContent telemetry;
    try {
        telemetry.imei = extractField(bodyText, "IMEI");
        // Extract MOMSN 
        std::string momsnStr = extractField(bodyText, "MOMSN");
        if (!momsnStr.empty()) {
            telemetry.momsn = std::stoi(momsnStr); // string to int
        }
        // Extract Transmit Time
        telemetry.transmitTime = extractField(bodyText, "Transmit Time");
        // Extract Iridium Latitude
        std::string latStr = extractField(bodyText, "Iridium Latitude");
        if (!latStr.empty()) {
            telemetry.iridiumLatitude = std::stod(latStr); // string to double
        }
        // Extract Iridium Longitude
        std::string lonStr = extractField(bodyText, "Iridium Longitude");
        if (!lonStr.empty()) {
            telemetry.iridiumLongitude = std::stod(lonStr);
        }
        // Extract Iridium CEP 
        std::string cepStr = extractField(bodyText, "Iridium CEP");
        if (!cepStr.empty()) {
            telemetry.iridiumCep = std::stod(cepStr);
        }
        // Extract Iridium Session Status
        std::string statusStr = extractField(bodyText, "Iridium Session Status");
        if (!statusStr.empty()) {
            telemetry.sessionStatus = std::stoi(statusStr);
        }
        // Extract hex data
        telemetry.hexData = extractField(bodyText, "Data");
        // Decode the hex payload
        if (!telemetry.hexData.empty()) {
            telemetry.payload = decodeHexPayload(telemetry.hexData);
        }
        // Mark as valid if we got the essential fields
        if (!telemetry.imei.empty() && !telemetry.hexData.empty()) {
            telemetry.isValid = true;
            logger.log(LOG_INFO, "Successfully parsed telemetry - MOMSN: " + std::to_string(telemetry.momsn));
        } else {
            logger.log(LOG_WARNING, "Parsed telemetry missing essential fields");
        }
        
    } catch (const std::exception& e) {
        logger.log(LOG_ERROR, "Error parsing email: " + std::string(e.what()));
        telemetry.isValid = false;
    }
    return telemetry;
}

// Extract a field value from the email text
std::string Decoder::extractField(const std::string& text, const std::string& fieldName) {
    try {
        std::string pattern = fieldName + ":\\s*([^\\n\\r]+)";
        // Zero or more whitespace \s*
        // () Capture whats inside
        // anything after collon, basically fieldName:**
        std::regex fieldRegex(pattern);
        std::smatch match;
        if (std::regex_search(text, match, fieldRegex)) {
            std::string value = match[1].str();
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\nUTC") + 1);
            return value;
        }
    } catch (const std::exception& e) {
        logger.log(LOG_ERROR, "Error extracting field '" + fieldName + "': " + std::string(e.what()));
    }
    return "";
}

// Convert hex string to byte array
std::vector<uint8_t> Decoder::hexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::string cleanHex;
    for (char c : hex) {
        // range based for loop, "For Each"
        if (std::isxdigit(c)) { //Are we valid hex?
            cleanHex += c;
        }
    }
    // Convert pairs of hex digits to bytes
    // 1 byte = 2 hex digits
    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        if (i + 1 < cleanHex.length()) {
            std::string byteString = cleanHex.substr(i, 2); // get the substring and append it to bytes vector as decimals
            uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16)); // string to unsigned long, parse as base 16 hex
            bytes.push_back(byte);
        }
    }
    return bytes;
}
PayloadData Decoder::decodeHexPayload(const std::string& hexString) {
    PayloadData payload;
    try {
        // Convert hex to bytes
        std::vector<uint8_t> bytes = hexStringToBytes(hexString); // vector of bytes
        logger.log(LOG_INFO, "Decoding hex payload: " + hexString.substr(0, 50) + "... " + "(" + std::to_string(bytes.size()) + " bytes)");
        
        // Validate Hex Array Length (Should be 48 - 50 bytes)
        if (bytes.size() < PayloadConfig::EXPECTED_PAYLOAD_SIZE) {logger.log(LOG_WARNING, "Payload too short! Expected " + std::to_string(PayloadConfig::EXPECTED_PAYLOAD_SIZE) + " bytes, got " + std::to_string(bytes.size()));
            return payload;
        }

        char header1 = static_cast<char>(bytes[PayloadConfig::HEADER_BYTE_0]);
        char header2 = static_cast<char>(bytes[PayloadConfig::HEADER_BYTE_1]);

        // Access the Header at PayloadConfig::HEADER_BYTE_0 within the clean byte array
        payload.header = std::string(1, header1) + std::string(1, header2);
        payload.headerValid = (payload.header == "RB"); // Validate Payload Header
        logger.log(LOG_INFO, "  Header: " + payload.header + (payload.headerValid ? " Valid" : " Not Valid (expected 'RB')"));

        // Reconstruct Payload Serial number 3 bytes 
        payload.serialNumber = (bytes[PayloadConfig::SERIAL_BYTE_0] << 16) | (bytes[PayloadConfig::SERIAL_BYTE_1] << 8) | bytes[PayloadConfig::SERIAL_BYTE_2]; 
        logger.log(LOG_INFO, "  RockBLOCK Serial: " + std::to_string(payload.serialNumber));
        
        // Set UTC Time, HOURS, MIN, SECOND
        payload.utcHours = bytes[PayloadConfig::UTC_HOURS];
        payload.utcMinutes = bytes[PayloadConfig::UTC_MINUTES];
        payload.utcSeconds = bytes[PayloadConfig::UTC_SECONDS];
        logger.log(LOG_INFO, "  UTC Time: " + std::to_string(payload.utcHours) + ":" + std::to_string(payload.utcMinutes) + ":" + std::to_string(payload.utcSeconds));
        
        // SET GPS LAT DATA
        payload.latitudeDMS.degrees = bytes[PayloadConfig::LAT_DEGREES];
        payload.latitudeDMS.minutes = bytes[PayloadConfig::LAT_MINUTES];
        payload.latitudeDMS.seconds = bytes[PayloadConfig::LAT_SECONDS];
        payload.latitudeDMS.hemisphere = (bytes[PayloadConfig::LAT_HEMISPHERE] == 0) ? 'N' : 'S';
        
        // Convert to decimal degrees
        payload.latitude = payload.latitudeDMS.degrees + (payload.latitudeDMS.minutes / 60.0) + (payload.latitudeDMS.seconds / 3600.0);
        if (payload.latitudeDMS.hemisphere == 'S') {
            payload.latitude = -payload.latitude;
        }
        
        logger.log(LOG_INFO, "  Latitude: " + std::to_string(payload.latitudeDMS.degrees) + "° " + std::to_string(payload.latitudeDMS.minutes) + "' " + std::to_string(payload.latitudeDMS.seconds) + "\" " + std::string(1, payload.latitudeDMS.hemisphere) + " = " + std::to_string(payload.latitude) + "°");
        
        // SET GPS LON DATA
        payload.longitudeDMS.degrees = bytes[PayloadConfig::LON_DEGREES];
        payload.longitudeDMS.minutes = bytes[PayloadConfig::LON_MINUTES];
        payload.longitudeDMS.seconds = bytes[PayloadConfig::LON_SECONDS];
        payload.longitudeDMS.hemisphere = (bytes[PayloadConfig::LON_HEMISPHERE] == 0) ? 'E' : 'W';
        
        // Convert to decimal degrees
        payload.longitude = payload.longitudeDMS.degrees + (payload.longitudeDMS.minutes / 60.0) + (payload.longitudeDMS.seconds / 3600.0);
        if (payload.longitudeDMS.hemisphere == 'W') {
            payload.longitude = -payload.longitude;
        }
        
        logger.log(LOG_INFO, "  Longitude: " + std::to_string(payload.longitudeDMS.degrees) + "° " + std::to_string(payload.longitudeDMS.minutes) + "' " + std::to_string(payload.longitudeDMS.seconds) + "\" " + std::string(1, payload.longitudeDMS.hemisphere) + " = " + std::to_string(payload.longitude) + "°");
        
        // ===== ALTITUDE (4 bytes, big-endian int32) =====
        payload.altitude = (bytes[PayloadConfig::ALTITUDE_BYTE_0] << 24) |
                          (bytes[PayloadConfig::ALTITUDE_BYTE_1] << 16) |
                          (bytes[PayloadConfig::ALTITUDE_BYTE_2] << 8) |
                           bytes[PayloadConfig::ALTITUDE_BYTE_3]; //  Reconstruct Altitude Bytes
        payload.altitudeUnits = (bytes[PayloadConfig::ALTITUDE_UNITS] == 0) ? "meters" : "feet";
        logger.log(LOG_INFO, "  Altitude: " + std::to_string(payload.altitude) + " " + payload.altitudeUnits);
        
        // ===== ANALOG SENSOR DATA (8 ports × 2 bytes each) =====
        logger.log(LOG_INFO, "  Analog Sensors:");
        
        // Helper lambda to read 16-bit value (MSB first)
        auto readAnalogPort = [&bytes](int portNumber) -> uint16_t { // lambda function to read &bytes by reference pass in port number return byte
            int baseIndex = PayloadConfig::ANALOG_DATA_BASE + (portNumber * PayloadConfig::BYTES_PER_PORT);
            uint8_t msb = bytes[baseIndex]; // each port is two bytes get MSB
            uint8_t lsb = bytes[baseIndex + 1]; // get second byte which is LSB
            return (msb << 8) | lsb; // return combined
        };
        
        // Decode each sensor port
        payload.internalTemp = decodeInternalTemp(readAnalogPort(PayloadConfig::PORT_INTERNAL_TEMP));
        payload.pressure = decodePressure(readAnalogPort(PayloadConfig::PORT_PRESSURE));

        // This is to decode Team 2 UV Light Sensor Data
        //payload.uvLight = decodeUVLight(readAnalogPort(PayloadConfig::PORT_UV_LIGHT));
        payload.externalTemp = decodeExternalTemp(readAnalogPort(PayloadConfig::PORT_EXTERNAL_TEMP));
        payload.accelY = decodeAccelY(readAnalogPort(PayloadConfig::PORT_ACCEL_Y));
        payload.accelX = decodeAccelX(readAnalogPort(PayloadConfig::PORT_ACCEL_X));
        payload.battery = decodeBattery(readAnalogPort(PayloadConfig::PORT_BATTERY));
        
        // Log each sensor
        auto logSensor = [](const AnalogSensorData& sensor) {
            if (sensor.isValid) { logger.log(LOG_INFO, "    " + sensor.name + ": " + std::to_string(sensor.measurement) + " " + sensor.unit + " (Voltage: " + std::to_string(sensor.voltage) + " V)");
            }
        };
        
        logSensor(payload.internalTemp);
        logSensor(payload.pressure);
        //logSensor(payload.uvLight);
        logSensor(payload.externalTemp);
        logSensor(payload.accelY);
        logSensor(payload.accelX);
        logSensor(payload.battery); 
        
        // Modem Status
        payload.modemStatus = (bytes[PayloadConfig::MODEM_STATUS_MSB] << 8) | bytes[PayloadConfig::MODEM_STATUS_LSB];
        payload.modemStatusDescription = getModemStatusDescription(payload.modemStatus);
        logger.log(LOG_INFO, "  Modem Status: " + std::to_string(payload.modemStatus) + " - " + payload.modemStatusDescription);
        payload.isValid = payload.headerValid;
        
    } catch (const std::exception& e) {
        logger.log(LOG_ERROR, "Error decoding hex payload: " + std::string(e.what()));
        payload.isValid = false;
    }
    
    return payload;
}

/*
AnalogSensorData Decoder::decodeUVLight(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "UV Light";
    sensor.voltage = rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE;
    
    sensor.measurement = sensor.voltage / PayloadConfig::SensorCalibration::UV_SCALE;
    sensor.unit = "UV Index";
    sensor.isValid = true;
    return sensor;
}
*/

// Decode Sensor Values, Pass in the Raw value, which should be 
AnalogSensorData Decoder::decodeInternalTemp(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "Internal Temperature";
    sensor.voltage = rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE;
    float tempC = (sensor.voltage - PayloadConfig::SensorCalibration::INTERNAL_TEMP_OFFSET) /
                  PayloadConfig::SensorCalibration::INTERNAL_TEMP_SCALE;
    float tempF = (tempC * 1.8f) + 32.0f;
    sensor.measurement = tempC;
    sensor.unit = "°C (" + std::to_string(static_cast<int>(tempF)) + "°F)";
    sensor.isValid = true;
    return sensor;
}
AnalogSensorData Decoder::decodePressure(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "Pressure";
    sensor.voltage = rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE;
    
    sensor.measurement = (sensor.voltage - PayloadConfig::SensorCalibration::PRESSURE_OFFSET) /
                         PayloadConfig::SensorCalibration::PRESSURE_SCALE;
    sensor.unit = "PSI";
    sensor.isValid = true;
    return sensor;
}
AnalogSensorData Decoder::decodeExternalTemp(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "External Temperature";
    sensor.voltage = rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE;
    
    float tempC = (sensor.voltage - PayloadConfig::SensorCalibration::EXTERNAL_TEMP_OFFSET) /
                  PayloadConfig::SensorCalibration::EXTERNAL_TEMP_SCALE;
    float tempF = (tempC * 1.8f) + 32.0f;
    
    sensor.measurement = tempC;
    sensor.unit = "°C (" + std::to_string(static_cast<int>(tempF)) + "°F)";
    sensor.isValid = true;
    return sensor;
}
AnalogSensorData Decoder::decodeAccelY(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "Y-axis Accelerometer";
    sensor.voltage = rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE;
    
    sensor.measurement = (sensor.voltage - PayloadConfig::SensorCalibration::ACCEL_Y_OFFSET) /
                         PayloadConfig::SensorCalibration::ACCEL_Y_SCALE;
    sensor.unit = "G";
    sensor.isValid = true;
    return sensor;
}
AnalogSensorData Decoder::decodeAccelX(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "X-axis Accelerometer";
    sensor.voltage = rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE;
    
    sensor.measurement = (sensor.voltage - PayloadConfig::SensorCalibration::ACCEL_X_OFFSET) /
                         PayloadConfig::SensorCalibration::ACCEL_X_SCALE;
    sensor.unit = "G";
    sensor.isValid = true;
    return sensor;
}
AnalogSensorData Decoder::decodeBattery(uint16_t rawValue) {
    AnalogSensorData sensor;
    sensor.name = "Battery";
    sensor.voltage = (rawValue * PayloadConfig::SensorCalibration::ADC_TO_VOLTAGE) /
                        PayloadConfig::SensorCalibration::BATTERY_IDEAL_OFFSET;

    sensor.measurement = sensor.voltage;
    sensor.unit = "V";
    sensor.isValid = true;
    return sensor;
}

std::string Decoder::getModemStatusDescription(uint16_t status) {
    switch (status) {
        // Success codes (400-499)
        case 400: return "Ping Through MPM And Modem Success";
        case 401: return "Modem Ready For Use";
        case 402: return "Transmit Successful And No Receive";
        case 403: return "Transmit And Receive Successful";
        case 404: return "Transmit And Receive Successful Plus Receive Pending";
        case 407: return "Receive Data Placed In Receive Array";
        
        // Status codes (300-399)
        case 300: return "Success Byte After SBDIX";
        case 304: return "OK Found";
        case 311: return "Idle";
        case 313: return "Network Available With Acceptable Signal Strength";
        
        // Error codes (200-299)
        case 200: return "Transmit Has Failed";
        case 290: return "Write To MO Buffer Failed";
        case 291: return "Wrong Modem Connected Check Serial Number";
        
        default:
            if (status >= 200 && status < 300) return "Error Code: " + std::to_string(status);
            if (status >= 300 && status < 400) return "Status Code: " + std::to_string(status);
            if (status >= 400 && status < 500) return "Success Code: " + std::to_string(status);
            return "Unknown: " + std::to_string(status);
    }
}

// ============================================================================
// toString METHODS
// ============================================================================

std::string PayloadData::toString() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << " DECODED PAYLOAD DATA\n";
    ss << "Header:          " << header << (headerValid ? " Valid" : " Not Valid") << "\n";
    ss << "Serial Number:   " << serialNumber << "\n";
    ss << "UTC Time:        " << static_cast<int>(utcHours) << ":" 
       << static_cast<int>(utcMinutes) << ":" << static_cast<int>(utcSeconds) << "\n";
    ss << "Latitude:        " << latitude << "° (" 
       << static_cast<int>(latitudeDMS.degrees) << "° "
       << static_cast<int>(latitudeDMS.minutes) << "' "
       << static_cast<int>(latitudeDMS.seconds) << "\" " 
       << latitudeDMS.hemisphere << ")\n";
    ss << "Longitude:       " << longitude << "° ("
       << static_cast<int>(longitudeDMS.degrees) << "° "
       << static_cast<int>(longitudeDMS.minutes) << "' "
       << static_cast<int>(longitudeDMS.seconds) << "\" "
       << longitudeDMS.hemisphere << ")\n";
    ss << "Altitude:        " << altitude << " " << altitudeUnits << "\n";
    ss << "\n️  SENSORS:\n";
    ss << "  " << internalTemp.name << ": " << internalTemp.measurement << " " << internalTemp.unit << "\n";
    ss << "  " << externalTemp.name << ": " << externalTemp.measurement << " " << externalTemp.unit << "\n";
    ss << "  " << pressure.name << ": " << pressure.measurement << " " << pressure.unit << "\n";
    //ss << "  " << uvLight.name << ": " << uvLight.measurement << " " << uvLight.unit << "\n";
    ss << "  " << accelX.name << ": " << accelX.measurement << " " << accelX.unit << "\n";
    ss << "  " << accelY.name << ": " << accelY.measurement << " " << accelY.unit << "\n";
    ss << "  " << battery.name << ": " << battery.measurement << " " << battery.unit << "\n";
    ss << "\n Modem Status: " << modemStatus << " - " << modemStatusDescription << "\n";
    return ss.str();
}

std::string EmailContent::toString() const {
    std::stringstream ss;
    ss << " BALLOON TELEMETRY (MOMSN: " << momsn << ")\n";
    ss << "IMEI:            " << imei << "\n";
    ss << "Transmit Time:   " << transmitTime << "\n";
    ss << "Iridium Lat:     " << iridiumLatitude << "°\n";
    ss << "Iridium Lon:     " << iridiumLongitude << "°\n";
    ss << "Iridium CEP:     " << iridiumCep << "\n";
    ss << "Session Status:  " << sessionStatus << "\n";
    ss << "\n" << payload.toString();
    return ss.str();
}