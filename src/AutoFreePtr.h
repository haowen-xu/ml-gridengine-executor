//
// Created by 许昊文 on 2018/11/18.
//

#ifndef ML_GRIDENGINE_EXECUTOR_AUTOFREEPTR_H
#define ML_GRIDENGINE_EXECUTOR_AUTOFREEPTR_H

#include <memory.h>

template <typename T>
struct AutoFreePtr {
  T* ptr;

  void swap(T** other) {
    T* tmp = *other;
    *other = ptr;
    ptr = tmp;
  }

  explicit AutoFreePtr(T* ptr): ptr(ptr) {}
  ~AutoFreePtr() { free(ptr); }
};

#endif //ML_GRIDENGINE_EXECUTOR_AUTOFREEPTR_H
