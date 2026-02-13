#ifndef DECODER_H
#define DECODER_H

#include <vector>
#include <string>
#include <cstdint>

struct PayloadData {

}
class Decode {
    public:
    // Initialize the Decoder with The Body Html and also the 
    // Iridium 45 Byte Array Template Filled Out From main
    Decode(std::string bodyHtml, struct Iridium);
    private:
    std::vector<GmailMessage> convertHexToDecimal();
    std::vector<GmailMessage> 
};
#endif