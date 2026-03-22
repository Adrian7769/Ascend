#include "Decoder.h"
#include "logger.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <cmath>

extern Logger logger;

// ============================================================
//   EMAIL PARSING (unchanged — same Rockblock email format)
// ============================================================

EmailContent Decoder::parseEmail(const std::string& bodyText) {
    EmailContent telemetry;
    try {
        telemetry.imei = extractField(bodyText, "IMEI");

        std::string momsnStr = extractField(bodyText, "MOMSN");
        if (!momsnStr.empty()) telemetry.momsn = std::stoi(momsnStr);

        telemetry.transmitTime = extractField(bodyText, "Transmit Time");

        std::string latStr = extractField(bodyText, "Iridium Latitude");
        if (!latStr.empty()) telemetry.iridiumLatitude = std::stod(latStr);

        std::string lonStr = extractField(bodyText, "Iridium Longitude");
        if (!lonStr.empty()) telemetry.iridiumLongitude = std::stod(lonStr);

        std::string cepStr = extractField(bodyText, "Iridium CEP");
        if (!cepStr.empty()) telemetry.iridiumCep = std::stod(cepStr);

        std::string statusStr = extractField(bodyText, "Iridium Session Status");
        if (!statusStr.empty()) telemetry.sessionStatus = std::stoi(statusStr);

        telemetry.hexData = extractField(bodyText, "Data");

        if (!telemetry.hexData.empty()) {
            telemetry.payload = decodeHexPayload(telemetry.hexData);
        }

        if (!telemetry.imei.empty() && !telemetry.hexData.empty()) {
            telemetry.isValid = true;
            logger.log(LOG_INFO, "Parsed telemetry - MOMSN: " + std::to_string(telemetry.momsn));
        } else {
            logger.log(LOG_WARNING, "Parsed telemetry missing essential fields");
        }
    } catch (const std::exception& e) {
        logger.log(LOG_ERROR, "Error parsing email: " + std::string(e.what()));
        telemetry.isValid = false;
    }
    return telemetry;
}

