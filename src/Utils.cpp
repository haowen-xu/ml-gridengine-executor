//
// Created by 许昊文 on 2018/11/17.
//

#include <unistd.h>
#include <Poco/Format.h>
#include <Poco/Path.h>
#include <Poco/File.h>
#include "Utils.h"

namespace {
  /** Minimum size to use "GB" as the size unit. */
  size_t FORMAT_SIZE_GB_UNIT_LOW_LIMIT = 1000UL * 1024 * 1024;
  /** Minimum size to use "MB" as the size unit. */
  size_t FORMAT_SIZE_MB_UNIT_LOW_LIMIT = 1000UL * 1024;
  /** Minimum size to use "KB" as the size unit. */
  size_t FORMAT_SIZE_KB_UNIT_LOW_LIMIT = 1000UL;

  /** Size of "GB" unit. */
  double FORMAT_SIZE_GB = 1UL << 30;
  /** Size of "MB" unit. */
  double FORMAT_SIZE_MB = 1UL << 20;
  /** Size of "KB" unit. */
  double FORMAT_SIZE_KB = 1UL << 10;

  std::string formatFileSize(double size, std::string const& unit) {
    std::string s = Poco::format("%.2f", size);
    size_t dotPos = s.find('.');
    if (dotPos != std::string::npos) {
      size_t pos = s.size() - 1;
      while (pos > dotPos && s.at(pos) == '0') {
        --pos;
      }
      if (pos != dotPos) {
        pos += 1;
      }
      return s.substr(0, pos) + unit;
    } else {
      return s + unit;
    }
  }
}

std::string Utils::formatSize(size_t size) {
  if (size >= FORMAT_SIZE_GB_UNIT_LOW_LIMIT) {
    return formatFileSize(size / FORMAT_SIZE_GB, "G");
  } else if (size >= FORMAT_SIZE_MB_UNIT_LOW_LIMIT) {
    return formatFileSize(size / FORMAT_SIZE_MB, "M");
  } else if (size >= FORMAT_SIZE_KB_UNIT_LOW_LIMIT) {
    return formatFileSize(size / FORMAT_SIZE_KB, "K");
  } else {
    return Poco::format("%zB", size);
  }
}

void Utils::makeParents(std::string const &filePath) {
  Poco::Path path(filePath);
  Poco::File parentDir(path.parent());
  if (!parentDir.exists()) {
    parentDir.createDirectories();
  }
}
