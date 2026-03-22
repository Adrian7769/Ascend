#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
//   RECEIVED HEX PAYLOAD — 50 bytes total
// ============================================================
//   [0..1]   "RB" header         (modem manufacturer)
//   [2..4]   Serial number       (modem manufacturer, 3 bytes big-endian)
//   [5..49]  TX array            (our 45 bytes from buildIridiumTxArray)
//
//   TX ARRAY v4.2.4 layout at offset +5:
//   [5]      Datalink byte  (0=good BT link, 1=bad BT link)
//   [6..21]  GNSS from BT slave (lastValidBtRx[0..15])
//              [6]  UTC Hours   [7]  UTC Min    [8]  UTC Sec
//              [9]  Lat°  [10] Lat'  [11] Lat"  [12] Lat hemi (0=N 1=S)
//              [13] Lon°  [14] Lon'  [15] Lon"  [16] Lon hemi (0=E 1=W)
//              [17..20] Altitude (4-byte big-endian)
//              [21] Altitude units (0=m 1=ft)
//   [22..37] I2C sensor block (latestI2Cblock[0..15])
//              [22] Radio sweep count (0-255 wrapping)
//              [23..25] AHT20 Hum  (20-bit raw)
//              [26..28] AHT20 Temp (20-bit raw)
//              [29] AHT20 status (2=OK 3=fail 4=timeout)
//              [30..31] Radio current station idx (MSB/LSB)
//              [32] Radio current signal (0-15)
//              [33] Radio stereo flag (1=stereo 0=mono)
//              [34] Datalink byte copy (mirrors TX[5])
//              [35] Radio peak signal this sweep (0-15)
//              [36..37] Radio peak station idx (MSB/LSB)
//   [38]     Reserved (0x00)
//   [39..40] Master battery ADC (MSB/LSB) — analogRead(29) on RP2040
//   [41..42] Internal temp ADC  (MSB/LSB) — fdr.ADCread(port 4)
//   [43..44] External temp ADC  (MSB/LSB) — fdr.ADCread(port 3)
//   [45..46] Slave battery ADC  (MSB/LSB) — slave fdr.ADCread(7) via BT
//   [47]     Current modem return code  (low byte)
//   [48]     Previous modem return code (low byte)
//   [49]     Cumulative TX success count
// ============================================================

namespace PayloadIndex {
    // Manufacturer bytes
    constexpr int HEADER_BYTE_0     = 0;
    constexpr int HEADER_BYTE_1     = 1;
    constexpr int SERIAL_BYTE_0     = 2;
    constexpr int SERIAL_BYTE_1     = 3;
    constexpr int SERIAL_BYTE_2     = 4;

    // TX offset
    constexpr int TX_OFFSET         = 5;
    constexpr int DATALINK_BYTE     = 5;   // 0=good BT link, 1=bad BT link
    constexpr int MODEM_CURRENT_MSB = 38;  // TX[33] — current modem code high byte
    // MODEM_PREVIOUS: use MODEM_PREVIOUS_MSB (48) + MODEM_PREVIOUS_LSB (49) for full 16-bit
    constexpr int MODEM_CURRENT_LSB  = 47;  // TX[42] — current modem code low byte
    constexpr int MODEM_PREVIOUS_MSB = 48;  // TX[43] — previous modem code high byte
    constexpr int MODEM_PREVIOUS_LSB = 49;  // TX[44] — previous modem code low byte

    // GNSS [6..21]
    constexpr int UTC_HOURS         = 6;
    constexpr int UTC_MINUTES       = 7;
    constexpr int UTC_SECONDS       = 8;
    constexpr int LAT_DEGREES       = 9;
    constexpr int LAT_MINUTES       = 10;
    constexpr int LAT_SECONDS       = 11;
    constexpr int LAT_HEMISPHERE    = 12;
    constexpr int LON_DEGREES       = 13;
    constexpr int LON_MINUTES       = 14;
    constexpr int LON_SECONDS       = 15;
    constexpr int LON_HEMISPHERE    = 16;
    constexpr int ALT_BYTE_0        = 17;
    constexpr int ALT_BYTE_1        = 18;
    constexpr int ALT_BYTE_2        = 19;
    constexpr int ALT_BYTE_3        = 20;
    constexpr int ALT_UNITS         = 21;

