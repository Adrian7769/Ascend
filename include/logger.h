#ifndef LOGGER_H
#define LOGGER_H
#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
enum LogLevel {LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR};
class Logger {
    public:
        Logger(const std::string filename, LogLevel level); 
        // Logger instance needs a filepath and a default log level. so anything over the default level will be showed.
        ~Logger();
        void log(LogLevel level, const std::string message); // take the message and level and dump to log file.
    private:
        std::ofstream logFile; // Output file stream
        LogLevel minLevel; // Min level of log instance
        std::string getLevelString(LogLevel level); // translate enum into string
        std::string getTimestamp(); // Get current time of log.
        // Id like to also add the location of the log
};
#endif