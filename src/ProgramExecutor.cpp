//
// Created by 许昊文 on 2018/11/12.
//

#include <cstdio>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <Poco/Condition.h>
#include <Poco/Exception.h>
#include <Poco/Mutex.h>
#include <Poco/RunnableAdapter.h>
#include <Poco/Thread.h>
#include "ProgramExecutor.h"
#include "Logger.h"

#define REQUIRE_STARTED() if (_status == NOT_STARTED) { \
    throw Poco::IllegalStateException("Program has not started."); \
  }

namespace {
  char* copyAsCString(std::string const& s) {
    char* ret = (char*)malloc(s.length() + 1);
    memcpy(ret, s.data(), s.length());
    ret[s.length()] = 0;
    return ret;
  }

  inline std::string errorMessage() {
    return std::string(strerror(errno));
  }
}

ProgramExecutor::ProgramExecutor(ArgList args, EnvironMap environMap, Path workDir) :
  _args(std::move(args)),
  _environ(std::move(environMap)),
  _workDir(std::move(workDir)),
  _waitMutex(new Poco::Mutex()),
  _waitCond(new Poco::Condition()),
  _waitThread(new Poco::Thread()),
  _killMutex(new Poco::Mutex()),
  _status(NOT_STARTED),
  _waitStatus(0),
  _processId(-1),
  _pipeFd(0)
{
  if (_args.empty()) {
    throw Poco::InvalidArgumentException("`args` must not be empty.");
  }
}

ProgramExecutor::~ProgramExecutor() {
  if (_status == RUNNING) {
    kill();
  }
  if (!_waitThread->tryJoin(3000)) {
    Logger::getLogger().warn("The background waiting thread cannot be stopped.");
  }
  delete _waitThread;
  delete _waitCond;
  delete _killMutex;
  delete _waitMutex;
}

void ProgramExecutor::start() {
  if (_status != NOT_STARTED) {
    throw Poco::IllegalStateException("Process has started.");
  }

  int pfd[2] = {0};
  if (pipe(pfd) != 0) {
    throw Poco::SystemException("Failed to open pipe: " + errorMessage());
  }

  _processId = fork();
  if (_processId == 0) {  // child process
    close(pfd[0]);
    _pipeFd = pfd[1];

    // redirect stdout and stderr to the pipe
    dup2(_pipeFd, STDOUT_FILENO);
    dup2(_pipeFd, STDERR_FILENO);

    // change the current directory
    if (!_workDir.empty()) {
      char* workDir = copyAsCString(_workDir);
      if (chdir(workDir) != 0) {
        fprintf(stderr, "Cannot chdir to working directory \"%s\": %s\n", workDir, errorMessage().c_str());
        exit(-1);
      }
      std::free(workDir);
    }

    // prepare the args for launching the user program
    char* programFile = copyAsCString(_args.at(0));
    char** programArgs = (char**)malloc(sizeof(char**) * (_args.size() + 1));

    programArgs[0] = programFile;
    programArgs[_args.size()] = 0;
    for (size_t i=1; i<_args.size(); ++i) {
      programArgs[i] = copyAsCString(_args.at(i));
    }

    // set the environmental variables
    for (auto &it : _environ) {
      setenv(it.first.c_str(), it.second.c_str(), 1);
    }

    // start the user program
    if (execvp(programFile, programArgs) != 0) {
      fprintf(stderr, "Cannot launch the program \"%s\": %s\n", programFile, errorMessage().c_str());
      exit(-1);
    }

  } else { // parent process
    close(pfd[1]);
    _pipeFd = pfd[0];
    _status = RUNNING;
    _waitThread->startFunc([this] {
      this->_waitInBackground();
    });
    Logger::getLogger().info("Program launched.");
  }
}

void ProgramExecutor::_waitInBackground() {
  int status;
  pid_t waitRet = waitpid(_processId, &status, 0);

  if (waitRet > 0) {
    Poco::Mutex::ScopedLock scopedLock(*_waitMutex);

    if (WIFEXITED(status)) {
      _status = EXITED;
      _waitStatus = status;
      _waitCond->broadcast();
      Logger::getLogger().info("Program exited normally with code: %d", exitCode());

    } else if (WIFSIGNALED(status)) {
      _status = SIGNALLED;
      _waitStatus = status;
      _waitCond->broadcast();
      Logger::getLogger().info("Program killed by signal: %d", exitSignal());

    } else {
      Logger::getLogger().warn("Unexpected wait status: %x", _waitStatus);
    }

  } else {
    Logger::getLogger().error("Failed to wait for child process: %s", errorMessage());
  }
}

bool ProgramExecutor::poll() {
  REQUIRE_STARTED();
  return _status != RUNNING;
}

bool ProgramExecutor::wait(long timeout) {
  Poco::Mutex::ScopedLock scopedLock(*_waitMutex);
  REQUIRE_STARTED();
  if (_status == RUNNING) {
    if (timeout <= 0) {
      _waitCond->wait(*_waitMutex);
      return true;
    } else {
      return _waitCond->tryWait(*_waitMutex, timeout);
    }
  } else {
    return true;
  }
}

ssize_t ProgramExecutor::readOutput(void *target, size_t count) {
  REQUIRE_STARTED();
  return read(_pipeFd, target, count);
}

void ProgramExecutor::_killIfRunning(int signal) {
  Poco::Mutex::ScopedLock scopedLock(*_waitMutex);
  if (_status == RUNNING) {
    ::kill(_processId, signal);
  }
}

void ProgramExecutor::kill() {
  REQUIRE_STARTED();

  Poco::Mutex::ScopedLock scopedLock(*_killMutex);

  // First step, attempt to kill by signal SIGINT
  _killIfRunning(SIGINT);
  if (!wait(10 * 1000)) {
    // Second step, attempt to kill by signal SIGINT.
    //
    // Some applications may listen to a double CTRL+C signal, in which the first
    // Ctrl+C tells the application to exit cleanly, while the second Ctrl+C tells
    // the application to exit immediately.  We thus attempt to emit this second
    // Ctrl+C signal here.
    Logger::getLogger().warn("Program does not exit after received Ctrl+C for 10 seconds, "
                             "send Ctrl+C again.");
    _killIfRunning(SIGINT);

    if (!wait(20 * 1000)) {
      Logger::getLogger().warn("Program does not exit after received double Ctrl+C for 20 seconds, "
                               "now ready to kill it.");
      _killIfRunning(SIGKILL);

      if (!wait(60 * 1000)) {
        Logger::getLogger().warn("Program does not exit after being killed for 60 seconds, now give up.");
        Poco::Mutex::ScopedLock scopedLock2(*_waitMutex);
        _status = CANNOT_KILL;
        close(_pipeFd);  // force closing the pipe, in order for the IO controller to stop
        _waitCond->broadcast();  // notify all threads waiting on the process to exit
      }
    }
  }
}
