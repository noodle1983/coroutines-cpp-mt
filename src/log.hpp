#ifndef LOG_H
#define LOG_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "singleton.hpp"
#include "worker.hpp"

#if WIN32
#define localtime_r(a, b) localtime_s((b), (a))
#endif

#ifndef __FILE_NAME__
// https://stackoverflow.com/a/54335644
template <typename T, size_t S>
inline constexpr size_t get_file_name_offset(const T (&str)[S], size_t i = S - 1) {
  return (str[i] == '/' || str[i] == '\\') ? i + 1 : (i > 0 ? get_file_name_offset(str, i - 1) : 0);
}

template <typename T>
inline constexpr size_t get_file_name_offset(T (&str)[1]) {
  return 0;
}

namespace utility {
template <typename T, T v>
struct const_expr_value {
  static constexpr const T value = v;
};
}  // namespace utility

#define UTILITY_CONST_EXPR_VALUE(exp) ::utility::const_expr_value<decltype(exp), exp>::value
#define __FILE_NAME__ (&__FILE__[UTILITY_CONST_EXPR_VALUE(get_file_name_offset(__FILE__))])

#endif

const char* const g_log_filename = "log.log";
enum class LogLevel { TRACE = 0, DEBUG, INFO, WARN, ERROR, FATAL };
const int g_log_level = (int)LogLevel::TRACE;
const char* const g_loglevel_str[] = {"TRACE ", "DEBUG ", "INFO  ", "WARN  ", "ERR   ", "FATAL "};

template <typename StreamType>
StreamType& FormatLogPrefix(StreamType& _os, const char* _level_str, const char* _file, const unsigned _lineno) {
  struct tm info;
  constexpr size_t TIME_STR_LEN = 80;
  char time_str[TIME_STR_LEN];

  using namespace std::chrono;
  auto now = system_clock::now();
  auto ms_time = duration_cast<milliseconds>(now.time_since_epoch()).count();
  constexpr int64_t MILLISECONDS_PER_SECOND = 1000;
  time_t in_time_t = ms_time / MILLISECONDS_PER_SECOND;
  auto ms_time_left = ms_time % MILLISECONDS_PER_SECOND;

  localtime_r(&in_time_t, &info);
  strftime(time_str, TIME_STR_LEN, "%Y-%m-%d %H:%M:%S", &info);

  _os << time_str << '.' << std::setfill('0') << std::setw(3) << ms_time_left << " " << _level_str << nd::Worker::GetCurrWorkerName() << "(" << _file << ":"
      << _lineno << ") ";
  return _os;
}

class FileLogger {
 public:
  FileLogger() { m_out.open(g_log_filename); }

  std::ofstream& Stream(const char* _level_str, const char* _file, const unsigned _lineno) { return FormatLogPrefix(m_out, _level_str, _file, _lineno); }

  std::mutex& Mutex() { return m_mutex; }

  virtual ~FileLogger() { m_out.flush(); }

 private:
  std::ofstream m_out;
  std::mutex m_mutex;
};

#define g_file_logger (nd::Singleton<FileLogger, 0>::Instance())
#define FILE_LOG(level, to_err, msg)                                                        \
  {                                                                                         \
    if (level >= g_log_level) {                                                             \
      const char* filename = __FILE_NAME__;                                                 \
      std::lock_guard<std::mutex> lock(g_file_logger->Mutex());                             \
      g_file_logger->stream(g_loglevel_str[level], filename, __LINE__) << msg << std::endl; \
      if (to_err) {                                                                         \
        std::cerr << msg << std::endl;                                                      \
      }                                                                                     \
    }                                                                                       \
  }

#define STD_LOG(level, to_err, msg)                                                              \
  {                                                                                              \
    if (level >= g_log_level) {                                                                  \
      const char* filename = __FILE_NAME__;                                                      \
      std::lock_guard<std::mutex> lock(g_file_logger->Mutex());                                  \
      FormatLogPrefix(std::cout, g_loglevel_str[level], filename, __LINE__) << msg << std::endl; \
    }                                                                                            \
  }

// log relate
#define LOG_TRACE(msg) STD_LOG(((int)LogLevel::TRACE), false, msg)
#define LOG_DEBUG(msg) STD_LOG(((int)LogLevel::DEBUG), false, msg)
#define LOG_INFO (msg) STD_LOG(((int)LogLevel::INFO), false, msg)
#define LOG_WARN (msg) STD_LOG(((int)LogLevel::WARN), false, msg)
#define LOG_ERROR(msg) STD_LOG(((int)LogLevel::ERROR), true, msg)
#define LOG_FATAL(msg) STD_LOG(((int)LogLevel::FATAL), true, msg)

#endif
