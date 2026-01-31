#include <iostream>
#include "GmailAuth.h"
#include <string>
#include <ctime>
using namespace std;
GmailAuth::GmailAuth (const string clientSecret) {
    clientSecretFile = clientSecret;
    tokenCacheFile = "env/token_cache.json";
    redirectUrl = "https://localhost:8000";
    tokenExpiry = 0;
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
    return now >= (tokenExpiry - 300);
}
bool GmailAuth::loadClientSecret() {
    cout << "[GmailAuth] Loading client secret from: " << clientSecretFile << endl;
    
}