//
// Created by 许昊文 on 2018/11/17.
//

#include <unistd.h>
#include <Poco/Format.h>
#include "Utils.h"

namespace {
  const double GB1 = 1000. * 1000 * 1000, GB2 = 1024. * 1024 * 1024;
  const double MB1 = 1000. * 1000, MB2 = 1024. * 1024;
  const double KB1 = 1000., KB2 = 1024.;
}

std::string Utils::formatSize(size_t size) {
  if (size >= GB1) {
    return Poco::format("%.2fG", size / GB2);
  } else if (size >= MB1) {
    return Poco::format("%.2fM", size / MB2);
  } else if (size >= KB1) {
    return Poco::format("%.2fK", size / KB2);
  } else {
    return Poco::format("%zB", size);
  }
}

std::string Utils::getHostname() {
  char buf[256];
  gethostname(buf, sizeof(buf));
  return std::string(buf);
}
