//
// Created by 许昊文 on 2018/11/26.
//

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <Poco/Mutex.h>
#include <Poco/Semaphore.h>
#include <catch2/catch.hpp>
#include <src/SignalHandler.h>
#include <src/AutoFreePtr.h>
#include <Poco/Environment.h>
#include "CapturingLogger.h"

#define REQUIRE_OUTPUT_EQUALS(output, expected) \
  REQUIRE_EQUALS(std::string((char*)(output).data(), (output).size()), (expected))
#define CAPTURE_LOGGING() \
  CapturingLogger logger; Logger::ScopedRootLogger scopedRootLogger(&logger);

namespace {
  std::string getSignalHandlerExampleExe() {
    char* programFile = getenv("SIGNAL_HANDLER_EXAMPLE_EXE");
    if (programFile == NULL)
      return std::string();
    return std::string(programFile);
  }

  void childProc(std::string const& path) {
    char* programFile = (char*)malloc(path.size() + 1);
    memcpy(programFile, path.c_str(), path.size() + 1);
    char* const programArgs[2] = {programFile, nullptr};
    if (execvp(programFile, programArgs) != 0) {
      fprintf(stderr, "Cannot launch SignalHandlerExample: %s\n", strerror(errno));
    }
  }

  void parentProc(int pid, int killSignal) {
    usleep(200 * 1000);
    ::kill(pid, killSignal);
    int status;
    waitpid(pid, &status, 0);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 123);
  }
}

TEST_CASE("Test kill by SIGINT", "[SignalHandler]") {
  int pid = fork();
  std::string signalHandlerExampleExe = getSignalHandlerExampleExe();
  REQUIRE(!signalHandlerExampleExe.empty());
  if (pid == 0) {
    childProc(signalHandlerExampleExe);
  } else {
    parentProc(pid, SIGINT);
  }
}

TEST_CASE("Test kill by SIGTERM", "[SignalHandler]") {
  int pid = fork();
  std::string signalHandlerExampleExe = getSignalHandlerExampleExe();
  REQUIRE(!signalHandlerExampleExe.empty());
  if (pid == 0) {
    childProc(signalHandlerExampleExe);
  } else {
    parentProc(pid, SIGTERM);
  }
}
