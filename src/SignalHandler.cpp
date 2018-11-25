//
// Created by 许昊文 on 2018/11/23.
//

#include <signal.h>
#include <vector>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>
#include "SignalHandler.h"

namespace {
  void globalSignalHandlerFunc(int signal_value);

  class GlobalSignalHandler {
  private:
    /** Whether or not the global signal handler is installed? */
    volatile bool _installed;
    /** Whether or not an interruption signal has been received? */
    volatile bool _interrupted;
    /** The mutex to install the global signal handler. */
    Poco::Mutex _mutex;
    /** The conditional variable to wait for the signal. */
    Poco::Condition _cond;
    /** The stack of the installed signal handlers. */
    std::vector<SignalHandler*> _stack;

    GlobalSignalHandler() : _installed(false), _interrupted(false) {}

    ~GlobalSignalHandler() {
      // Force all waiting threads to interrupt
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      _cond.broadcast();
    }

    void tryInstallGlobalHandler() {
      if (!_installed) {
        struct sigaction action;
        action.sa_handler = globalSignalHandlerFunc;
        action.sa_flags = 0;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, NULL);
        sigaction(SIGTERM, &action, NULL);
        _installed = true;
      }
    }

  public:
    inline bool interrupted() const { return _interrupted; }

    /**
     * Push a signal handler to the stack.
     *
     * @param handler The signal handler.
     */
    void push(SignalHandler *handler) {
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      tryInstallGlobalHandler();
      _stack.push_back(handler);
    }

    /**
     * Pop a signal handler from the stack.
     */
    void pop() {
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      _stack.pop_back();
    }

    /** Get the global signal handler instance. */
    static GlobalSignalHandler* getInstance() {
      static GlobalSignalHandler instance;
      return &instance;
    }

    /**
     * Notify this global handler using {@arg signal_value}.
     *
     * @param signal_value The signal value.
     */
    void notify(int signal_value) {
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      _interrupted = true;
      for (auto it=_stack.rbegin(); it!=_stack.rend(); ++it) {
        (*it)->notify(signal_value);
      }
      _cond.broadcast();
    }

    /**
     * Wait for the signal.
     */
    void wait() {
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      if (!_interrupted) {
        _cond.wait(_mutex);
      }
    }
  };

  void globalSignalHandlerFunc(int signal_value) {
    switch (signal_value) {
      case SIGINT:
      case SIGTERM:
        GlobalSignalHandler::getInstance()->notify(signal_value);
        break;
      default:
        break;
    }
  }
}

SignalHandler::SignalHandler(SignalHandlerCallback callback) :
  _callback(std::move(callback))
{
  GlobalSignalHandler::getInstance()->push(this);
}

SignalHandler::~SignalHandler() {
  GlobalSignalHandler::getInstance()->pop();
}

void SignalHandler::wait() {
  GlobalSignalHandler::getInstance()->wait();
}

void SignalHandler::notify(int signal_value) {
  _callback();
}

bool SignalHandler::interrupted() {
  return GlobalSignalHandler::getInstance()->interrupted();
}