std::string Decoder::extractField(const std::string& text, const std::string& fieldName) {
    try {
        std::string pattern = fieldName + ":\\s*([^\\n\\r]+)";
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

std::vector<uint8_t> Decoder::hexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::string cleanHex;
    for (char c : hex) {
        if (std::isxdigit(c)) cleanHex += c;
    }
    for (size_t i = 0; i + 1 < cleanHex.length(); i += 2) {
        std::string byteString = cleanHex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// ============================================================
//   HEX PAYLOAD DECODE — 50-byte frame (5 mfr + 45 TX v4.2)
// ============================================================

PayloadData Decoder::decodeHexPayload(const std::string& hexString) {
    PayloadData payload;
    try {
        std::vector<uint8_t> b = hexStringToBytes(hexString);
        logger.log(LOG_INFO, "Decoding hex payload: " + hexString.substr(0, 50) +
            "... (" + std::to_string(b.size()) + " bytes)");

        if (static_cast<int>(b.size()) < PayloadIndex::EXPECTED_SIZE) {
            logger.log(LOG_WARNING, "Payload too short: expected " +
                std::to_string(PayloadIndex::EXPECTED_SIZE) + " bytes, got " +
                std::to_string(b.size()));
            return payload;
        }

        // ===== [0..1] MANUFACTURER HEADER =====
        char h0 = static_cast<char>(b[PayloadIndex::HEADER_BYTE_0]);
        char h1 = static_cast<char>(b[PayloadIndex::HEADER_BYTE_1]);
        payload.header = std::string(1, h0) + std::string(1, h1);
        payload.headerValid = (payload.header == "RB");
        logger.log(LOG_INFO, "  Header: " + payload.header +
            (payload.headerValid ? " Valid" : " Not Valid (expected 'RB')"));

        // ===== [2..4] SERIAL NUMBER =====
        payload.serialNumber = (b[PayloadIndex::SERIAL_BYTE_0] << 16) |
                               (b[PayloadIndex::SERIAL_BYTE_1] << 8)  |
                                b[PayloadIndex::SERIAL_BYTE_2];
        logger.log(LOG_INFO, "  RockBLOCK Serial: " + std::to_string(payload.serialNumber));

        // ===== [5] DATALINK BYTE =====
        payload.datalinkByte = b[PayloadIndex::DATALINK_BYTE];
        payload.btLinkGood   = (payload.datalinkByte == 0);
        logger.log(LOG_INFO, "  Datalink: " + std::string(payload.btLinkGood ? "GOOD" : "BAD") +
            " (raw=0x" +
            ([](uint8_t v){ std::stringstream s; s << std::hex << std::uppercase
            << std::setw(2) << std::setfill('0') << (int)v; return s.str(); })
            (payload.datalinkByte) + ")");

        // ===== [6..21] GNSS DATA FROM BT SLAVE =====
        payload.utcHours   = b[PayloadIndex::UTC_HOURS];
        payload.utcMinutes = b[PayloadIndex::UTC_MINUTES];
        payload.utcSeconds = b[PayloadIndex::UTC_SECONDS];
        logger.log(LOG_INFO, "  UTC: " + std::to_string(payload.utcHours) + ":" +
            std::to_string(payload.utcMinutes) + ":" + std::to_string(payload.utcSeconds));

        // Latitude DMS
        payload.latitudeDMS.degrees   = b[PayloadIndex::LAT_DEGREES];
        payload.latitudeDMS.minutes   = b[PayloadIndex::LAT_MINUTES];
        payload.latitudeDMS.seconds   = b[PayloadIndex::LAT_SECONDS];
        payload.latitudeDMS.hemisphere = (b[PayloadIndex::LAT_HEMISPHERE] == 0) ? 'N' : 'S';
        payload.latitude = payload.latitudeDMS.degrees
                         + (payload.latitudeDMS.minutes / 60.0)
                         + (payload.latitudeDMS.seconds / 3600.0);
        if (payload.latitudeDMS.hemisphere == 'S') payload.latitude = -payload.latitude;

        // Longitude DMS
        payload.longitudeDMS.degrees   = b[PayloadIndex::LON_DEGREES];
        payload.longitudeDMS.minutes   = b[PayloadIndex::LON_MINUTES];
        payload.longitudeDMS.seconds   = b[PayloadIndex::LON_SECONDS];
        payload.longitudeDMS.hemisphere = (b[PayloadIndex::LON_HEMISPHERE] == 0) ? 'E' : 'W';
        payload.longitude = payload.longitudeDMS.degrees
                          + (payload.longitudeDMS.minutes / 60.0)
                          + (payload.longitudeDMS.seconds / 3600.0);
        if (payload.longitudeDMS.hemisphere == 'W') payload.longitude = -payload.longitude;

        logger.log(LOG_INFO, "  Lat: " + std::to_string(payload.latitude) +
            "  Lon: " + std::to_string(payload.longitude));

        // Altitude (4 bytes big-endian)
        payload.altitude = (static_cast<int32_t>(b[PayloadIndex::ALT_BYTE_0]) << 24) |
                           (static_cast<int32_t>(b[PayloadIndex::ALT_BYTE_1]) << 16) |
                           (static_cast<int32_t>(b[PayloadIndex::ALT_BYTE_2]) <<  8) |
                            static_cast<int32_t>(b[PayloadIndex::ALT_BYTE_3]);
        payload.altitudeUnits = (b[PayloadIndex::ALT_UNITS] == 0) ? "meters" : "feet";
        logger.log(LOG_INFO, "  Altitude: " + std::to_string(payload.altitude) + " " + payload.altitudeUnits);

        // ===== GNSS SANITY CHECKS =====
        // The BT slave can send junk data. We still log everything
        // but only set gnssValid=true if ALL checks pass.
        {
            bool sane = true;
            // UTC range checks
            if (payload.utcHours > 23) {
                logger.log(LOG_WARNING, "  GNSS SANITY: UTC hours=" + std::to_string(payload.utcHours) + " > 23");
                sane = false;
            }
            if (payload.utcMinutes > 59) {
                logger.log(LOG_WARNING, "  GNSS SANITY: UTC minutes=" + std::to_string(payload.utcMinutes) + " > 59");
                sane = false;
            }
            if (payload.utcSeconds > 59) {
                logger.log(LOG_WARNING, "  GNSS SANITY: UTC seconds=" + std::to_string(payload.utcSeconds) + " > 59");
                sane = false;
            }
            // Latitude: degrees 0-90, minutes 0-59, seconds 0-59
            if (payload.latitudeDMS.degrees > 90) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lat degrees=" + std::to_string(payload.latitudeDMS.degrees) + " > 90");
                sane = false;
            }
            if (payload.latitudeDMS.minutes > 59) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lat minutes=" + std::to_string(payload.latitudeDMS.minutes) + " > 59");
                sane = false;
            }
            if (payload.latitudeDMS.seconds > 59) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lat seconds=" + std::to_string(payload.latitudeDMS.seconds) + " > 59");
                sane = false;
            }
            // Longitude: degrees 0-180, minutes 0-59, seconds 0-59
            if (payload.longitudeDMS.degrees > 180) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lon degrees=" + std::to_string(payload.longitudeDMS.degrees) + " > 180");
                sane = false;
            }
            if (payload.longitudeDMS.minutes > 59) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lon minutes=" + std::to_string(payload.longitudeDMS.minutes) + " > 59");
                sane = false;
            }
            if (payload.longitudeDMS.seconds > 59) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lon seconds=" + std::to_string(payload.longitudeDMS.seconds) + " > 59");
                sane = false;
            }
            // Hemisphere bytes must be 0 or 1
            if (b[PayloadIndex::LAT_HEMISPHERE] > 1) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lat hemisphere byte=" + std::to_string(b[PayloadIndex::LAT_HEMISPHERE]) + " (expected 0 or 1)");
                sane = false;
            }
            if (b[PayloadIndex::LON_HEMISPHERE] > 1) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Lon hemisphere byte=" + std::to_string(b[PayloadIndex::LON_HEMISPHERE]) + " (expected 0 or 1)");
                sane = false;
            }
            // Altitude units byte must be 0 or 1
            if (b[PayloadIndex::ALT_UNITS] > 1) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Alt units byte=" + std::to_string(b[PayloadIndex::ALT_UNITS]) + " (expected 0 or 1)");
                sane = false;
            }
            // Altitude sanity: reject obviously absurd values (> 50,000 m or < -1000 m)
            if (payload.altitude > 50000 || payload.altitude < -1000) {
                logger.log(LOG_WARNING, "  GNSS SANITY: Altitude=" + std::to_string(payload.altitude) + " out of range");
                sane = false;
            }
            // All-zero GNSS block means no lock
            if (payload.utcHours == 0 && payload.utcMinutes == 0 && payload.utcSeconds == 0 &&
                payload.latitudeDMS.degrees == 0 && payload.latitudeDMS.minutes == 0 &&
                payload.longitudeDMS.degrees == 0 && payload.longitudeDMS.minutes == 0) {
                logger.log(LOG_WARNING, "  GNSS SANITY: All-zero GNSS — no satellite lock");
                sane = false;
            }

            payload.gnssValid = sane;
            logger.log(LOG_INFO, "  GNSS Valid: " + std::string(sane ? "YES" : "NO — junk data, will not plot"));
        }

        // ===== [22..37] I2C SENSOR BLOCK =====
        payload.radio.sweepCount = b[PayloadIndex::RADIO_SWEEP_COUNT];

        // AHT20 Humidity: 20-bit raw -> RH%
        uint32_t rawHum = (static_cast<uint32_t>(b[PayloadIndex::AHT20_HUM_0]) << 16) |
                          (static_cast<uint32_t>(b[PayloadIndex::AHT20_HUM_1]) <<  8) |
                           static_cast<uint32_t>(b[PayloadIndex::AHT20_HUM_2]);
        payload.aht20.humidityRH = (rawHum * 100.0f) / SensorCal::AHT20_DIVISOR;

        // AHT20 Temperature: 20-bit raw -> C
        uint32_t rawTemp = (static_cast<uint32_t>(b[PayloadIndex::AHT20_TEMP_0]) << 16) |
                           (static_cast<uint32_t>(b[PayloadIndex::AHT20_TEMP_1]) <<  8) |
                            static_cast<uint32_t>(b[PayloadIndex::AHT20_TEMP_2]);
        payload.aht20.tempC = (rawTemp * 200.0f / SensorCal::AHT20_DIVISOR) - 50.0f;
        payload.aht20.tempF = payload.aht20.tempC * 9.0f / 5.0f + 32.0f;
        payload.aht20.status = b[PayloadIndex::AHT20_STATUS];
        payload.aht20.isValid = (payload.aht20.status == 2);

        logger.log(LOG_INFO, "  AHT20: " + std::to_string(payload.aht20.humidityRH) + "% RH, " +
            std::to_string(payload.aht20.tempC) + " C (" + std::to_string(payload.aht20.tempF) +
            " F) [" + payload.aht20.statusString() + "]");

        // Radio scanner — current station
        payload.radio.stationIndex = (static_cast<uint16_t>(b[PayloadIndex::RADIO_STATION_MSB]) << 8) |
                                      static_cast<uint16_t>(b[PayloadIndex::RADIO_STATION_LSB]);
        payload.radio.frequencyMHz = SensorCal::RADIO_BASE_MHZ +
                                     (payload.radio.stationIndex * SensorCal::RADIO_STEP_MHZ);
        payload.radio.signalStrength = b[PayloadIndex::RADIO_SIGNAL];
        payload.radio.stereo = (b[PayloadIndex::RADIO_STEREO] != 0);

        // Radio scanner — peak signal this sweep
        payload.radio.peakSignal = b[PayloadIndex::RADIO_PEAK_SIGNAL];

        // MODEM 2.0 — modem code stored in I2C block bytes [14..15]
        uint16_t i2cModemCode = (static_cast<uint16_t>(b[PayloadIndex::I2C_MODEM_MSB]) << 8) |
                                 static_cast<uint16_t>(b[PayloadIndex::I2C_MODEM_LSB]);
        payload.modem.i2cModemCode = i2cModemCode;  // per-record modem code from SEEPROM

        logger.log(LOG_INFO, "  Radio: " + std::to_string(payload.radio.frequencyMHz) +
            " MHz, signal=" + std::to_string(payload.radio.signalStrength) + "/15" +
            (payload.radio.stereo ? " STEREO" : " MONO") +
            " | sweep#=" + std::to_string(payload.radio.sweepCount) +
            " peakSig=" + std::to_string(payload.radio.peakSignal));

        // ===== [39..46] ANALOG SENSORS =====
        uint16_t rawMBatt = (static_cast<uint16_t>(b[PayloadIndex::MASTER_BATT_MSB]) << 8) |
                             static_cast<uint16_t>(b[PayloadIndex::MASTER_BATT_LSB]);
        payload.masterBattery = decodeMasterBattery(rawMBatt);

        uint16_t rawIntT = (static_cast<uint16_t>(b[PayloadIndex::INT_TEMP_MSB]) << 8) |
                            static_cast<uint16_t>(b[PayloadIndex::INT_TEMP_LSB]);
        payload.internalTemp = decodeInternalTemp(rawIntT);

        uint16_t rawExtT = (static_cast<uint16_t>(b[PayloadIndex::EXT_TEMP_MSB]) << 8) |
                            static_cast<uint16_t>(b[PayloadIndex::EXT_TEMP_LSB]);
        payload.externalTemp = decodeExternalTemp(rawExtT);

        payload.slaveBattery = decodeSlaveBattery(b[PayloadIndex::SLAVE_BATT_MSB],
                                                   b[PayloadIndex::SLAVE_BATT_LSB]);

        logger.log(LOG_INFO, "  Master Batt: " + std::to_string(payload.masterBattery.measurement) + " V");
        logger.log(LOG_INFO, "  Slave Batt:  " + std::to_string(payload.slaveBattery.measurement) + " V");
        logger.log(LOG_INFO, "  Int Temp:    " + std::to_string(payload.internalTemp.measurement) + " C");
        logger.log(LOG_INFO, "  Ext Temp:    " + std::to_string(payload.externalTemp.measurement) + " C");

        // ===== MODEM STATUS =====
        // Full 16-bit current code: MSB at TX[33]=payload[38], LSB at TX[42]=payload[47]
        uint16_t modemFull = (static_cast<uint16_t>(b[PayloadIndex::MODEM_CURRENT_MSB]) << 8) |
                              static_cast<uint16_t>(b[PayloadIndex::MODEM_CURRENT_LSB]);
        payload.modem.currentCode  = modemFull;
        uint16_t modemPrevFull = (static_cast<uint16_t>(b[PayloadIndex::MODEM_PREVIOUS_MSB]) << 8) |
                                   static_cast<uint16_t>(b[PayloadIndex::MODEM_PREVIOUS_LSB]);
        payload.modem.previousCode = modemPrevFull;
        payload.modem.currentDesc    = getModemStatusDescription(payload.modem.currentCode);
        payload.modem.previousDesc   = getModemStatusDescription(payload.modem.previousCode);

        logger.log(LOG_INFO, "  Modem: cur=" + std::to_string(payload.modem.currentCode) +
            " (" + payload.modem.currentDesc + "), prev=" + std::to_string(payload.modem.previousCode) +
            " (" + payload.modem.previousDesc + ")");

        payload.isValid = payload.headerValid;

    } catch (const std::exception& e) {
        logger.log(LOG_ERROR, "Error decoding hex payload: " + std::string(e.what()));
        payload.isValid = false;
    }
    return payload;
}

