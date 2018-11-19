//
// Created by 许昊文 on 2018/11/17.
//

#include <Poco/Format.h>
#include "FormatUtils.h"

namespace {
  const double GB1 = 1000. * 1000 * 1000, GB2 = 1024. * 1024 * 1024;
  const double MB1 = 1000. * 1000, MB2 = 1024. * 1024;
  const double KB1 = 1000., KB2 = 1024.;

  template <typename Arg1>
  std::string sprintf(std::string const& fmt, Arg1 const& arg1) {
    std::string ret;
    char buf[64];
    int retSize = ::snprintf(buf, sizeof(buf), fmt.c_str(), arg1);
    if (retSize >= sizeof(buf)) {
      size_t newBufSize = (size_t)retSize + 1;
      char* newBuf = (char*)malloc(newBufSize);
      ::snprintf(newBuf, newBufSize, fmt.c_str(), arg1);
      ret.assign(newBuf);
      free(newBuf);
    } else {
      ret.assign(buf);
    }
    return std::move(ret);
  }
}

std::string FormatUtils::formatSize(size_t size) {
  if (size >= GB1) {
    return sprintf("%.2fG", size / GB2);
  } else if (size >= MB1) {
    return sprintf("%.2fM", size / MB2);
  } else if (size >= KB1) {
    return sprintf("%.2fK", size / KB2);
  } else {
    return sprintf("%zdB", size);
  }
}
