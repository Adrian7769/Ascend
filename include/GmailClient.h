#ifndef GMAIL_CLIENT_H
#define GMAIL_CLIENT_H
#include "GmailAuth.h"
#include <string>
#include <vector>
#include <memory>
struct GmailMessage {
    std::string id;
    std::string threadId;
    std::string subject;
    std::string from;
    std::string to;
    std::string date;
    std::string snippet;     
    std::string bodyText;     
    std::string bodyHtml;     
};
class GmailClient {
    public:
        explicit GmailClient(std::shared_ptr<GmailAuth> auth);
        ~GmailClient();
        std::vector<std::string> searchMessages(const std::string& query, int maxResults = 10); // Gmail Auth wont be destroyed until main() and gmailClient are done with it
        GmailMessage getMessage(const std::string& messageId);
        std::vector<GmailMessage> getMessages(const std::vector<std::string>& messageIds);
        std::vector<GmailMessage> searchAndGetMessages(const std::string& query, int maxResults = 10);
        std::vector<GmailMessage> getMessagesFrom(const std::string& senderEmail, int maxResults = 10);
        std::vector<GmailMessage> getMessagesBySubject(const std::string& subject, int maxResults = 10);
        std::vector<GmailMessage> getMessagesAfter(time_t afterTime, const std::string& senderEmail= "");
        std::vector<GmailMessage> getLatestMessagesFrom(const std::string& senderEmail, int count = 1);
    private:
        std::shared_ptr<GmailAuth> auth_;
        std::string baseUrl_;
        std::string makeGetRequest(const std::string& endpoint);
        std::vector<std::string> parseMessageList(const std::string& jsonResponse);
        GmailMessage parseMessage(const std::string& jsonResponse);
        std::string decodeBase64Url(const std::string& encoded);
        std::string extractBodyFromParts(const std::string& parts, const std::string& mimeType);
};
#endif 