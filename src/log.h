#ifndef LOG_H
#define LOG_H

#include "singleton.hpp"

#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <chrono>

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#if WIN32
#define localtime_r(a, b) localtime_s((b), (a))
#endif

#ifndef __FILE_NAME__
	// https://stackoverflow.com/a/54335644
	template <typename T, size_t S>
	inline constexpr size_t get_file_name_offset(const T (& str)[S], size_t i = S - 1) {
		return (str[i] == '/' || str[i] == '\\') ? i + 1 : (i > 0 ? get_file_name_offset(str, i - 1) : 0);
	}

	template <typename T>
	inline constexpr size_t get_file_name_offset(T (& str)[1]) { return 0; }

    namespace utility {
		template <typename T, T v>
		struct const_expr_value {
			static constexpr const T value = v;
		};
    }

#define UTILITY_CONST_EXPR_VALUE(exp) ::utility::const_expr_value<decltype(exp), exp>::value
#define __FILE_NAME__ (&__FILE__[UTILITY_CONST_EXPR_VALUE(get_file_name_offset(__FILE__))])

#endif

const char* const g_log_filename = "log.h";
enum LogLevel{ TRACE = 0, DEBUG, INFO, WARN, ERROR, FATAL };
const int g_log_level = TRACE;
const char* const g_loglevel_str[] = {
    "TRACE ",
    "DEBUG ",
    "INFO  ",
    "WARN  ",
    "ERR   ",
    "FATAL "
};
template<typename StreamType>
StreamType& formatLogPrefix(StreamType& os, const char* levelStr, const char* file, const unsigned lineno) {
	struct tm info;
	char timeStr[80];

    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms_time = duration_cast<milliseconds>(now.time_since_epoch()).count();
    time_t in_time_t = ms_time / 1000;
    auto ms_time_left = ms_time % 1000;

	localtime_r(&in_time_t, &info);
	strftime(timeStr, 80, "%Y-%m-%d %H:%M:%S", &info);

	os << timeStr << '.' << std::setfill('0') << std::setw(3) << ms_time_left << " " 
        << levelStr << nd::Worker::getCurrWorkerName() << "(" << file << ":" << lineno << ") ";
    return os;
}

class FileLogger{
public:
    FileLogger(){
        outM.open(g_log_filename);
    }

    std::ofstream& stream(const char* levelStr, const char* file, const unsigned lineno){
        return formatLogPrefix(outM, levelStr, file, lineno);
    }

    std::mutex& mutex(){return mutexM;}

    virtual ~FileLogger(){
        outM.flush();
    }

private:
    std::ofstream outM;
    std::mutex mutexM;
};

#define g_file_logger (nd::Singleton<FileLogger, 0>::instance())
#define FILE_LOG(level, toErr, msg) {\
    if (level >= g_log_level) {\
        const char* filename = __FILE_NAME__; \
        std::lock_guard<std::mutex> lock(g_file_logger->mutex()); \
        g_file_logger->stream(g_loglevel_str[level], filename, __LINE__) << msg << std::endl; \
        if(toErr){std::cerr << msg << std::endl;} \
    }\
}

#define STD_LOG(level, toErr, msg) {\
    if (level >= g_log_level) {\
        const char* filename = __FILE_NAME__; \
        std::lock_guard<std::mutex> lock(g_file_logger->mutex()); \
        formatLogPrefix(std::cout, g_loglevel_str[level], filename, __LINE__) << msg << std::endl; \
    }\
}

//log relate
#define LOG_TRACE(msg) STD_LOG(0, false, msg)
#define LOG_DEBUG(msg) STD_LOG(1, false, msg)
#define LOG_INFO (msg) STD_LOG(2, false, msg)
#define LOG_WARN (msg) STD_LOG(3, false, msg)
#define LOG_ERROR(msg) STD_LOG(4, true,  msg)
#define LOG_FATAL(msg) STD_LOG(5, true,  msg)

#endif
