#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
};

class Logger {
private:
    std::ofstream logFile;
    LogLevel minLevel;
    std::string getTimestamp();
    std::string getLevelString(LogLevel level);

public:
    Logger(const std::string filename, LogLevel level);
    ~Logger();
    void log(LogLevel level, const std::string message);
    void exportLogsToJson(); // Export recent logs to JSON for dashboard
};

#endif