#include "logger.h"
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
using namespace std;
Logger::Logger(const string filename, LogLevel level = LOG_INFO) {
    minLevel = level;
    logFile.open(filename, ios::app);
    if (!logFile.is_open()) {
        cout << "Could not open log file!" << endl;
    }
}
Logger::~Logger() {
    if(logFile.is_open()) {
        logFile.close();
    }
}
void Logger::log(LogLevel level, const string message) {
    if (level < minLevel) {
        return;
    }
    string logMessage = "[" + getTimestamp() + "] " + "[" + getLevelString(level) + "] " + message;
    if (logFile.is_open()) {
        logFile << logMessage << endl;
    } 
    if (level >= LOG_WARNING) {
        cout << logMessage << endl;
    }
}
string Logger::getTimestamp() {
    time_t now = time(nullptr); // get current time
    char buffer[80]; // store date in here
    struct tm * timeinfo = localtime(&now); // this returns a struct ptr with fields for local time
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo); // format time
    return string(buffer); // return time
}
string Logger::getLevelString(LogLevel level) {
    switch(level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}