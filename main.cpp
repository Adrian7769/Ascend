#include "GmailAuth.h"
#include <iostream>
#include "logger.h"
Logger logger("logs/debug.log", LOG_DEBUG);
int main() {
    cout << "Program Running..." << endl;
    GmailAuth auth("env/client_secret.json");
    if (auth.authenticate()) {
       logger.log(LOG_INFO,"Authentication Successful");
       logger.log(LOG_INFO, "Access Token: " + auth.getAccessToken().substr(0, 30));
       logger.log(LOG_INFO, "Is Authenticated: " + string((auth.isAuthenticated() ? "Yes" : "No")));
    } else {
       logger.log(LOG_ERROR, "Authentication failed!");
       return 1;
    }
    cout << "Program Ran Successfully" << endl;
    return 0;
}