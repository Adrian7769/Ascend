#include "GmailAuth.h"
#include <iostream>
using namespace std;
int main() {
    cout << "Gmail authentication test" << endl;
    GmailAuth auth("env/client_secret.json");
    string authStatus = (auth.isAuthenticated() ? "Yes": "No");
    std::cout << "Authenticated: " << authStatus << endl;
    return 0;
}