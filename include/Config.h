#ifndef CONFIG_H
#define CONFIG_H

#include <string>


const std::string MODEM_EMAIL = "300534065390120@rockblock.rock7.com";

const int POLL_INTERVAL_MINUTES = 1; // Modem sends every 5 min; 1 min poll catches it quickly

const std::string PATH_TO_SECRET = "env/client_secret.json";
const std::string PATH_TO_TOKEN  = "env/token_cache.json";

// All payload structures, byte indices, and sensor calibration
// constants are now defined in Decoder.h

#endif