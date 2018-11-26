//
// Created by 许昊文 on 2018/11/25.
//

#include "CapturingLogger.h"

Logger &CapturingLogger::log(std::string const &level, std::string const &message) {
  _capturedLogs.emplace_back(level, message);
  return *this;
}
