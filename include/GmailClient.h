// 

#ifndef GMAIL_CLIENT_H
#define GMAIL_CLIENT_H
#include "GmailAuth.h"
#include <string>
#include <vector>
#include <memory>

// Stucture of the Gmail Message Object
struct GmailMessage {
    std::string id;
    std::string threadId;
    std::string subject;
    std::string from;
    std::string to;
    std::string date;
    std::string snippet;     
    std::string bodyText; // Body Text is where the data is located
    std::string bodyHtml;     
};
class GmailClient {
    public:
        explicit GmailClient(std::shared_ptr<GmailAuth> auth);
        ~GmailClient();
        std::vector<GmailMessage> getMessageAfter(time_t afterTime, const std::string& senderEmail= "");
    private:
        std::shared_ptr<GmailAuth> auth_;
        std::string baseUrl_;
        std::string makeGetRequest(const std::string& endpoint);
        GmailMessage getMessageById(const std::string& messageId);
        GmailMessage parseResponse(const std::string& jsonResponse);
        std::string decodeBase64Url(const std::string& encoded);
        std::string extractBodyFromParts(const std::string& parts, const std::string& mimeType);
};
#endif 