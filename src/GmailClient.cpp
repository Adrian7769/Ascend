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
// Make GET request to Gmail API
std::string GmailClient::makeGetRequest(const std::string& endpoint) {
    // But first check the status of token
    if (auth_->isTokenExpired()) {
        logger.log(LOG_INFO, "GmailClient Token expired, refreshing...");
        if (!auth_->refreshToken()) {
            logger.log(LOG_ERROR, "Failed to refresh token!");
            return "";
        }
    }
    CURL * curl = curl_easy_init();
    if (!curl) {
        logger.log(LOG_ERROR, "Failed to initialize CURL");
        return "";
    }
    std::string response;
    std::string url = baseUrl_ + endpoint;
    std::string authHeader = "Authorization: Bearer " + auth_->getAccessToken();
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        logger.log(LOG_ERROR, "[ERROR] CURL request failed: " + std::string(curl_easy_strerror(res)));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";
    }
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (httpCode != 200) {
        logger.log(LOG_ERROR, "HTTP error " + std::to_string(httpCode));
        logger.log(LOG_ERROR,"   Response: " + response);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}
std::vector<std::string> GmailClient::searchMessages(const std::string& query, int maxResults) {
    logger.log(LOG_INFO,"GmailClient Searching for: \"" + query + "\"");
    CURL* curl = curl_easy_init();
    char* encodedQuery = curl_easy_escape(curl, query.c_str(), query.length());
    std::stringstream endpoint;
    endpoint << "/messages?q=" << encodedQuery << "&maxResults=" << maxResults;
    curl_free(encodedQuery);
    curl_easy_cleanup(curl);
    std::string response = makeGetRequest(endpoint.str());
    if (response.empty()) {
        logger.log(LOG_ERROR, "Empty response from search");
        return {};
    }
    return parseMessageList(response);
}
std::vector<std::string> GmailClient::parseMessageList(const std::string& jsonResponse) {
    std::vector<std::string> messageIds;
    try {
        json j = json::parse(jsonResponse);
        if (j.contains("messages")) {
            for (const auto& msg : j["messages"]) {
                if (msg.contains("id")) {
                    messageIds.push_back(msg["id"].get<std::string>());
                }
            }
            logger.log(LOG_INFO, "GmailClient Found " + std::to_string(messageIds.size()) + " messages");
        } else {
            logger.log(LOG_INFO, "GmailClient No messages found");
        }
    } catch (const json::exception& e) {
        logger.log(LOG_ERROR, "JSON parse error: " + std::string(e.what()));
    }
    return messageIds;
}
GmailMessage GmailClient::getMessage(const std::string& messageId) {
    logger.log(LOG_INFO, "GmailClient Fetching message: " + messageId.substr(0, 10) + "...");
    std::string endpoint = "/messages/" + messageId + "?format=full";
    std::string response = makeGetRequest(endpoint);
    if (response.empty()) {
        logger.log(LOG_ERROR, "Failed to fetch message");
        return GmailMessage();
    }
    return parseMessage(response);
}
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
GmailMessage GmailClient::parseMessage(const std::string& jsonResponse) {
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
std::vector<GmailMessage> GmailClient::getMessages(const std::vector<std::string>& messageIds) {
    std::vector<GmailMessage> messages;
    for (const auto& id : messageIds) {
        messages.push_back(getMessage(id));
    }
    return messages;
}
std::vector<GmailMessage> GmailClient::searchAndGetMessages(const std::string& query, int maxResults) {
    auto ids = searchMessages(query, maxResults);
    return getMessages(ids);
}
std::vector<GmailMessage> GmailClient::getMessagesFrom(const std::string& senderEmail, int maxResults) {
    std::string query = "from:" + senderEmail;
    return searchAndGetMessages(query, maxResults);
}
std::vector<GmailMessage> GmailClient::getMessagesBySubject(const std::string& subject, int maxResults) {
    std::string query = "subject:" + subject;
    return searchAndGetMessages(query, maxResults);
}
std::vector<GmailMessage> GmailClient::getMessagesAfter(time_t afterTime, const std::string& senderEmail) {
    // Gmail uses "after:YYYY/MM/DD" format
    char dateStr[32];
    struct tm* tm_info = localtime(&afterTime);
    strftime(dateStr, sizeof(dateStr), "%Y/%m/%d", tm_info);
    std::stringstream query;
    query << "after:" << dateStr;
    if (!senderEmail.empty()) {
        query << " from:" << senderEmail;
    }
    return searchAndGetMessages(query.str(), 50);
}
std::vector<GmailMessage> GmailClient::getLatestMessagesFrom(
    const std::string& senderEmail, 
    int count
) {
    return getMessagesFrom(senderEmail, count);
}