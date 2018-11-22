//
// Created by 许昊文 on 2018/11/17.
//

#ifndef ML_GRIDENGINE_EXECUTOR_FORMATUTILS_H
#define ML_GRIDENGINE_EXECUTOR_FORMATUTILS_H

#include <string>


class Utils {
public:
  static std::string formatSize(size_t size);

  /** Make parent directories. */
  static void makeParents(std::string const& filePath);
};


#endif //ML_GRIDENGINE_EXECUTOR_FORMATUTILS_H