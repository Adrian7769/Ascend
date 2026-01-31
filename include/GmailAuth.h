#ifndef GMAIL_AUTH_H
#define GMAIL_AUTH_H
#include <string>

class GmailAuth {
    public:
        GmailAuth(const std::string clientSecretFile); //Constructor
        bool authenticate();
        const std::string getAccessToken();
        const bool isAuthenticated();
        bool refreshToken();
        const bool isTokenExpired();
    private:
        std::string clientId;
        std::string clientSecret;
        std::string redirectUrl;
        std::string accessToken;
        std::string refreshToken_;
        time_t tokenExpiry;
        std::string clientSecretFile;
        std::string tokenCacheFile;
        bool loadClientSecret();
        std::string getAuthorizationCode();
        bool exchangeCodeForTokens(const std::string& code);
        std::string makeTokenRequest(const std::string& postData);
        bool saveTokens();
        bool loadTokens();
        std::string urlEncode(const std::string value);
};
#endif