// ============================================================
//   ANALOG SENSOR DECODERS
// ============================================================

AnalogSensor Decoder::decodeMasterBattery(uint16_t rawADC) {
    AnalogSensor s;
    s.name = "Master Battery";
    // Source: analogRead(29) on the RP2040 built-in ADC.
    // ADC_TO_VOLTAGE = 0.00322 V/count; MASTER_BATT_CAL = 0.971 (divider ratio).
    s.voltage = rawADC * SensorCal::ADC_TO_VOLTAGE;
    s.measurement = s.voltage / SensorCal::MASTER_BATT_CAL;
    s.unit = "V";
    s.isValid = true;
    return s;
}

AnalogSensor Decoder::decodeSlaveBattery(uint8_t msb, uint8_t lsb) {
    AnalogSensor s;
    s.name = "Slave Battery";
    uint16_t rawADC = (static_cast<uint16_t>(msb) << 8) | static_cast<uint16_t>(lsb);
    // Ground-truth calibration (bench measurement):
    //   raw≈3248 → fdr.ADCconvertToVoltage → Vmeas≈0.609V → actual=6.98V (multimeter)
    //   Slave ADC conversion: 0.609 / 3248 = 0.0001875 V/count
    //   Voltage divider on slave port 7: 6.98 / 0.609 = 11.46
    //   Combined constant: 0.0001875 × 11.46 = 0.002149 V/count
    // The old formula (FDR_ADC_TO_VOLTAGE × 1.049) gave ~0.63V — incorrect.
    s.voltage     = rawADC * SensorCal::SLAVE_ADC_TO_VOLTAGE;         // Vmeasured at ADC pin
    s.measurement = rawADC * SensorCal::SLAVE_BATT_VOLT_PER_COUNT;    // true battery voltage
    s.unit = "V";
    s.isValid = true;
    return s;
}