    // I2C block [22..37]
    constexpr int RADIO_SWEEP_COUNT = 22;  // FM scan sweep counter (wraps 0-255)
    constexpr int AHT20_HUM_0       = 23;
    constexpr int AHT20_HUM_1       = 24;
    constexpr int AHT20_HUM_2       = 25;
    constexpr int AHT20_TEMP_0      = 26;
    constexpr int AHT20_TEMP_1      = 27;
    constexpr int AHT20_TEMP_2      = 28;
    constexpr int AHT20_STATUS      = 29;
    constexpr int RADIO_STATION_MSB = 30;
    constexpr int RADIO_STATION_LSB = 31;
    constexpr int RADIO_SIGNAL      = 32;
    constexpr int RADIO_STEREO      = 33;
    constexpr int I2C_DATALINK      = 34;  // mirrors DATALINK_BYTE at [5]
    constexpr int RADIO_PEAK_SIGNAL = 35;  // highest signal seen this sweep (0-15)
    constexpr int I2C_MODEM_MSB     = 36;  // last modem return code MSB (MODEM 2.0)
    constexpr int I2C_MODEM_LSB     = 37;  // last modem return code LSB — decode: (MSB<<8)|LSB

    // Analog [39..46]
    constexpr int RESERVED_38       = 38;
    constexpr int MASTER_BATT_MSB   = 39;
    constexpr int MASTER_BATT_LSB   = 40;
    constexpr int INT_TEMP_MSB      = 41;
    constexpr int INT_TEMP_LSB      = 42;
    constexpr int EXT_TEMP_MSB      = 43;
    constexpr int EXT_TEMP_LSB      = 44;
    constexpr int SLAVE_BATT_MSB    = 45;
    constexpr int SLAVE_BATT_LSB    = 46;

    // Modem [47..49]
    constexpr int MODEM_CURRENT     = 47;
    constexpr int MODEM_PREVIOUS    = 48;
    constexpr int TX_RESERVED_44    = 44;  // was TX count, now 0x00 — dashboard counts by MOMSN
    constexpr int EXPECTED_SIZE     = 50;
}

namespace SensorCal {
    // ── Master battery ────────────────────────────────────────────────────
    // Source: analogRead(29) on the RP2040 built-in ADC.
    // Formula: BatteryV = raw * ADC_TO_VOLTAGE / MASTER_BATT_CAL
    constexpr float ADC_TO_VOLTAGE    = 0.00322f;   // V/count  — RP2040 analogRead(29) ONLY
    constexpr float MASTER_BATT_CAL   = 0.971f;     // resistor divider ratio for master board

    // ── External FDR ADC (temp sensors) ──────────────────────────────────
    // Source: fdr.ADCread() on the external FDR ADC chip (master board).
    // Back-calculated from MOMSN 77: raw=4284 → 22.2°C board temp.
    constexpr float FDR_ADC_TO_VOLTAGE = 0.00017411f; // V/count — external FDR ADC (master)

    // ── Slave battery ─────────────────────────────────────────────────────
    // Source: slave fdr.ADCread(7), forwarded over BT in lastValidBtRx[16..17].
    // Ground-truth calibration: raw≈3248, Vmeas≈0.609V, actual=6.98V (multimeter).
    //   → fdr.ADCconvertToVoltage factor: 0.609 / 3248 = 0.0001875 V/count
    //   → Voltage divider ratio: 6.98 / 0.609 = 11.46
    //   → Combined: 0.0001875 × 11.46 = 0.002149 V/count
    // Formula: BatteryV = rawADC * SLAVE_BATT_VOLT_PER_COUNT
    constexpr float SLAVE_ADC_TO_VOLTAGE     = 0.0001875f;  // V/count (slave FDR ADC)
    constexpr float SLAVE_BATT_DIVIDER_RATIO = 11.46f;      // voltage divider on slave port 7
    constexpr float SLAVE_BATT_VOLT_PER_COUNT = 0.002149f;  // combined constant (V/count)

