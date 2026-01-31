#ifndef GMAIL_AUTH_H
#define GMAIL_AUTH_H
#include <string>
/*
Google Authentication Flow (As far as i understand it). 
1.) First step is to acquire a client secret json file from google cloud console. So you have to create Oauth2 Credentials.
2.) Once you have these credentials you use them to receive a code from google. You only really have to do this once if you have a token refresh method but 
3.) for Authentication I believe the code is just to verify if you are actually you. so they require a user to enter their own email and password.
3.) Once you have the code, you send this to google and you get a token. So you need to be able to handle the response from google that contains the token. I store it in tokenCache.
4.) You also need methods to read the token file and write to the token file. The token also expires every hour, so you need a method to be able to handle refreshes. 
5.) for refreshes You can just send google the token, and they will give you another updated one. every time the client makes a get request, it checks the status of the token. So basically we never have to get another code.
 To summarize Client Secret -> User Code -> Token -> Refresh Token (This last step ideally runs forever)
*/
class GmailAuth {
    public:
        GmailAuth(const std::string clientSecretFile, const std::string tokenCache); //Every GmailAuth Method Needs a CLient Secret File Location and a Token Cache Location
        bool authenticate(); // This is the Method that handles the entire authentication flow. Its the Highest level of the class.
        const std::string getAccessToken(); // Gets access token
        const bool isAuthenticated(); // For a user to be authenticated they need to have a token and it has to not be expired // Getter Method
        bool refreshToken(); // Returns true of refresh token exists
        const bool isTokenExpired(); // Getter Method
    private:
        std::string clientId; // stores client ID
        std::string clientSecret; // stores client secret filepath
        std::string redirectUrl; // redirectUrl used to get code.
        std::string accessToken; // Stores AccessToken
        std::string refreshToken_; // stores refreshtoken
        time_t tokenExpiry; //stores the time left for token
        std::string clientSecretFile; // Store client secret file path
        std::string tokenCacheFile; // Stores token cache file path
        bool loadClientSecret();
        std::string getAuthorizationCode();
        bool exchangeCodeForTokens(const std::string& code); // This is really only used once initially, the code needs to be copy and pasted by a user. not sure how to automate this.
        std::string makeTokenRequest(const std::string& postData); 
        bool saveTokens(); // write to the token file
        bool loadTokens(); // read from the token file
        std::string urlEncode(const std::string value); // Api requests need specific formats
};
#endif