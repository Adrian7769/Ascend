#include "GmailAuth.h"
#include "GmailClient.h"
#include "logger.h"
#include "config.h"
#include "Decoder.h"
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
Logger logger("logs/debug.log", LOG_DEBUG);
void exportTelemetryToJson(const std::vector<EmailContent>& telemetryHistory) {
    try {
        json telemetryArray = json::array();
        for (const auto& telemetry : telemetryHistory) {
            json entry = {
                {"momsn", telemetry.momsn},
                {"imei", telemetry.imei},
                {"transmitTime", telemetry.transmitTime},
                {"iridiumLatitude", telemetry.iridiumLatitude},
                {"iridiumLongitude", telemetry.iridiumLongitude},
                {"iridiumCep", telemetry.iridiumCep},
                {"sessionStatus", telemetry.sessionStatus},
                {"hexData", telemetry.hexData},
                {"isValid", telemetry.isValid}
            };

            if (telemetry.payload.isValid) {
                const auto& p = telemetry.payload;
                entry["payload"] = {
                    // Manufacturer header
                    {"header", p.header},
                    {"headerValid", p.headerValid},
                    {"serialNumber", p.serialNumber},

                    // Flight status
                    {"flightStatus", {
                        {"raw", p.flightStatus.raw},
                        {"phase", p.flightStatus.getPhaseString()},
                        {"btRxSuccess", p.flightStatus.btRxSuccess},
                        {"btRxFailure", p.flightStatus.btRxFailure},
                        {"balloonBurst", p.flightStatus.balloonBurst},
                        {"descent", p.flightStatus.descent},
                        {"ascent", p.flightStatus.ascent},
                        {"landing", p.flightStatus.landing}
                    }},

                    // GNSS
                    {"gnssValid", p.gnssValid},
                    {"utcHours", p.utcHours},
                    {"utcMinutes", p.utcMinutes},
                    {"utcSeconds", p.utcSeconds},
                    {"latitude", p.latitude},
                    {"longitude", p.longitude},
                    {"altitude", p.altitude},
                    {"altitudeUnits", p.altitudeUnits},

                    // AHT20
                    {"aht20", {
                        {"humidityRH", p.aht20.humidityRH},
                        {"tempC", p.aht20.tempC},
                        {"tempF", p.aht20.tempF},
                        {"status", p.aht20.status},
                        {"statusString", p.aht20.statusString()},
                        {"isValid", p.aht20.isValid}
                    }},

                    // Radio
                    {"radio", {
                        {"stationIndex", p.radio.stationIndex},
                        {"frequencyMHz", p.radio.frequencyMHz},
                        {"signalStrength", p.radio.signalStrength},
                        {"stereo", p.radio.stereo}
                    }},

                    // Analog sensors
                    {"sensors", {
                        {"masterBattery", {
                            {"name", p.masterBattery.name},
                            {"voltage", p.masterBattery.voltage},
                            {"measurement", p.masterBattery.measurement},
                            {"unit", p.masterBattery.unit},
                            {"isValid", p.masterBattery.isValid}
                        }},
                        {"slaveBattery", {
                            {"name", p.slaveBattery.name},
                            {"voltage", p.slaveBattery.voltage},
                            {"measurement", p.slaveBattery.measurement},
                            {"unit", p.slaveBattery.unit},
                            {"isValid", p.slaveBattery.isValid}
                        }},
                        {"internalTemp", {
                            {"name", p.internalTemp.name},
                            {"voltage", p.internalTemp.voltage},
                            {"measurement", p.internalTemp.measurement},
                            {"unit", p.internalTemp.unit},
                            {"isValid", p.internalTemp.isValid}
                        }},
                        {"externalTemp", {
                            {"name", p.externalTemp.name},
                            {"voltage", p.externalTemp.voltage},
                            {"measurement", p.externalTemp.measurement},
                            {"unit", p.externalTemp.unit},
                            {"isValid", p.externalTemp.isValid}
                        }}
                    }},

                    // Modem
                    {"modem", {
                        {"currentCode", p.modem.currentCode},
                        {"currentDesc", p.modem.currentDesc},
                        {"previousCode", p.modem.previousCode},
                        {"previousDesc", p.modem.previousDesc},
                        {"txSuccessCount", p.modem.txSuccessCount}
                    }},

                    {"recordSequence", p.recordSequence}
                };
            }
            telemetryArray.push_back(entry);
        }

        json output = {
            {"lastUpdated", std::time(nullptr)},
            {"totalRecords", telemetryHistory.size()},
            {"telemetry", telemetryArray}
        };

        std::ofstream file("dashboard/data/telemetry.json");
        if (file.is_open()) {
            file << output.dump(2);
            file.close();
            logger.log(LOG_INFO, "Exported " + std::to_string(telemetryHistory.size()) + " records to JSON");
        } else {
            logger.log(LOG_ERROR, "Failed to open telemetry.json for writing");
        }
    } catch (const std::exception& e) {
        logger.log(LOG_ERROR, "Error exporting to JSON: " + std::string(e.what()));
    }
}

int main() {
    std::shared_ptr auth = std::make_shared<GmailAuth>(PATH_TO_SECRET, PATH_TO_TOKEN);
    if (!auth->authenticate()) {
        logger.log(LOG_ERROR, "Authentication failed!");
        return 1;
    }

    GmailClient client(auth);
    Decoder decoder;
    std::set<std::string> processedIds;
    std::vector<EmailContent> telemetryHistory;
    time_t lastCheckTime = time(nullptr);

    logger.log(LOG_INFO, "Monitoring started, polling every " +
        std::to_string(POLL_INTERVAL_MINUTES) + " minutes...");

    while (true) {
        try {
            std::vector<GmailMessage> message = client.getMessageAfter(lastCheckTime, MODEM_EMAIL);
            logger.log(LOG_INFO, "Found " + std::to_string(message.size()) + " new message(s)");

            bool newDataAdded = false;
            for (size_t i = 0; i < message.size(); ++i) {
                const GmailMessage msg = message[i];
                logger.log(LOG_INFO, "Processing message from: " + msg.from);

                if (processedIds.count(msg.id) > 0) continue;

                EmailContent telemetry = decoder.parseEmail(msg.bodyText);
                if (telemetry.isValid && telemetry.payload.isValid) {
                    telemetryHistory.push_back(telemetry);
                    newDataAdded = true;
                    logger.log(LOG_INFO, "\n" + telemetry.toString() + "\n");
                } else {
                    logger.log(LOG_WARNING, "Failed to parse valid telemetry from message");
                    logger.log(LOG_INFO, "Body preview: " + msg.bodyText.substr(0, 200));
                }
                processedIds.insert(msg.id);
            }

            if (newDataAdded || telemetryHistory.size() > 0) {
                exportTelemetryToJson(telemetryHistory);
            }
            lastCheckTime = time(nullptr);
        } catch (const std::exception& e) {
            logger.log(LOG_ERROR, "Error: " + std::string(e.what()));
        }

        logger.log(LOG_INFO, "Waiting " + std::to_string(POLL_INTERVAL_MINUTES) + " minutes...");
        std::this_thread::sleep_for(std::chrono::minutes(POLL_INTERVAL_MINUTES));
    }
    return 0;
}