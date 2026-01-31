// Contains Google Authentication Code
#include <iostream>
#include <string>
#include <ctime>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include "GmailAuth.h"
#include "Logger.h"

using json = nlohmann::json; // This library is used to work with json files and makes life much easier!

extern Logger logger; // Have to use Extern to reference an existing Logger instance (declared in main) because I was getting errors initially for having multiple instances.
// Look at logger.cpp to see the logger file. 
static size_t WriteCallBack(void* contents, size_t size, size_t nmemb, void * userp) {
    ((std::string *)userp)->append((char*)contents, size * nmemb); // In order to use the .append() method i need to use -> because it is a string ptr you can also do (*((std::string *)userp)).append()
    return size * nmemb; 
}
GmailAuth::GmailAuth (const std::string clientSecret, const std::string tokenCache) {
    clientSecretFile = clientSecret; // Constructor method 
    tokenCacheFile = tokenCache;
    redirectUrl = "https://localhost:8000";
    tokenExpiry = 0;
    logger.log(LOG_INFO, "GmailAuth Initialized.");
}
const std::string GmailAuth::getAccessToken() {
    return accessToken;
}
const bool GmailAuth::isAuthenticated() {
    return !accessToken.empty() && !isTokenExpired(); // Make sure access token is not expired and exists!!
}
const bool GmailAuth::isTokenExpired() {
    time_t now = time(nullptr); // When you dont want to store a time anywhere you pass a nullptr and just assign a variable to it.
    return now >= (tokenExpiry - 300); // 5 minutes till expiration
}

bool GmailAuth::loadClientSecret() { //Method use to read client secret json file
    logger.log(LOG_INFO, "Loading client secret from: " + clientSecretFile);
    std::ifstream file(clientSecretFile); //ifstream used for reading files, ofstream used for writing to files.
    if (!file.is_open()) {
        logger.log(LOG_ERROR,"Could not open client secret file: " + clientSecretFile);
        return false;
    }
    try {
        json j; // create json object j, then read the file contents and copy it it j;
        file >> j;
        if (!j.contains("installed")) {  // if there is not a key called "installed" then somthing is wrong
            logger.log(LOG_ERROR, "Invalid client secret format - missing 'installed' field."); 
            return false;
        }
        json installed = j["installed"]; // json object
        clientId = installed["client_id"].get<std::string>(); //geting typed value string and store in object attribute clientID
        clientSecret = installed["client_secret"].get<std::string>(); // getting typed value string, auto decodes.
        logger.log(LOG_INFO, "Loaded Client Credentials");
        logger.log(LOG_INFO, "Client ID: " + clientId.substr(0,20) + "...");
        logger.log(LOG_INFO, "Successfully opened client secret file.");
        return true;
    } catch (const json::exception& e) { // pass by reference here because we are just reading the error not modifying it
        logger.log(LOG_ERROR, "Failed to parse client secret: " + std::string(e.what()));
        return false;
    }
}
std::string GmailAuth::urlEncode(const std::string value) {
    // When adding values to url parameters, they need to be encoded into a special format
    CURL * curl = curl_easy_init(); // using curl here to make network requests, basically a way to do HTTP requests programmatically
    // returns a pointer, we pass this pointer to all CURL functions to tell them which requests 
    if (!curl) { // if curl_easy_init() fails it returns a nullptr
        return value;
    }
    // pass in handler, value as a c string with null terminator, and length of string
    char * encoded = curl_easy_escape(curl, value.c_str(), value.length());
    if (!encoded) {
        curl_easy_cleanup(curl); //free curl handler
        logger.log(LOG_ERROR, "Tested as !encoded, value: " + value);
        return value;
    }
    std::string result(encoded); // copy the char* ptr to regular string
    curl_free(encoded); // free encoded memory
    curl_easy_cleanup(curl); // free curl handler
    logger.log(LOG_INFO, "EncodedURL: " + result);
    return result;
}
// Once we have the user authentication code, it is now time to make the token request.
std::string GmailAuth::makeTokenRequest(const std::string& postData) {
    logger.log(LOG_INFO, "Making token request to Google.");
    CURL * curl = curl_easy_init();
    if (!curl){
        logger.log(LOG_ERROR, "Failed to Initialize CURL");
        return "";
    }
    std::string response;
    const char * tokenUrl = "https://oauth2.googleapis.com/token"; // endpoint to recieve token from code.
    // This is all curl setup code
    curl_easy_setopt(curl, CURLOPT_URL, tokenUrl); 
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallBack);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response); // Store Response in std::string response
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); 

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    // execute the request
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        logger.log(LOG_ERROR, "Curl request Failed: " + std::string(curl_easy_strerror(res)));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";
    }
    // get the response and store http code
    long int httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    logger.log(LOG_INFO, "HTTP Status: " + std::to_string(httpCode));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (httpCode != 200) {
        logger.log(LOG_ERROR, "HTTP error " + std::to_string(httpCode));
        logger.log(LOG_ERROR, "Response: " + response);
        return "";
    }
    return response;
}
// Once we have the authorization code, we now need to exchange the code for tokens.
// setter method
bool GmailAuth::exchangeCodeForTokens(const std::string& code) {
    logger.log(LOG_INFO, "Exchanging authorization code for tokens.");

    std::stringstream postData; // Build the string stream to make request
    postData << "code=" << urlEncode(code);
    postData << "&client_id=" << urlEncode(clientId);
    postData << "&client_secret=" << urlEncode(clientSecret);
    postData << "&redirect_uri=" << urlEncode(redirectUrl);
    postData << "&grant_type=authorization_code";
    // string stream finished and url is Built
    std::string response = makeTokenRequest(postData.str()); // make the token request
    if (response.empty()) {
        logger.log(LOG_ERROR, "Empty response from token endpoint");
        return false;
    }
    // Now parse the response to get the access token and access token expiry
    try {
        json j = json::parse(response);
        if (j.contains("error")) {
            logger.log(LOG_ERROR, "Token Exchange Failed" + j["error"].get<std::string>());
            if (j.contains("error_description")) {
                logger.log(LOG_ERROR, "     " + j["error_description"].get<std::string>());
            }
            return false;
        }
        if (!j.contains("access_token")) {
            logger.log(LOG_ERROR, "No access token in response");
            return false;
        }
        accessToken = j["access_token"].get<std::string>(); // update accessToken method attribute
        if(j.contains("refresh_token")) {
            refreshToken_ = j["refresh_token"].get<std::string>();
            logger.log(LOG_INFO, "Received Refresh token");
        }
        int expiresIn = j.value("expires_in", 3600);
        tokenExpiry = time(nullptr) + expiresIn; // update
        logger.log(LOG_INFO, "Access token obtained");
        logger.log(LOG_INFO, "Expires in (s): " + std::to_string(expiresIn));
        return true;
    } catch (const json::exception& e) {
        logger.log(LOG_ERROR, "Failed to parse token response: " + std::string(e.what()));
        logger.log(LOG_ERROR, "Response: " + response);
        return false;
    }
}

