#include "logger.h"
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>

Logger::Logger(const std::string filename, LogLevel level = LOG_INFO) {
    minLevel = level; // Set the minimum level of the instance
    logFile.open(filename, std::ios::app); //std::ios::app tells the logger to append not overwrite
    if (!logFile.is_open()) {
        std::cout << "[LOGGER] Could not open log file!" << std::endl;
    }
}
Logger::~Logger() {
    if(logFile.is_open()) { // Close the file when the logger instance is killed
        logFile.close();
    }
}
void Logger::log(LogLevel level, const std::string message) {
    if (level < minLevel) { // dont display log if it is less then min level
        return;               
    }
    std::string logMessage = "[" + getTimestamp() + "] " + "[" + getLevelString(level) + "] " + message;
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
    } 
    if (level >= LOG_DEBUG) { // change this to see what logs you want in the terminal
        std::cout << logMessage << std::endl;
    }
}
std::string Logger::getTimestamp() {
    time_t now = time(nullptr); // get current time
    char buffer[80]; // store date in here
    struct tm * timeinfo = localtime(&now); // this returns a struct ptr with fields for local time
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo); // format time
    return std::string(buffer); // return time
}
std::string Logger::getLevelString(LogLevel level) {
    switch(level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}