//
// Created by 许昊文 on 2018/11/12.
//

#ifndef ML_GRIDENGINE_EXECUTOR_PROGRAMCONTAINER_H
#define ML_GRIDENGINE_EXECUTOR_PROGRAMCONTAINER_H

#include <map>
#include <string>
#include <vector>
#include "macros.h"

namespace Poco {
  class Mutex;
  class Condition;
  class Thread;
}

typedef std::string Path;
typedef std::vector<Path> ArgList;
typedef std::map<std::string, std::string> EnvironMap;

typedef enum {
  NOT_STARTED = 0,    // not started
  RUNNING = 1,        // still running
  EXITED = 2,         // exited normally
  SIGNALLED = 3,      // stopped because of signal
  CANNOT_KILL = 4     // killed but not stopped
} ProgramStatus;

/**
 * Class for executing the user program.
 */
class ProgramExecutor {
  DEFINE_NON_PRIMITIVE_PROPERTY(ArgList, args);
  DEFINE_NON_PRIMITIVE_PROPERTY(EnvironMap, environ);
  DEFINE_NON_PRIMITIVE_PROPERTY(Path, workDir);

private:
  bool _captureOutput;
  std::string _loggingTag;
  Poco::Mutex *_waitMutex;          // mutex for operating on the wait condition
  Poco::Condition *_waitCond;       // the wait conditional variable
  Poco::Thread *_waitThread;        // the thread for actually perform waiting
  Poco::Mutex *_killMutex;          // mutex for synchronizing killing
  volatile ProgramStatus _status;   // status of the program
  volatile int _waitStatus;         // waitpid status of the child process.
  int _processId;                   // ID of the child (program) process
  int _pipeFd;                      // pipe fd to read/write from/to the child process

  void _waitInBackground();
  void _killIfRunning(int signal);

public:
  /** Get the program status. */
  inline ProgramStatus status() const { return _status; }

  /**
   * Get the process ID of the program.
   *
   * @return The process ID, or -1 if the program has not started.
   */
  inline int processId() const { return _processId; }

  /**
   * Get the exit code of the program.
   *
   * @return The program exit code if the program has been started and exited.
   *         Otherwise returns -1.
   */
  inline int exitCode() const {
    if (_status == EXITED) return WEXITSTATUS(_waitStatus); else return -1;
  }

  /**
   * Get the exit signal of the program.
   *
   * @return The program exit signal if the program has been started and killed by signal.
   *         Otherwise returns -1.
   */
  inline int exitSignal() const {
    if (_status == SIGNALLED) return WTERMSIG(_waitStatus); else return -1;
  }

  /** Construct a new {@class ProgramExecutor}. */
  explicit ProgramExecutor(ArgList args, EnvironMap environMap=EnvironMap(), Path workDir=Path(),
                           bool captureOutput=true, std::string const& loggingTag="Program");

  /** Start the user program. */
  void start();

  /**
   * Read program output from the pipe.
   *
   * For performance consideration, this method does not perform any checks
   * (e.g., whether or not the program has started, or the output is captured).
   *
   * @param target Target array.
   * @param count Maximum number of bytes to read.
   * @return Actual number of bytes read.
   */
  ssize_t readOutput(void* target, size_t count);

  /**
   * Wait the user program to exit.
   *
   * @param timeout The timeout in milliseconds.
   *                If <= 0, wait forever.
   * @return Whether or not the program has exited.
   * @throw Poco::IllegalStateException If the program has not started.
   */
  bool wait(long timeout=0);

  /**
   * Kill the user program.
   *
   * @param firstWait Seconds to wait after first attempt to kill by SIGINT.
   * @param secondWait Seconds to wait after second attempt to kill by SIGINT.
   * @param finalWait Seconds to wait after final attempt to kill.
   *
   * @return The pointer to the exit code if the program has exited with a code,
   *         otherwise NULL.
   * @throw Poco::IllegalStateException If the program has not started.
   */
  void kill(double firstWait, double secondWait, double finalWait);

  inline void kill() {
    kill(ML_GRIDENGINE_KILL_PROGRAM_FIRST_WAIT_SECONDS,
         ML_GRIDENGINE_KILL_PROGRAM_SECOND_WAIT_SECONDS,
         ML_GRIDENGINE_KILL_PROGRAM_FINAL_WAIT_SECONDS);
  }

  ~ProgramExecutor();
};


#endif //ML_GRIDENGINE_EXECUTOR_PROGRAMCONTAINER_H
