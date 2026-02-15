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

// Function to export telemetry history to JSON
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
            
            // Add payload data if valid
            if (telemetry.payload.isValid) {
                entry["payload"] = {
                    {"header", telemetry.payload.header},
                    {"headerValid", telemetry.payload.headerValid},
                    {"serialNumber", telemetry.payload.serialNumber},
                    {"utcHours", telemetry.payload.utcHours},
                    {"utcMinutes", telemetry.payload.utcMinutes},
                    {"utcSeconds", telemetry.payload.utcSeconds},
                    {"latitude", telemetry.payload.latitude},
                    {"longitude", telemetry.payload.longitude},
                    {"altitude", telemetry.payload.altitude},
                    {"altitudeUnits", telemetry.payload.altitudeUnits},
                    {"sensors", {
                        {"internalTemp", {
                            {"name", telemetry.payload.internalTemp.name},
                            {"voltage", telemetry.payload.internalTemp.voltage},
                            {"measurement", telemetry.payload.internalTemp.measurement},
                            {"unit", telemetry.payload.internalTemp.unit},
                            {"isValid", telemetry.payload.internalTemp.isValid}
                        }},
                        {"externalTemp", {
                            {"name", telemetry.payload.externalTemp.name},
                            {"voltage", telemetry.payload.externalTemp.voltage},
                            {"measurement", telemetry.payload.externalTemp.measurement},
                            {"unit", telemetry.payload.externalTemp.unit},
                            {"isValid", telemetry.payload.externalTemp.isValid}
                        }},
                        {"pressure", {
                            {"name", telemetry.payload.pressure.name},
                            {"voltage", telemetry.payload.pressure.voltage},
                            {"measurement", telemetry.payload.pressure.measurement},
                            {"unit", telemetry.payload.pressure.unit},
                            {"isValid", telemetry.payload.pressure.isValid}
                        }},
                        {"accelX", {
                            {"name", telemetry.payload.accelX.name},
                            {"voltage", telemetry.payload.accelX.voltage},
                            {"measurement", telemetry.payload.accelX.measurement},
                            {"unit", telemetry.payload.accelX.unit},
                            {"isValid", telemetry.payload.accelX.isValid}
                        }},
                        {"accelY", {
                            {"name", telemetry.payload.accelY.name},
                            {"voltage", telemetry.payload.accelY.voltage},
                            {"measurement", telemetry.payload.accelY.measurement},
                            {"unit", telemetry.payload.accelY.unit},
                            {"isValid", telemetry.payload.accelY.isValid}
                        }},
                        {"battery", {
                            {"name", telemetry.payload.battery.name},
                            {"voltage", telemetry.payload.battery.voltage},
                            {"measurement", telemetry.payload.battery.measurement},
                            {"unit", telemetry.payload.battery.unit},
                            {"isValid", telemetry.payload.battery.isValid}
                        }}
                    }},
                    {"modemStatus", telemetry.payload.modemStatus},
                    {"modemStatusDescription", telemetry.payload.modemStatusDescription}
                };
            }
            
            telemetryArray.push_back(entry);
        }
        
        // Create wrapper object with metadata
        json output = {
            {"lastUpdated", std::time(nullptr)},
            {"totalRecords", telemetryHistory.size()},
            {"telemetry", telemetryArray}
        };
        
        // Write to file (in a web-accessible directory)
        std::ofstream file("dashboard/data/telemetry.json");
        if (file.is_open()) {
            file << output.dump(2);  // Pretty print with 2-space indent
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
    // Gmail Auth Instance
    std::shared_ptr auth = std::make_shared<GmailAuth>(PATH_TO_SECRET,PATH_TO_TOKEN);
    if (!auth->authenticate()) {
        logger.log(LOG_ERROR, "Authentication failed!");
        return 1;
    }
    
    // The Client Instance
    GmailClient client(auth);
    
    // Create Decoder Instance
    Decoder decoder;
    std::set<std::string> processedIds;
    std::vector<EmailContent> telemetryHistory;
    time_t lastCheckTime = time(nullptr);
    
    logger.log(LOG_INFO, "Monitoring started, polling every: " + std::to_string(POLL_INTERVAL_MINUTES) + " minutes...");
    
    while (true) {
        try {
            // get the latest messages
            std::vector<GmailMessage> message = client.getMessageAfter(lastCheckTime, MODEM_EMAIL);
            logger.log(LOG_INFO,"Found " + std::to_string(message.size()) + " new message(s)");
            
            bool newDataAdded = false;
            
            for (size_t i = 0; i < message.size(); ++i) {
                const GmailMessage msg = message[i];
                logger.log(LOG_INFO, "Processing message from: " + msg.from);
                
                // Skip if processed
                if (processedIds.count(msg.id) > 0) {
                    continue;
                }
                
                // Parse the balloon telemetry Data
                EmailContent telemetry = decoder.parseEmail(msg.bodyText);
                if (telemetry.isValid && telemetry.payload.isValid) {
                    telemetryHistory.push_back(telemetry);
                    newDataAdded = true;
                    
                    // Display complete telemetry
                    logger.log(LOG_INFO, "\n" + telemetry.toString() + "\n");
                } else {
                    logger.log(LOG_WARNING, "Failed to parse valid telemetry from message");
                    logger.log(LOG_INFO, "Body preview: " + msg.bodyText.substr(0, 200));
                }                    
                
                // Updated the processedIds Vector
                processedIds.insert(msg.id);
            }
            
            // Export to JSON if new data was added
            if (newDataAdded || telemetryHistory.size() > 0) {
                exportTelemetryToJson(telemetryHistory);
            }
            
            // Update last check time
            lastCheckTime = time(nullptr);
            
        } catch (const std::exception& e) {
            logger.log(LOG_ERROR, "Error: " + std::string(e.what()));
        }
        
        // Wait before next poll
        logger.log(LOG_INFO, "Waiting " + std::to_string(POLL_INTERVAL_MINUTES) + " minutes...");
        std::this_thread::sleep_for(std::chrono::minutes(POLL_INTERVAL_MINUTES));
    }
    
    return 0;
}
