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

  /**
   * Calculate the size of a directory.
   * Symbolic links will not be followed.
   *
   * @param path Path of the directory.
   * @param interrupted Flag whether the operation is interrupted?
   * @param ignoreErrors Whether or not to ignore any errors? (default true)
   * @return Size of the directory.
   */
  static size_t calculateDirSize(std::string const& path, volatile bool *interrupted, bool ignoreErrors = true);
};


#endif //ML_GRIDENGINE_EXECUTOR_FORMATUTILS_H
