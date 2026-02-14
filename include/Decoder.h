#ifndef DECODER_H
#define DECODER_H

#include <vector>
#include <string>
#include <cstdint>
#include "Config.h"
struct EmailContent {
    std::string imei;
    int momsn = 0;
    std::string transmitTime;
    double iridiumLatitude = 0.0;
    double iridiumLongitude = 0.0;
    double iridiumCep = 0.0;
    int sessionStatus = 0;
    std::string hexData;
    PayloadData payload;
    bool isValid = false;
    std::string toString() const;
};

class Decoder {
public:
    // Main parsing function - extracts all data from email body
    EmailContent parseEmail(const std::string& bodyText);
    
    // Decode hex string into payload data (uses PayloadConfig)
    PayloadData decodeHexPayload(const std::string& hexString);
    
private:
    // Helper functions
    std::string extractField(const std::string& text, const std::string& fieldName);
    std::vector<uint8_t> hexStringToBytes(const std::string& hex);
    
    // Sensor-specific decoders (use calibration from PayloadConfig)
    AnalogSensorData decodeInternalTemp(uint16_t rawValue);
    AnalogSensorData decodePressure(uint16_t rawValue);
    //AnalogSensorData decodeUVLight(uint16_t rawValue);
    AnalogSensorData decodeExternalTemp(uint16_t rawValue);
    AnalogSensorData decodeAccelY(uint16_t rawValue);
    AnalogSensorData decodeAccelX(uint16_t rawValue);
    AnalogSensorData decodeBattery(uint16_t rawValue);
    
    std::string getModemStatusDescription(uint16_t status);
};
#endif