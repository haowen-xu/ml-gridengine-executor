//
// Created by 许昊文 on 2018/11/16.
//

#ifndef ML_GRIDENGINE_EXECUTOR_LOGGER_H
#define ML_GRIDENGINE_EXECUTOR_LOGGER_H

#include <string>
#include <Poco/Format.h>

namespace Poco {
  class Mutex;
}

class Logger {
private:
  static Logger* _rootLogger;
  Poco::Mutex *_mutex;

public:
  class ScopedRootLogger {
  private:
    Logger* _rootLogger;
    Logger* _originalLogger;

  public:
    explicit ScopedRootLogger(Logger* rootLogger);

    ~ScopedRootLogger();

    inline Logger& rootLogger() const { return *_rootLogger; }
  };

  explicit Logger();
  virtual ~Logger();

  /** Get the root logger. */
  static Logger& getLogger();

  /**
   * Replace the default root logger with given logger.
   *
   * @param rootLogger The logger used to replace the default root logger.
   *                   If NULL, will reset to the default logger.
   * @return The current root logger.
   */
  static Logger* setRootLogger(Logger *rootLogger);

  /**
   * Log a message.
   *
   * @param level Level of the message.
   * @param message Message text.
   *
   * @return The logger instance itself.
   */
  virtual Logger& log(std::string const& level, std::string const& message);

  // ---- logging methods of INFO level ----
  inline Logger& info(std::string const& message) {
    return log("INFO", message);
  }
  template <typename Arg1>
  inline Logger& info(std::string const& format, Arg1 const& arg1) {
    return info(Poco::format(format, arg1));
  }
  template <typename Arg1, typename Arg2>
  inline Logger& info(std::string const& format, Arg1 const& arg1, Arg2 const& arg2) {
    return info(Poco::format(format, arg1, arg2));
  }
  template <typename Arg1, typename Arg2, typename Arg3>
  inline Logger& info(std::string const& format, Arg1 const& arg1, Arg2 const& arg2, Arg3 const& arg3) {
    return info(Poco::format(format, arg1, arg2, arg3));
  }

  // ---- logging methods of WARN level ----
  inline Logger& warn(std::string const& message) {
    return log("WARN", message);
  }
  template <typename Arg1>
  inline Logger& warn(std::string const& format, Arg1 const& arg1) {
    return warn(Poco::format(format, arg1));
  }
  template <typename Arg1, typename Arg2>
  inline Logger& warn(std::string const& format, Arg1 const& arg1, Arg2 const& arg2) {
    return warn(Poco::format(format, arg1, arg2));
  }

  // ---- logging methods of ERROR level ----
  inline Logger& error(std::string const& message) {
    return log("ERROR", message);
  }
  template <typename Arg1>
  inline Logger& error(std::string const& format, Arg1 const& arg1) {
    return error(Poco::format(format, arg1));
  }
  template <typename Arg1, typename Arg2>
  inline Logger& error(std::string const& format, Arg1 const& arg1, Arg2 const& arg2) {
    return error(Poco::format(format, arg1, arg2));
  }
};


#endif //ML_GRIDENGINE_EXECUTOR_LOGGER_H
