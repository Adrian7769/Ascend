#include <iostream>
#include "GmailAuth.h"
#include <string>
#include <ctime>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
using namespace std;
using json = nlohmann::json;

GmailAuth::GmailAuth (const string clientSecret) {
    clientSecretFile = clientSecret;
    tokenCacheFile = "env/token_cache.json";
    redirectUrl = "https://localhost:8000";
    tokenExpiry = 600;
    cout << "token expiry: " << tokenExpiry << endl;
    cout << "[GmailAuth] Initialized" << endl;
}
GmailAuth::~GmailAuth() {
    cout << "[GmailAuth] Destroyed" << endl;
}
const string GmailAuth::getAccessToken() {
    return accessToken;
}
const bool GmailAuth::isAuthenticated() {
    return !accessToken.empty() && !isTokenExpired(); // Make sure access token is not expired and exists
}
const bool GmailAuth::isTokenExpired() {
    time_t now = time(nullptr);
    cout << "now: " << ctime(&now) << endl;
    return now >= (tokenExpiry - 300);
}
bool GmailAuth::loadClientSecret() {
    cout << "[GmailAuth] Loading client secret from: " << clientSecretFile << endl;
    ifstream file(clientSecretFile);
    if (!file.is_open()) {
        cout << "[Error] Could not open client secret file: " << clientSecretFile << endl;
        return false;
    }
    try {
        cout << "Successfully opened secret file!";
    } catch (const json::exception e) {
        cout << "cought error" << endl;
    }
}