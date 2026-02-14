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

Logger logger("logs/debug.log", LOG_DEBUG);
int main() {
    // Gmail Auth Instance
    std::shared_ptr auth = std::make_shared<GmailAuth>(PATH_TO_SECRET,PATH_TO_TOKEN); //this allows inheritance of class instances, so the GmailAuth instance stays alive while the client class is referencing it.
    if (!auth->authenticate()) { // Have to use -> here because the auth variable is a pointer to GmailAuth. So to get the authenticate() method in GmailAuth I need to use ->
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
            for (size_t i = 0; i < message.size(); ++i) { //messages.size() returns size_t, compare size_t to size_t to avoid compiler error.
                const GmailMessage msg = message[i];
                logger.log(LOG_INFO, "Processing message from: " + msg.from);
                // Skip if processed
                if (processedIds.count(msg.id) > 0) {
                    continue;
                }
                // Parse the ballon telemetry Data
                EmailContent telemetry = decoder.parseEmail(msg.bodyText);
                if (telemetry.isValid && telemetry.payload.isValid) {
                    telemetryHistory.push_back(telemetry);

                    // Display complete telemetry
                    logger.log(LOG_INFO, "\n" + telemetry.toString() + "\n");
               
                 
                } else {
                    logger.log(LOG_WARNING, "Failed to parse valid telemetry from message");
                    logger.log(LOG_INFO, "Body preview: " + msg.bodyText.substr(0, 200));
                }                    
                // Updated the processedIds Vector
                processedIds.insert(msg.id);
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
