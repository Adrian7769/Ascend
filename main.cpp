#include "GmailAuth.h"
#include "GmailClient.h"
#include "logger.h"
#include <iostream>
#include <set>
#include <thread>
#include <chrono>
#include <ctime>

Logger logger("logs/debug.log", LOG_DEBUG);

// This is the program globals that you will modify
const std::string MODEM_EMAIL = "adr2211700@maricopa.edu";
const int POLL_INTERVAL_MINUTES = 3; // You can poll every minute but, we are receiving modem data every 5 minutes so its kinda useless. 3 minutes is a happy middle ground
const std::string PATH_TO_SECRET = "env/client_secret.json";
const std::string PATH_TO_TOKEN = "env/token_cache.json"; //If these two file locations aren't correct, will log an error. Make sure they are correct.
// Specify your data format here *You should know what ports are for what sensors*, this is also highly dependent on your fdr.ino code and how you fill your 45 byte data array.

int main() {
    // Gmail Auth Instance
    auto auth = std::make_shared<GmailAuth>(PATH_TO_SECRET,PATH_TO_TOKEN); //this allows inheritance of class instances, so the GmailAuth instance stays alive while the client class is referencing it.
    if (!auth->authenticate()) { // Have to use -> here because the auth variable is a pointer to GmailAuth. So to get the authenticate() method in GmailAuth I need to use ->
        logger.log(LOG_ERROR, "Authentication failed!");
        return 1;
    }
    // The Client Instance
    GmailClient client(auth);
    // Track the messages
    std::set<std::string> processedIds; // The set is used to filter out any duplicate emails. However, I kinda already handle this with my getMessagesAfter method.
    time_t lastCheckTime = time(nullptr);
    logger.log(LOG_INFO, "Monitoring started, polling every: " + std::to_string(POLL_INTERVAL_MINUTES) + " minutes...");
    while (true) {
        try {
            // get the latest messages
            auto messages = client.getMessagesAfter(lastCheckTime, MODEM_EMAIL);
            logger.log(LOG_INFO,"Found " + std::to_string(messages.size()) + " new messages");
            for (const auto& msg : messages) {
                if (processedIds.count(msg.id) > 0) {
                    continue;
                }
                logger.log(LOG_INFO, "Processing message from: " + msg.from);
                
                // TODO: Extract hex and parse
                // std::string hex = BalloonDataParser::extractHex(msg.bodyText);
                // auto telemetry = BalloonDataParser::parseHex(hex);
                // saveTelemetry(telemetry);
                
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
