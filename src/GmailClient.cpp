// Contains Google Client Code
#include <iostream>
#include <sstream>
#include <algorithm>
#include "GmailClient.h"
#include "logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
extern Logger logger;
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
// Constructor
GmailClient::GmailClient(std::shared_ptr<GmailAuth> auth) {
    auth_ = auth;
    baseUrl_ ="https://gmail.googleapis.com/gmail/v1/users/me";
    logger.log(LOG_INFO, "GmailClient initialized");
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
// Destructor
GmailClient::~GmailClient() {
    logger.log(LOG_INFO,"GmailClient Cleaning up");
    curl_global_cleanup();
}
std::string GmailClient::makeGetRequest(const std::string& endpoint) {
    // But first check the status of token
    if (auth_->isTokenExpired()) {
        logger.log(LOG_INFO, "GmailClient Token expired, refreshing...");
        if (!auth_->refreshToken()) {
            logger.log(LOG_ERROR, "Failed to refresh token!");
            return "";
        }
    }
    // Initialize Curl object
    CURL * curl = curl_easy_init();
    if (!curl) {
        logger.log(LOG_ERROR, "Failed to initialize CURL");
        return "";
    }
    // Store Response
    std::string response;
    // When making get request append the endpoint to the base url to hit correct api endpoint
    std::string url = baseUrl_ + endpoint;
    std::string authHeader = "Authorization: Bearer " + auth_->getAccessToken();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    // Configure For get Request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    // Store Response code
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        logger.log(LOG_ERROR, "[ERROR] CURL request failed: " + std::string(curl_easy_strerror(res)));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";
    }
    // Store Http code
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (httpCode != 200) { // 200 indicates success
        logger.log(LOG_ERROR, "HTTP error " + std::to_string(httpCode));
        logger.log(LOG_ERROR,"   Response: " + response);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

// Called from top level of main loop
std::vector<GmailMessage> GmailClient::getMessageAfter(time_t afterTime, const std::string& senderEmail) {
    std::string query = "after:" + std::to_string(afterTime) + " from:" + senderEmail;
    logger.log(LOG_INFO,"GmailClient Searching for: \"" + query + "\"");

    CURL* curl = curl_easy_init();
    char* encodedQuery = curl_easy_escape(curl, query.c_str(), query.length());
    std::stringstream endpoint;
    endpoint << "/messages?q=" << encodedQuery;
    curl_free(encodedQuery);
    curl_easy_cleanup(curl);
    // Response stores message ID
    std::string response = makeGetRequest(endpoint.str());
    //logger.log(LOG_INFO,"initial Response: " + response);
    
    if (response.empty()) {
        logger.log(LOG_ERROR, "Empty response from search");
        return {};
    }
    
    nlohmann::json json = nlohmann::json::parse(response);
    std::vector<GmailMessage> messages;
    // Use ID's to make api request to get body text
    if (json.contains("messages")) {
        for (const auto& msgRef : json["messages"]) {
            std::string msgId = msgRef["id"];
            GmailMessage fullMsg = getMessageById(msgId);
            messages.push_back(fullMsg);
        }
    }
    return messages;
}

// Make API calls BY ID
GmailMessage GmailClient::getMessageById(const std::string& messageId) {
    std::string endpoint = "/messages/" + messageId + "?format=full";
    std::string response = makeGetRequest(endpoint);
    //logger.log(LOG_INFO, "Full Response By ID (" + messageId + "): "  + response);
    if (response.empty()) {
        logger.log(LOG_ERROR, "Empty response for message ID: " + messageId);
    }
    return parseResponse(response);
}
// Parse Response and map to struct GmailMessage
GmailMessage GmailClient::parseResponse(const std::string& jsonResponse) {
    GmailMessage message;
    try {
        json j = json::parse(jsonResponse);
        message.id = j["id"].get<std::string>();
        message.threadId = j["threadId"].get<std::string>();
        if (j.contains("snippet")) {
            message.snippet = j["snippet"].get<std::string>();
        }
        if (j.contains("payload") && j["payload"].contains("headers")) {
            for (const auto& header : j["payload"]["headers"]) {
                std::string name = header["name"].get<std::string>();
                std::string value = header["value"].get<std::string>();
                if (name == "Subject") {
                    message.subject = value;
                } else if (name == "From") {
                    message.from = value;
                } else if (name == "To") {
                    message.to = value;
                } else if (name == "Date") {
                    message.date = value;
                }
            }
        }
        if (j.contains("payload")) {
            std::string plainText = extractBodyFromParts(j["payload"].dump(), "text/plain");
            std::string htmlText = extractBodyFromParts(j["payload"].dump(), "text/html");
            message.bodyText = plainText;
            message.bodyHtml = htmlText;
        }
        logger.log(LOG_INFO, "GmailClient Parsed message from: " + message.from);
    } catch (const json::exception& e) {
        logger.log(LOG_ERROR, "Failed to parse message: " + std::string(e.what()));
    }
    return message;
}
// Sample Base 64 encoded email content
std::string GmailClient::extractBodyFromParts(const std::string& partsJson, const std::string& mimeType) {
    try {
        json payload = json::parse(partsJson);
        if (payload.contains("mimeType") && 
            payload["mimeType"].get<std::string>() == mimeType &&
            payload.contains("body") && 
            payload["body"].contains("data")) {
            return decodeBase64Url(payload["body"]["data"].get<std::string>());
        }
        if (payload.contains("parts")) {
            for (const auto& part : payload["parts"]) {
                if (part.contains("mimeType") && 
                    part["mimeType"].get<std::string>() == mimeType &&
                    part.contains("body") && 
                    part["body"].contains("data")) {
                    return decodeBase64Url(part["body"]["data"].get<std::string>());
                }
                if (part.contains("parts")) {
                    std::string result = extractBodyFromParts(part.dump(), mimeType);
                    if (!result.empty()) return result;
                }
            }
        }
    } catch (const json::exception& e) {
        logger.log(LOG_ERROR,"extracting body: " + std::string(e.what()));
    }
    return "";
}
// Sample Base 64 encoded email content
/* 
              "data": "LS0tLS0tLS0tLSBGb3J3YXJkZWQg
              bWVzc2FnZSAtLS0tLS0tLS0NCkZyb206IERhbmllbCBCYWRpdSA8ZGFuMjM
              1MjY0OEBtYXJpY29wYS5lZHU-DQpEYXRlOiBUaHUsIEphbiAyOSwgMjAyNiBhdCA2OjQ
              w4oCvUE0NClN1YmplY3Q6IEZ3ZDogTWVzc2FnZSA1NSBmcm9tIFJvY2tCTE9DSyAzMDA1MzQ
              wNjUzOTAxMjANClRvOiA8QURSMjIxMTcwMEBtYXJpY29wYS5lZHU-DQoNCg0KDQoNCi0tLS0
              tLS0tLS0gRm9yd2FyZGVkIG1lc3NhZ2UgLS0tLS0tLS0tDQpGcm9tOiAzMDA1MzQwNjUzO
              TAxMjAgPDMwMDUzNDA2NTM5MDEyMEByb2NrYmxvY2sucm9jazcuY29tPg0KRGF0ZTogU2F0LC
              BNYXIgMjksIDIwMjUgYXQgMTI6NTHigK9QTQ0KU3ViamVjdDogTWVzc2FnZSA1NSBmcm9tIF
              JvY2tCTE9DSyAzMDA1MzQwNjUzOTAxMjANClRvOiA8ZGFuMjM1MjY0OEBtYXJpY29wYS5lZHU
              -DQoNCg0KDQpJTUVJOiAzMDA1MzQwNjUzOTAxMjANCk1PTVNOOiA1NQ0KVHJhbnNtaXQgVGltZ
              TogMjAyNS0wMy0yOVQxOTo1MDoxMlogVVRDDQpJcmlkaXVtIExhdGl0dWRlOiAzMi43ODgwDQpJc
              mlkaXVtIExvbmdpdHVkZTogLTExMS4zNDEyDQpJcmlkaXVtIENFUDogMy4wDQpJcmlkaXVtIFNlc3Np
              b24gU3RhdHVzOiAwDQpEYXRhOg0KNTI0MjAwMzNmNTEzMzIwNzIwMmYyZTAwNmYxNjMzMDEwMDAwMDIyNzA
              wMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMA0K"
Decoded to:

From: 300534065390120 <300534065390120@rockblock.rock7.com>
Date: Sat, Mar 29, 2025 at 12:51 PM
Subject: Message 55 from RockBLOCK 300534065390120
To: <dan2352648@maricopa.edu>

IMEI: 300534065390120
MOMSN: 55
Transmit Time: 2025-03-29T19:50:12Z UTC
Iridium Latitude: 32.7880
Iridium Longitude: -111.3412
Iridium CEP: 3.0
Iridium Session Status: 0
Data: 524200333f5133207202f2e006f1633010000022700000000000000000000000000000000000000000000000000000
*/
std::string GmailClient::decodeBase64Url(const std::string& encoded) {
    std::string base64 = encoded;
    replace(base64.begin(), base64.end(), '-', '+');
    replace(base64.begin(), base64.end(), '_', '/');
    while (base64.length() % 4 != 0) {
        base64 += '=';
    }
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string decoded;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : base64) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