std::string GmailAuth::getAuthorizationCode() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "GMAIL AUTHORIZATION REQUIRED" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::stringstream authUrl; // use stringstream when building a string to redirect user to the Oauth 2.0 Server

    // Building the correct url to receive code from google.
    authUrl << "https://accounts.google.com/o/oauth2/v2/auth?";
    authUrl << "client_id=" << urlEncode(clientId);
    authUrl << "&redirect_uri=" << urlEncode(redirectUrl);
    authUrl << "&response_type=code";
    authUrl << "&scope=" << urlEncode("https://www.googleapis.com/auth/gmail.readonly");// Scope
    authUrl << "&access_type=offline";  
    authUrl << "&prompt=consent";       
    // Parameters https://developers.google.com/identity/protocols/oauth2/web-server
    
    // Redirect to Google's Oauth 2.0 Server
    std::cout << "\nStep 1: Open this URL in your browser:\n" << std::endl;
    std::cout << authUrl.str() << std::endl;
    std::cout << "\nStep 2: Authorize the application" << std::endl;
    std::cout << "        (Google will show a permission screen)" << std::endl;
    std::cout << "\nStep 3: After authorizing, you'll be redirected to:" << std::endl;
    std::cout << "        http://localhost:8080/?code=XXXXXXXXX" << std::endl;
    std::cout << "        (The page won't load)" << std::endl;
    std::cout << "\nStep 4: Copy the 'code' parameter from the URL" << std::endl;
    std::cout << "        (Everything after 'code=' and before '&' or end)" << std::endl;
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Paste the authorization code here: ";
    std::string code;
    std::getline(std::cin, code);
    code.erase(0, code.find_first_not_of(" \t\n\r"));
    code.erase(code.find_last_not_of(" \t\n\r") + 1);

    if (code.empty()) {
        logger.log(LOG_ERROR,"Empty authorization code");
        return "";
    }
    logger.log(LOG_INFO, "Authentication code received");
    return code; // Return the Authorization code
}
// save tokens to ofstream file
bool GmailAuth::saveTokens() {
    logger.log(LOG_INFO, "Saving tokens to: " + tokenCacheFile);
    json j = {
        {"access_token", accessToken},
        {"refresh_token", refreshToken_},
        {"token_expiry", tokenExpiry},
    };
    std::ofstream file(tokenCacheFile);
    if (!file.is_open()) {
        logger.log(LOG_ERROR, "Could not open token cache file for writing. ");
        return false;
    }
    file << j.dump(2);
    logger.log(LOG_INFO, "Tokens Saved.");
    return true;
}
bool GmailAuth::loadTokens() {
    logger.log(LOG_INFO, "Attempting to load cached tokens");
    std::ifstream file(tokenCacheFile);
    if (!file.is_open()) {
        logger.log(LOG_WARNING, "No cached tokens found, ok on first run");
        return false;
    }
    try {
        json j;
        file >> j;
        accessToken = j["access_token"].get<std::string>();
        refreshToken_ = j["refresh_token"].get<std::string>();
        tokenExpiry = j["token_expiry"].get<time_t>();
        logger.log(LOG_INFO, "Loaded cached tokens");
        time_t now = time(nullptr);
        int secondsRemaining = tokenExpiry - now;
        if (secondsRemaining > 0){
            logger.log(LOG_INFO, "Token valid for (s) " + std::to_string(secondsRemaining));
        } else {
            logger.log(LOG_WARNING, "Token expired (s) " + std::to_string((-secondsRemaining)));
        }
        return true;
    } catch(const json::exception& e) {
        logger.log(LOG_ERROR, "Failed to parse token cache: " + std::string(e.what()));
        return false;
    }
}
// refresh token if it is expired
bool GmailAuth::refreshToken() {
    if (refreshToken_.empty()) {
        logger.log(LOG_ERROR, "No refresh token available.");
        return false;
    }
    logger.log(LOG_INFO, "Refreshing access token.");
    std::stringstream postData;
    postData << "refresh_token=" << urlEncode(refreshToken_);
    postData << "&client_id=" << urlEncode(clientId);
    postData << "&client_secret=" << urlEncode(clientSecret);
    postData << "&grant_type=refresh_token";
    std::string response = makeTokenRequest(postData.str());
    if (response.empty()) {
        return false;
    }
    try {
        json j = json::parse(response);
        if (j.contains("error")) {
            logger.log(LOG_ERROR, "Token Refresh failed:" + j["error"].get<std::string>());
            return false;            
        }
        accessToken = j["access_token"].get<std::string>();
        int expiresIn = j.value("expires_in", 3600);
        tokenExpiry = time(nullptr) + expiresIn;
        logger.log(LOG_INFO, "Token refreshed successfully.");
        return true;
    } catch (const json::exception& e) {
        logger.log(LOG_ERROR,"Failed to parse refresh response: " + std::string(e.what()));
        return false;
    }
}
bool GmailAuth::authenticate() {
    logger.log(LOG_INFO, "Starting authentication process");
    //Step 1 load client credentials
    if (!loadClientSecret()) {
        return false;
    }
    //step 2 try to load cached tokens
    if (loadTokens()) {
        // token still valid?
        if (!isTokenExpired()) {
            logger.log(LOG_INFO, "Using cached token (still valid)");
            return true;
        }
        logger.log(LOG_INFO,"Cached token expired, attempting to refresh.");
        if (refreshToken()) {
            saveTokens();
            return true;
        }
        logger.log(LOG_ERROR, "Token refresh failed, need new authorization.");
    }
    // need new authorization
    std::string code = getAuthorizationCode();
    if (code.empty()) {
        return false;
    }
    // exchange code for tokens
    if (!exchangeCodeForTokens(code)) {
        return false;
    }
    saveTokens();
    logger.log(LOG_INFO,"***Authentication Successful***");
    return true;
}
