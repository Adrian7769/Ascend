#ifndef CONFIG_H
#define CONFIG_H
#include <string>
#include <cstdint>

// This is the program globals that you will modify
const std::string MODEM_EMAIL = "adr2211700@maricopa.edu";
const int POLL_INTERVAL_MINUTES = 1; // You can poll every minute but, we are receiving modem data every 5 minutes so its kinda useless. 3 minutes is a happy middle ground
const std::string PATH_TO_SECRET = "env/client_secret.json";
const std::string PATH_TO_TOKEN = "env/token_cache.json"; //If these two file locations aren't correct, will log an error. Make sure they are correct.
// Specify your data
struct PayloadConfig {

    // Header (ASCII characters)
    static const int HEADER_BYTE_0 = 0;  // Should be 'R' (0x52)
    static const int HEADER_BYTE_1 = 1;  // Should be 'B' (0x42)
    // RockBLOCK Serial Number (3 bytes, big-endian)
    static const int SERIAL_BYTE_0 = 2;  // MSB
    static const int SERIAL_BYTE_1 = 3;
    static const int SERIAL_BYTE_2 = 4;  //LSB

    // *************************************** TEAM 2 GNSS (DATA) *************************************** //

    // UTC Time (3 bytes)
    static const int UTC_HOURS = 5; // if this is greater the 24 its an error
    static const int UTC_MINUTES = 6;
    static const int UTC_SECONDS = 7;
    // GPS Latitude (4 bytes)
    static const int LAT_DEGREES = 8;
    static const int LAT_MINUTES = 9;
    static const int LAT_SECONDS = 10;
    static const int LAT_HEMISPHERE = 11;  // 0=North, 1=South
    // GPS Longitude (4 bytes)
    static const int LON_DEGREES = 12;
    static const int LON_MINUTES = 13;
    static const int LON_SECONDS = 14;
    static const int LON_HEMISPHERE = 15;  // 0=East, 1=West
    // Altitude (4 bytes, big-endian int32)
    static const int ALTITUDE_BYTE_0 = 16;  // MSB
    static const int ALTITUDE_BYTE_1 = 17;
    static const int ALTITUDE_BYTE_2 = 18;
    static const int ALTITUDE_BYTE_3 = 19;  // LSB
    static const int ALTITUDE_UNITS = 20;   // 0=meters, 1=feet

    // *************************************** TEAM 1 ANALOG (DATA) *************************************** //

    // Analog Sensor Data (8 ports, 2 bytes each, MSB first)
    // Each port is in the byte array at: BASE + (port_number * 2)
    static const int ANALOG_DATA_BASE = 21; // At what index does you analog data start
    static const int BYTES_PER_PORT = 2; // Bytes per port
    // ANALOG PORTS TEAM 1
    static const int PORT_INTERNAL_TEMP = 4;
    static const int PORT_PRESSURE = 2;
    static const int PORT_EXTERNAL_TEMP = 3;
    static const int PORT_ACCEL_Y = 6;
    static const int PORT_ACCEL_X = 5;
    static const int PORT_BATTERY = 7;

    // *************************************** RESERVED (DATA) *************************************** //

    // Reserved bytes
    static const int RESERVED_START = 37;
    static const int RESERVED_END = 47;

    // Iridium Modem Status (2 bytes)
    static const int MODEM_STATUS_MSB = 48;
    static const int MODEM_STATUS_LSB = 49;

    // Total expected payload size
    static const int EXPECTED_PAYLOAD_SIZE = 50;  // 50 bytes = 100 hex chars
    // Constant used to validate HEX SIZE

    // Store your Sensor Calibrations HERE!!!!!
    struct SensorCalibration {

        // Analog-to-voltage conversion
        static constexpr float ADC_TO_VOLTAGE = 0.004888f;  // (5V / 1024)

        // These were our test equations numbers
        // Internal Temperature (Port 0)
        static constexpr float INTERNAL_TEMP_OFFSET = 0.502f;
        static constexpr float INTERNAL_TEMP_SCALE = 0.011f;
        // Pressure (Port 1)
        static constexpr float PRESSURE_OFFSET = 1.04f;
        static constexpr float PRESSURE_SCALE = 0.267f;
        // Battery (Port 7)
        static constexpr float BATTERY_IDEAL_OFFSET = 0.971f;
        // External Temperature (Port 4)
        static constexpr float EXTERNAL_TEMP_OFFSET = 0.491f;
        static constexpr float EXTERNAL_TEMP_SCALE = 0.011f;
        // Y-axis Accelerometer (Port 5)
        static constexpr float ACCEL_Y_OFFSET = 1.651f;
        static constexpr float ACCEL_Y_SCALE = 0.224f;
        // X-axis Accelerometer (Port 6)
        static constexpr float ACCEL_X_OFFSET = 1.651f;
        static constexpr float ACCEL_X_SCALE = 0.224f;

    };
};

struct AnalogSensorData {
    float voltage = 0.0f;
    float measurement = 0.0f;
    std::string unit;
    std::string name;
    bool isValid = false;
};

struct PayloadData {
    // Header validation
    std::string header;  // Should be "RB"
    bool headerValid = false;
    
    // RockBLOCK info
    uint32_t serialNumber = 0;
    
    // Time (UTC)
    uint8_t utcHours = 0;
    uint8_t utcMinutes = 0;
    uint8_t utcSeconds = 0;
    
    // GPS Position
    double latitude = 0.0;   // Decimal degrees
    double longitude = 0.0;  // Decimal degrees
    struct {
        uint8_t degrees = 0;
        uint8_t minutes = 0;
        uint8_t seconds = 0;
        char hemisphere = 'N';  // N or S
    } latitudeDMS;
    struct {
        uint8_t degrees = 0;
        uint8_t minutes = 0;
        uint8_t seconds = 0;
        char hemisphere = 'E';  // E or W
    } longitudeDMS;
    
    // Altitude
    int32_t altitude = 0;
    std::string altitudeUnits;  // "meters" or "feet"
    
    // Analog Sensors (8 ports)
    AnalogSensorData internalTemp;
    AnalogSensorData pressure;
    AnalogSensorData uvLight;
    AnalogSensorData unused;
    AnalogSensorData externalTemp;
    AnalogSensorData accelY;
    AnalogSensorData accelX;
    AnalogSensorData battery;
    
    // Modem status
    uint16_t modemStatus = 0;
    std::string modemStatusDescription;
    
    // Overall validity
    bool isValid = false;
    
    std::string toString() const;
};

#endif