#include "GmailAuth.h"
#include "GmailClient.h"
#include "logger.h"
#include <iostream>
#include <set>
#include <thread>
#include <chrono>
#include <ctime>

Logger logger("logs/debug.log", LOG_DEBUG);
const string MODEM_EMAIL = "adr2211700@maricopa.edu";
const int POLL_INTERVAL_MINUTES = 5;
const string PATH_TO_SECRET = "env/client_secret.json";

int main() {
    // Authenticate
    auto auth = make_shared<GmailAuth>(PATH_TO_SECRET);
    if (!auth->authenticate()) {
        logger.log(LOG_ERROR, "Authentication failed!");
        return 1;
    }
    // the client
    GmailClient client(auth);
    // Track the messages
    set<string> processedIds;
    time_t lastCheckTime = time(nullptr);
    
    logger.log(LOG_INFO, "Monitoring started, polling every: " + to_string(POLL_INTERVAL_MINUTES) + " minutes...");
    
    while (true) {
        try {
            // get the latest messages
            auto messages = client.getMessagesAfter(lastCheckTime, MODEM_EMAIL);
            logger.log(LOG_INFO,"Found " + to_string(messages.size()) + " new messages");
            for (const auto& msg : messages) {
                if (processedIds.count(msg.id) > 0) {
                    continue;
                }
                logger.log(LOG_INFO, "  Processing message from: " + msg.from);
                
                // TODO: Extract hex and parse
                // std::string hex = BalloonDataParser::extractHex(msg.bodyText);
                // auto telemetry = BalloonDataParser::parseHex(hex);
                // saveTelemetry(telemetry);
                
                processedIds.insert(msg.id);
            }
            // Update last check time
            lastCheckTime = time(nullptr);
        } catch (const std::exception& e) {
            logger.log(LOG_ERROR, "Error: " + string(e.what()));
        }
        // Wait before next poll
        logger.log(LOG_INFO, "Waiting " + to_string(POLL_INTERVAL_MINUTES) + " minutes...");
        this_thread::sleep_for(chrono::minutes(POLL_INTERVAL_MINUTES));
    }
    return 0;
}
