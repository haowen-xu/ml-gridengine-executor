//
// Created by 许昊文 on 2018/11/12.
//

#ifndef ML_GRIDENGINE_EXECUTOR_MACROS_H
#define ML_GRIDENGINE_EXECUTOR_MACROS_H

#define DEFINE_NON_PRIMITIVE_PROPERTY(TYPE, NAME) \
  private: \
    TYPE _##NAME; \
  public: \
    inline TYPE& NAME() { return _##NAME; } \
    inline TYPE const& NAME() const { return _##NAME; }

#endif //ML_GRIDENGINE_EXECUTOR_MACROS_H
