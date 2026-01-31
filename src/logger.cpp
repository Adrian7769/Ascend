#include "logger.h"
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
using namespace std;
Logger::Logger(const string filename, LogLevel level = INFO) {
    minLevel = level;
    logFile.open(filename, ios::app);
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
    string logMessage = "[]" + getTimestamp() + "] " + "[" + getLevelString(level) + "] " + message;
    if (logFile.is_open()) {
        logFile << logMessage << endl;
    } 
    if (level >= WARNING) {
        cout << logMessage << endl;
    }
}