AnalogSensor Decoder::decodeInternalTemp(uint16_t rawADC) {
    AnalogSensor s;
    s.name = "Internal Temperature";
    // Source: fdr.ADCread(ADC_PORT_INT_TEMP) — external fdr ADC chip.
    // Must use FDR_ADC_TO_VOLTAGE, NOT ADC_TO_VOLTAGE.
    // Using ADC_TO_VOLTAGE (0.00322) inflated voltage ~19× → ~1200°C instead of ~22°C.
    s.voltage = rawADC * SensorCal::FDR_ADC_TO_VOLTAGE;
    float tempC = (s.voltage - SensorCal::INT_TEMP_OFFSET_V) / SensorCal::TEMP_SCALE_VPDC;
    float tempF = tempC * 1.8f + 32.0f;
    s.measurement = tempC;
    s.unit = "°C (" + std::to_string(static_cast<int>(tempF)) + "°F)";
    s.isValid = true;
    return s;
}

AnalogSensor Decoder::decodeExternalTemp(uint16_t rawADC) {
    AnalogSensor s;
    s.name = "External Temperature";
    // Source: fdr.ADCread(ADC_PORT_EXT_TEMP) — external fdr ADC chip.
    // Same fix as decodeInternalTemp: must use FDR_ADC_TO_VOLTAGE, not ADC_TO_VOLTAGE.
    s.voltage = rawADC * SensorCal::FDR_ADC_TO_VOLTAGE;
    float tempC = (s.voltage - SensorCal::EXT_TEMP_OFFSET_V) / SensorCal::TEMP_SCALE_VPDC;
    float tempF = tempC * 1.8f + 32.0f;
    s.measurement = tempC;
    s.unit = "°C (" + std::to_string(static_cast<int>(tempF)) + "°F)";
    s.isValid = true;
    return s;
}

