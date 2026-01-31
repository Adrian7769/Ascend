#ifndef GMAIL_AUTH
#define GMAIL_AUTH
#include <string>

class GmailAuth {
    public:
        GmailAuth(const std::string clientSecretFile); //Constructor
        ~GmailAuth(); //Deconstructor
        bool authenticate();
        const std::string getAccessToken();
        const bool isAuthenticated();
        const bool isTokenExpired();
    private:
        std::string clientId;
        std::string clientSecret;
        std::string redirectUrl;
        std::string accessToken;
        std::string refreshToken;
        time_t tokenExpiry;
        std::string clientSecretFile;
        std::string tokenCacheFile;
        bool loadClientSecret();
        std::string getAuthorizationCode();
        bool exchangeCodeForTokens(const std::string code);
        std::string makeTokenRequest(const std::string postData);
        bool saveTokens();
        bool loadTokens();
        std::string urlEncode(const std::string value);
};
#endif