    // ── Temperature calibration ───────────────────────────────────────────
    // tempC = (V_measured - OFFSET) / SCALE
    constexpr float INT_TEMP_OFFSET_V = 0.502f;    // V at 0°C, internal sensor
    constexpr float EXT_TEMP_OFFSET_V = 0.491f;    // V at 0°C, external sensor
    constexpr float TEMP_SCALE_VPDC   = 0.011f;    // V per °C

    // ── AHT20 / Radio ─────────────────────────────────────────────────────
    constexpr float AHT20_DIVISOR  = 1048576.0f;   // 2^20 — 20-bit raw to RH/Temp
    constexpr float RADIO_BASE_MHZ = 89.2f;
    constexpr float RADIO_STEP_MHZ = 0.1f;
}

struct AHT20Data {
    float humidityRH = 0.0f;
    float tempC      = 0.0f;
    float tempF      = 0.0f;
    uint8_t status   = 0;
    bool isValid     = false;

    std::string statusString() const {
        switch (status) {
            case 2: return "OK";
            case 3: return "FAIL";
            case 4: return "TIMEOUT";
            case 5: return "NOT READY";
            default: return "UNKNOWN";
        }
    }
};

struct RadioData {
    uint16_t stationIndex     = 0;
    float    frequencyMHz     = 0.0f;
    uint8_t  signalStrength   = 0;
    bool     stereo           = false;
    // Peak signal for this sweep (station index dropped from record format)
    uint8_t  peakSignal  = 0;
    uint8_t  sweepCount  = 0;
};

struct AnalogSensor {
    std::string name;
    float voltage     = 0.0f;
    float measurement = 0.0f;
    std::string unit;
    bool isValid      = false;
};

struct ModemInfo {
    uint16_t currentCode   = 0;  // full 16-bit current code  — TX[33]<<8 | TX[42]
    uint16_t previousCode  = 0;  // full 16-bit previous code — TX[43]<<8 | TX[44]
    uint16_t i2cModemCode  = 0;  // per-record modem code from SEEPROM I2C block [14..15]
    std::string currentDesc;
    std::string previousDesc;
};

struct DMS {
    uint8_t degrees = 0;
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    char hemisphere = 'N';
};

struct PayloadData {
    bool isValid = false;

    // Manufacturer header
    std::string header;
    bool headerValid      = false;
    uint32_t serialNumber = 0;

    // Datalink status (TX[0] / I2C[12])
    uint8_t datalinkByte = 1;  // 0=good BT link, 1=bad BT link
    bool    btLinkGood   = false;

    // GNSS
    bool gnssValid     = false;
    uint8_t utcHours   = 0;
    uint8_t utcMinutes = 0;
    uint8_t utcSeconds = 0;
    DMS latitudeDMS;
    DMS longitudeDMS;
    double latitude  = 0.0;
    double longitude = 0.0;
    int32_t altitude = 0;
    std::string altitudeUnits = "meters";

    // I2C sensors
    AHT20Data aht20;
    RadioData radio;  // radio.sweepCount = byte[0] of I2C block

    // Analog sensors
    AnalogSensor masterBattery;
    AnalogSensor slaveBattery;
    AnalogSensor internalTemp;
    AnalogSensor externalTemp;

    // Modem
    ModemInfo modem;

    std::string toString() const;
};

struct EmailContent {
    bool isValid = false;
    std::string imei;
    int momsn = 0;
    std::string transmitTime;
    double iridiumLatitude  = 0.0;
    double iridiumLongitude = 0.0;
    double iridiumCep       = 0.0;
    int sessionStatus       = 0;
    std::string hexData;
    PayloadData payload;

    std::string toString() const;
};

class Decoder {
public:
    EmailContent parseEmail(const std::string& bodyText);
    PayloadData decodeHexPayload(const std::string& hexString);

    static AnalogSensor decodeMasterBattery(uint16_t rawADC);
    static AnalogSensor decodeSlaveBattery(uint8_t msb, uint8_t lsb);
    static AnalogSensor decodeInternalTemp(uint16_t rawADC);
    static AnalogSensor decodeExternalTemp(uint16_t rawADC);
    static std::string getModemStatusDescription(uint16_t code);

private:
    std::string extractField(const std::string& text, const std::string& fieldName);
    std::vector<uint8_t> hexStringToBytes(const std::string& hex);
};