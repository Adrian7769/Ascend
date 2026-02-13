#include "GmailAuth.h"
#include "GmailClient.h"
#include "logger.h"
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>

Logger logger("logs/debug.log", LOG_DEBUG);

// This is the program globals that you will modify
const std::string MODEM_EMAIL = "adr2211700@maricopa.edu";
const int POLL_INTERVAL_MINUTES = 1; // You can poll every minute but, we are receiving modem data every 5 minutes so its kinda useless. 3 minutes is a happy middle ground
const std::string PATH_TO_SECRET = "env/client_secret.json";
const std::string PATH_TO_TOKEN = "env/token_cache.json"; //If these two file locations aren't correct, will log an error. Make sure they are correct.
// Specify your data format here *You should know what ports are for what sensors*, this is also highly dependent on your fdr.ino code and how you fill your 45 byte send data array.

int main() {
    // Gmail Auth Instance
    std::shared_ptr auth = std::make_shared<GmailAuth>(PATH_TO_SECRET,PATH_TO_TOKEN); //this allows inheritance of class instances, so the GmailAuth instance stays alive while the client class is referencing it.
    if (!auth->authenticate()) { // Have to use -> here because the auth variable is a pointer to GmailAuth. So to get the authenticate() method in GmailAuth I need to use ->
        logger.log(LOG_ERROR, "Authentication failed!");
        return 1;
    }
    // The Client Instance
    GmailClient client(auth);
    time_t lastCheckTime = time(nullptr);
    logger.log(LOG_INFO, "Monitoring started, polling every: " + std::to_string(POLL_INTERVAL_MINUTES) + " minutes...");
    while (true) {
        try {
            // get the latest messages
            std::vector<GmailMessage> message = client.getMessageAfter(lastCheckTime, MODEM_EMAIL);
            logger.log(LOG_INFO,"Found " + std::to_string(message.size()) + " new message(s)");
            for (size_t i = 0; i < message.size(); ++i) { //messages.size() returns size_t, compare size_t to size_t to avoid compiler error.
                const GmailMessage msg = message[i];
                logger.log(LOG_INFO, msg.bodyText);
                
                logger.log(LOG_INFO, "  Processing message from: " + msg.from);
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