// ============================================================
//   MODEM STATUS DESCRIPTIONS
//
//   Accepts the full 16-bit return code — no truncation.
//
//   Code ranges:
//     0-65    Iridium MO status codes (from modem hardware)
//     100-299 Library failure codes   (from Iridium.ino)
//     300-399 Library status codes    (from Iridium.ino)
//     400-499 Library success codes   (from Iridium.ino)
//
//   WHY OLD CODES LOOKED WRONG: the previous implementation stored
//   only the low byte (code & 0xFF), so 400→144, 300→44, 290→34, etc.
//   All descriptions are now keyed on the real full code value.
// ============================================================

std::string Decoder::getModemStatusDescription(uint16_t code) {
    switch (code) {

        // ── Iridium MO Status Codes (hardware, 0-65) ──────────────────────
        case 0:   return "MO transfer successful";
        case 1:   return "MO success — MT message too large to transfer";
        case 2:   return "MO success — Location Update not accepted";
        case 3:   return "Reserved (MO success)";
        case 4:   return "Reserved (MO success)";
        case 5:   return "Reserved (MO failure)";
        case 6:   return "Reserved (MO failure)";
        case 7:   return "Reserved (MO failure)";
        case 8:   return "Reserved (MO failure)";
        case 10:  return "GSS: call did not complete in allowed time";
        case 11:  return "GSS: MO message queue full";
        case 12:  return "GSS: MO message has too many segments";
        case 13:  return "GSS: session did not complete";
        case 14:  return "GSS: invalid segment size";
        case 15:  return "GSS: access denied";
        case 16:  return "ISU locked — cannot make SBD calls";
        case 17:  return "Gateway not responding (local session timeout)";
        case 18:  return "Connection lost (RF drop)";
        case 19:  return "Link failure (protocol error)";
        case 32:  return "No network service — unable to initiate call";
        case 33:  return "Antenna fault — unable to initiate call";
        case 34:  return "Radio disabled — unable to initiate call";
        case 35:  return "ISU busy — unable to initiate call";
        case 36:  return "Try later — must wait 3 min since last registration";
        case 37:  return "SBD service temporarily disabled";
        case 38:  return "Try later — traffic management period";
        case 64:  return "Band violation — transmit outside permitted band";
        case 65:  return "PLL lock failure — hardware error during transmit";

        // ── Library Failure Codes (100-299) ───────────────────────────────
        case 100: return "Failure after SBDIX";
        case 101: return "Modem status timed out";
        case 104: return "Unexpected MO status value";
        case 105: return "Unexpected MOMSN value";
        case 106: return "Unexpected MT status value";
        case 107: return "Unexpected MTMSN status value";
        case 108: return "Unexpected MT SBD message length";
        case 109: return "Unexpected MT SBD message queued value";
        case 112: return "Modem failure after SBDIX";
        case 113: return "Unexpected response to SBDIX";
        case 114: return "Timeout after SBDIX";
        case 116: return "Timeout after sending message size";
        case 118: return "MO buffer cleared — error response";
        case 119: return "MO buffer cleared — timed out";
        case 120: return "Invalid command";
        case 200: return "MO buffer cleared — unexpected response";
        case 202: return "MT buffer cleared — error response";
        case 203: return "MT buffer cleared — timed out";
        case 204: return "MT buffer cleared — unexpected response";
        case 206: return "Disable flow control — timed out";
        case 207: return "Disable flow control — unexpected response";
        case 208: return "Disable SBD ring setup — timed out";
        case 209: return "Disable SBD ring setup — unexpected response";
        case 231: return "Ring indication erroneously enabled";
        case 232: return "Timeout waiting for verify-disable MT alert";
        case 233: return "Unexpected response to SBDMTA";
        case 234: return "Unexpected response after sending message size";
        case 236: return "Setup failed — timed out";
        case 237: return "Setup failed — unexpected response";
        case 238: return "Network status — unexpected response";
        case 239: return "Network status — timed out (no valid response)";
        case 240: return "Network not available";
        case 242: return "MT message — unexpected response";
        case 243: return "MT message — timed out";
        case 244: return "MT message — failed checksum";
        case 245: return "MT message too long";
        case 249: return "Modem setup failed — timed out";
        case 250: return "No modem connected";
        case 251: return "Unexpected modem connected";
        case 255: return "Duplicate transmit attempted";
        case 256: return "Transmit requested too soon — try later";
        case 257: return "Flow control setup — timed out";
        case 258: return "Flow control setup — unexpected response";
        case 259: return "Store configuration — timed out";
        case 260: return "Store configuration — unexpected response";
        case 261: return "Select profile — timed out";
        case 262: return "Select profile — unexpected response";
        case 263: return "Ring indication — unexpected response";
        case 264: return "OK search timed out";
        case 265: return "OK — unexpected response";
        case 269: return "Signal strength too low";
        case 272: return "Transmit successful but receive failed";
        case 273: return "Unexpected modem command";
        case 274: return "MPM busy — transmit command rejected";
        case 275: return "Ping to MPM timed out";
        case 276: return "Ping to MPM success but ping to modem failed";
        case 278: return "Initial setup modem status value";
        case 279: return "Timeout waiting for receive data";
        case 280: return "No ping — MPM busy";
        case 282: return "Unexpected response from modem during setup";
        case 283: return "Modem setup failed — MPM busy";
        case 284: return "Modem failed at setup — timed out";
        case 285: return "MPM busy when FDR asked for setup";
        case 286: return "MPM did not respond to request for data";
        case 287: return "Software error 1";
        case 288: return "MPM busy";
        case 289: return "Asked for ping result too soon — do ping again";
        case 290: return "Ping to MPM did not respond";
        case 291: return "Wrong modem connected — check serial number";
        case 292: return "Requested transmit too soon";
        case 293: return "No functioning modem present";
        case 294: return "Timeout after sending message";
        case 295: return "SBD message timed out by modem";
        case 296: return "SBD message checksum wrong";
        case 297: return "SBD message size wrong";
        case 298: return "Unexpected response after writing to MO buffer";
        case 299: return "SBD message size too big or too small";

        // ── Library Status Codes (300-399) ────────────────────────────────
        case 300: return "Success byte after SBDIX";
        case 301: return "Message size accepted";
        case 302: return "MO buffer cleared successfully";
        case 303: return "MT buffer cleared successfully";
        case 304: return "OK found";
        case 305: return "Ring indication disabled";
        case 306: return "Setup successful";
        case 307: return "MT message retrieved correctly";
        case 308: return "MT message is null";
        case 309: return "Modem setup successful";
        case 310: return "Correct modem connected";
        case 313: return "Network available — acceptable signal strength";
        case 314: return "Verify disable MT alert";
        case 315: return "Flushed UART buffer";
        case 316: return "Array sent to modem";
        case 317: return "Initiate transmit and receive";
        case 318: return "Tell modem to clear MO buffer";
        case 319: return "Tell modem to clear MT buffer";
        case 320: return "Told modem to give us the received message";
        case 322: return "Transmission process has begun";
        case 323: return "Sent PerformTransmit";
        case 324: return "Sent getReceivedData";
        case 326: return "About to start transmit process";
        case 327: return "Waiting for OK from modem";
        case 330: return "Busy setting up modem";
        case 331: return "Performing ping";
        case 333: return "Ping not running";
        case 334: return "MPM busy — transmit command pending";
        case 335: return "Modem setup proceeding";
        case 336: return "Modem defaults set";
        case 337: return "Sent ping";
        case 338: return "SBD message successfully written";
        case 339: return "MT message pending";
        case 340: return "MT messages pending";

        // ── Library Success Codes (400-499) ───────────────────────────────
        case 400: return "Ping through MPM and modem — success";
        case 401: return "Modem ready for use";
        case 402: return "Transmit successful — no receive";
        case 403: return "Transmit and receive successful";
        case 404: return "TX+RX successful — receive pending";
        case 405: return "Data loop-around enabled";
        case 406: return "Data loop-around disabled";
        case 407: return "Receive data placed in receive array";

        case 0xFFFF: return "No status yet";
        default: {
            if (code >= 20  && code <= 31)  return "Reserved (MO failure range 20-31)";
            if (code >= 39  && code <= 63)  return "Reserved (MO failure range 39-63)";
            if (code >= 300 && code <= 399) return "Status code " + std::to_string(code);
            if (code >= 400 && code <= 499) return "Success code " + std::to_string(code);
            if (code >= 100 && code <= 299) return "Failure code " + std::to_string(code);
            return "Unknown code " + std::to_string(code);
        }
    }
}

