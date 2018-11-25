//
// Created by 许昊文 on 2018/11/25.
//

#ifndef ML_GRIDENGINE_EXECUTOR_CAPTURINGLOGGER_H
#define ML_GRIDENGINE_EXECUTOR_CAPTURINGLOGGER_H

#include <string>
#include <vector>
#include "src/Logger.h"

struct CapturedLog {
  std::string level;
  std::string message;

  explicit CapturedLog(std::string const& level, std::string const& message) : level(level), message(message) {}

  bool operator==(CapturedLog const& other) const {
    return level == other.level && message == other.message;
  }
};

class CapturingLogger : public Logger {
private:
  std::vector<CapturedLog> _capturedLogs;

public:
  virtual Logger& log(std::string const& level, std::string const& message);

  std::vector<CapturedLog> const& capturedLogs() const { return _capturedLogs; }
};


#endif //ML_GRIDENGINE_EXECUTOR_CAPTURINGLOGGER_H
