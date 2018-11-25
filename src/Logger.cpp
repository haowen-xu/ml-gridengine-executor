//
// Created by 许昊文 on 2018/11/16.
//

#include <cstdio>
#include <Poco/Mutex.h>
#include "Logger.h"

Logger* Logger::_rootLogger = new Logger();

Logger &Logger::getLogger() {
  return *_rootLogger;
}

Logger* Logger::setRootLogger(Logger *rootLogger) {
  Logger* currentLogger = _rootLogger;
  _rootLogger = rootLogger;
  return currentLogger;
}

Logger& Logger::log(std::string const &level, std::string const &message) {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);
  std::fprintf(stderr, "[%s] %s\n", level.c_str(), message.c_str());
  return *this;
}

Logger::Logger(): _mutex(new Poco::Mutex()) {

}

Logger::~Logger() {
  delete _mutex;
}

Logger::ScopedRootLogger::ScopedRootLogger(Logger *rootLogger) :
  _rootLogger(rootLogger)
{
  _originalLogger = setRootLogger(_rootLogger);
}

Logger::ScopedRootLogger::~ScopedRootLogger() {
  setRootLogger(_originalLogger);
}