// ============================================================
//   toString
// ============================================================

std::string PayloadData::toString() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << " DECODED PAYLOAD (v4.2.4 — 50 byte frame: 5 mfr + 45 TX)\n";
    ss << "Header:          " << header << (headerValid ? " Valid" : " Not Valid") << "\n";
    ss << "Serial Number:   " << serialNumber << "\n";
    ss << "BT Datalink:     " << (btLinkGood ? "GOOD" : "BAD")
       << "  (raw=0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
       << (int)datalinkByte << std::dec << ")\n";

    ss << "\nGNSS: " << (gnssValid ? "(VALID)" : "(INVALID — junk data)") << "\n";
    ss << "  UTC:       " << (int)utcHours << ":" << (int)utcMinutes << ":" << (int)utcSeconds << "\n";
    ss << "  Latitude:  " << latitude << " ("
       << (int)latitudeDMS.degrees << "d " << (int)latitudeDMS.minutes << "' "
       << (int)latitudeDMS.seconds << "\" " << latitudeDMS.hemisphere << ")\n";
    ss << "  Longitude: " << longitude << " ("
       << (int)longitudeDMS.degrees << "d " << (int)longitudeDMS.minutes << "' "
       << (int)longitudeDMS.seconds << "\" " << longitudeDMS.hemisphere << ")\n";
    ss << "  Altitude:  " << altitude << " " << altitudeUnits << "\n";

    ss << "\nI2C Sensors:\n";
    ss << "  AHT20:     " << aht20.humidityRH << "% RH, "
       << aht20.tempC << " C (" << aht20.tempF << " F) [" << aht20.statusString() << "]\n";
    ss << "  Radio:     " << radio.frequencyMHz << " MHz, signal "
       << (int)radio.signalStrength << "/15 " << (radio.stereo ? "STEREO" : "MONO")
       << "  sweep#=" << (int)radio.sweepCount << "\n";
    ss << "  Radio Peak: " << radio.peakFrequencyMHz << " MHz, signal "
       << (int)radio.peakSignal << "/15\n";

    ss << "\nAnalog Sensors:\n";
    ss << "  " << masterBattery.name << ": " << masterBattery.measurement << " " << masterBattery.unit << "\n";
    ss << "  " << slaveBattery.name  << ": " << slaveBattery.measurement  << " " << slaveBattery.unit << "\n";
    ss << "  " << internalTemp.name  << ": " << internalTemp.measurement  << " " << internalTemp.unit << "\n";
    ss << "  " << externalTemp.name  << ": " << externalTemp.measurement  << " " << externalTemp.unit << "\n";

    ss << "\nModem:\n";
    ss << "  Current:   " << modem.currentCode << " — " << modem.currentDesc << "\n";
    ss << "  Previous:  " << modem.previousCode << " — " << modem.previousDesc << "\n";

    return ss.str();
}

std::string EmailContent::toString() const {
    std::stringstream ss;
    ss << " BALLOON TELEMETRY (MOMSN: " << momsn << ")\n";
    ss << "IMEI:            " << imei << "\n";
    ss << "Transmit Time:   " << transmitTime << "\n";
    ss << "Iridium Lat:     " << iridiumLatitude << "\n";
    ss << "Iridium Lon:     " << iridiumLongitude << "\n";
    ss << "Iridium CEP:     " << iridiumCep << "\n";
    ss << "Session Status:  " << sessionStatus << "\n";
    ss << "\n" << payload.toString();
    return ss.str();
}