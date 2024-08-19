/**
 * Licensing Information
 *    This is a release of rzsz-nd, brought to you by Dong Lu(noodle1983@126
 *    .com). Except for extra permissions from Dong Lu(noodle1983@126.com),
 *    this software is released under version 3 of the GNU General
 *    Public License (GPLv3).
 **/
#ifndef LOG_H
#define LOG_H

#include "singleton.hpp"

#include <iostream>
#include <fstream>
#include <thread>

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

const char* g_log_filename = "log.h";
const int g_log_level = 1;

#if WIN32
#define localtime_r(a, b) localtime_s((b), (a))

#endif

class FileLogger{
public:
    FileLogger(){
        outM.open(g_log_filename);
    }

    std::ofstream& stream(const char* levelStr, const char* file, const unsigned lineno){
        time_t rawTime;
        struct tm info;
        char timeStr[80];

        time(&rawTime);
        localtime_r(&rawTime, &info);
        strftime(timeStr, 80, "%Y-%m-%d %H:%M:%S ", &info);

        outM << timeStr << levelStr;// << "(" << file << ":" << lineno << ")";
        return outM;
    }

    std::mutex& mutex(){return mutexM;}

    virtual ~FileLogger(){
        outM.flush();
    }

private:
    std::ofstream outM;
    std::mutex mutexM;
};

const char* const g_loglevel_str[] = {
    "TRACE ",
    "DEBUG ",
    "INFO  ",
    "WARN  ",
    "ERR   ",
    "FATAL "
};

#define g_file_logger (nd::Singleton<FileLogger, 0>::instance())
#define LOG(level, toErr, msg) {\
    if (level >= g_log_level) {\
        std::lock_guard<std::mutex> lock(g_file_logger->mutex()); \
        g_file_logger->stream(g_loglevel_str[level], __FILE__, __LINE__) << msg << std::endl; \
        if(toErr){std::cerr << msg << std::endl;} \
    }\
}

//log relate
#define LOG_TRACE(msg) LOG(0, false, msg)
#define LOG_DEBUG(msg) LOG(1, false, msg)
#define LOG_INFO(msg)  LOG(2, false, msg)
#define LOG_WARN(msg)  LOG(3, false, msg)
#define LOG_ERROR(msg) LOG(4, true, msg)
#define LOG_FATAL(msg) LOG(5, true, msg)

#endif
