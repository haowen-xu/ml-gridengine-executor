//
// Created by 许昊文 on 2018/11/23.
//

#ifndef ML_GRIDENGINE_EXECUTOR_SIGNALHANDLER_H
#define ML_GRIDENGINE_EXECUTOR_SIGNALHANDLER_H

#include <functional>

/** Callback method of a signal handler. */
typedef std::function<void()> SignalHandlerCallback;

/**
 * A scoped signal handler.
 *
 * This class will install a scoped signal handler at construction,
 * and uninstall the handler at deconstruction.
 */
class SignalHandler {
private:
  SignalHandlerCallback _callback;

public:
  explicit SignalHandler(SignalHandlerCallback callback);

  ~SignalHandler();

  void wait();

  void notify(int signal_value);

  static bool interrupted();
};


#endif //ML_GRIDENGINE_EXECUTOR_SIGNALHANDLER_H
