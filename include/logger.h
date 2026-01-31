#ifndef LOGGER
#define LOGGER
#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
using namespace std;
enum LogLevel {DEBUG, INFO, WARNING, ERROR};
class Logger {
    public:
        Logger(const string filename, LogLevel level);
        ~Logger();
        void log(LogLevel level, const string message);
    private:
        ofstream logFile;
        LogLevel minLevel;
        string getLevelString(LogLevel level);
        string getTimestamp();
};
#endif