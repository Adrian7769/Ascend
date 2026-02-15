#include "logger.h"
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <deque>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Static members to store recent logs
static std::deque<json> recentLogs;
static const size_t MAX_LOGS = 500; 
static std::string jsonLogPath = "dashboard/data/logs.json";

Logger::Logger(const std::string filename, LogLevel level) {
    minLevel = level;
    logFile.open(filename, std::ios::app);
    if (!logFile.is_open()) {
        std::cout << "[LOGGER] Could not open log file!" << std::endl;
    }
}

Logger::~Logger() {
    if(logFile.is_open()) {
        logFile.close();
    }
}

void Logger::log(LogLevel level, const std::string message) {
    if (level < minLevel) {
        return;               
    }
    
    std::string timestamp = getTimestamp();
    std::string levelStr = getLevelString(level);
    std::string logMessage = "[" + timestamp + "] " + "[" + levelStr + "] " + message;
    
    // Write to file
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
        logFile.flush(); // Ensure immediate write
    }
    
    // Write to console
    if (level >= LOG_DEBUG) {
        std::cout << logMessage << std::endl;
    }
    
    // Store in memory for JSON export
    json logEntry = {
        {"timestamp", timestamp},
        {"level", levelStr},
        {"message", message},
        {"epochTime", time(nullptr)}
    };
    
    recentLogs.push_back(logEntry);
    
    // Keep only recent logs
    if (recentLogs.size() > MAX_LOGS) {
        recentLogs.pop_front();
    }
    
    // Export to JSON periodically (every 10 logs to reduce I/O)
    static int logCount = 0;
    logCount++;
    if (logCount >= 10) {
        exportLogsToJson();
        logCount = 0;
    }
}

void Logger::exportLogsToJson() {
    try {
        json output = {
            {"lastUpdated", time(nullptr)},
            {"totalLogs", recentLogs.size()},
            {"logs", recentLogs}
        };
        
        std::ofstream file(jsonLogPath);
        if (file.is_open()) {
            file << output.dump(2);
            file.close();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error exporting logs to JSON: " << e.what() << std::endl;
    }
}

std::string Logger::getTimestamp() {
    time_t now = time(nullptr);
    char buffer[80];
    struct tm * timeinfo = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
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