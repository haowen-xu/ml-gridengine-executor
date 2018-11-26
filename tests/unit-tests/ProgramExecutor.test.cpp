//
// Created by 许昊文 on 2018/11/25.
//

#include <unistd.h>
#include <signal.h>
#include <string>
#include <Poco/Thread.h>
#include <catch2/catch.hpp>
#include <src/Logger.h>
#include <src/AutoFreePtr.h>
#include "src/OutputBuffer.h"
#include "src/ProgramExecutor.h"
#include "CapturingLogger.h"
#include "macros.h"

#define REQUIRE_OUTPUT_EQUALS(output, expected) \
  REQUIRE_EQUALS(std::string((char*)(output).data(), (output).size()), (expected))
#define CAPTURE_LOGGING() \
  CapturingLogger logger; Logger::ScopedRootLogger scopedRootLogger(&logger);

namespace {
  void runExecutor(ProgramExecutor *executor, std::vector<Byte> *output=nullptr) {
    std::shared_ptr<Poco::Thread> ioThread;
    if (output != nullptr) {
      ioThread = std::shared_ptr<Poco::Thread>(new Poco::Thread);
    }
    if (executor->status() == NOT_STARTED) {
      executor->start();
    }
    if (output != nullptr) {
      ioThread->startFunc([&executor, output] () {
        static const int bufferSize = 8192;
        AutoFreePtr<Byte> buffer((Byte*)malloc(bufferSize));
        ssize_t bytesRead;
        while ((bytesRead = executor->readOutput(buffer.ptr, bufferSize)) > 0) {
          size_t oldSize = output->size();
          output->resize(oldSize + bytesRead);
          memcpy(output->data() + oldSize, buffer.ptr, (size_t)bytesRead);
        }
      });
    }
    REQUIRE(executor->wait());
    if (output != nullptr) {
      ioThread->join();
    }
  }
}

TEST_CASE("Test executing hello world program.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"echo", "hello, $world!"});
  runExecutor(&executor, &output);
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_EQUALS(executor.exitCode(), 0);
  REQUIRE_OUTPUT_EQUALS(output, "hello, $world!\n");

  // wait on exited program will do nothing
  REQUIRE(executor.wait());
  // kill exited program will do nothing
  executor.kill();
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_EQUALS(executor.exitCode(), 0);
}

TEST_CASE("Test capturing stderr.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"sh", "-c", "echo stdout; (>&2 echo \"stderr\")"});
  runExecutor(&executor, &output);
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_EQUALS(executor.exitCode(), 0);
  REQUIRE_OUTPUT_EQUALS(output, "stdout\nstderr\n");
}

TEST_CASE("Test exit with non-zero code.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"sh", "-c", "exit 123"});
  runExecutor(&executor, &output);
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_EQUALS(executor.exitCode(), 123);
}

TEST_CASE("Test feeding environmental variables.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor(
      {"sh", "-c", "echo $ML_GRIDENGINE_EXECUTOR_TEST_ENV_1; echo $ML_GRIDENGINE_EXECUTOR_TEST_ENV_2"},
      {
        {"ML_GRIDENGINE_EXECUTOR_TEST_ENV_1", "Value 1"},
        {"ML_GRIDENGINE_EXECUTOR_TEST_ENV_2", "Value 2"}
      });
  runExecutor(&executor, &output);
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_OUTPUT_EQUALS(output, "Value 1\nValue 2\n");
}

TEST_CASE("Test setting the working directory.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"pwd"}, {}, "/usr");
  runExecutor(&executor, &output);
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_OUTPUT_EQUALS(output, "/usr\n");
}

TEST_CASE("Test gracefully killing.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"python", "-c", "import time\n"
                                            "for i in range(1000):\n"
                                            "  print(i)\n"
                                            "  time.sleep(1)\n"});
  Poco::Thread runThread;
  executor.start();
  runThread.startFunc([&executor, &output] () {
    runExecutor(&executor, &output);
  });
  REQUIRE_FALSE(executor.wait(100));
  executor.kill();
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_FALSE(executor.exitCode() == 0);
  REQUIRE_OUTPUT_EQUALS(output, "0\n"
                                "Traceback (most recent call last):\n"
                                "  File \"<string>\", line 4, in <module>\n"
                                "KeyboardInterrupt\n");
}

TEST_CASE("Test force killing.", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"python", "-c", "import time\n"
                                            "i = 0\n"
                                            "while True:\n"
                                            "  try:\n"
                                            "    while True:\n"
                                            "      print(i)\n"
                                            "      i += 1\n"
                                            "      time.sleep(1)\n"
                                            "  except KeyboardInterrupt:\n"
                                            "    print('keyboard interrupt')\n"});
  Poco::Thread runThread;
  executor.start();
  runThread.startFunc([&executor, &output] () {
    runExecutor(&executor, &output);
  });
  REQUIRE_FALSE(executor.wait(100));
  executor.kill(.5, 1.5, 10);
  REQUIRE_EQUALS(executor.status(), SIGNALLED);
  REQUIRE_EQUALS(executor.exitSignal(), SIGKILL);
  REQUIRE_OUTPUT_EQUALS(output, "0\n"
                                "keyboard interrupt\n"
                                "1\n"
                                "keyboard interrupt\n"
                                "2\n"
                                "3\n");
}

TEST_CASE("Multiple wait and multiple kill", "[ProgramExecutor]") {
  CAPTURE_LOGGING();
  std::vector<Byte> output;
  ProgramExecutor executor({"python", "-c", "import time\n"
                                            "for i in range(1000):\n"
                                            "  print(i)\n"
                                            "  time.sleep(.1)\n"});
  Poco::Thread runThread;
  executor.start();
  runThread.startFunc([&executor, &output] () {
    runExecutor(&executor, &output);
  });

  // spawn 100 waiting threads
  Poco::Thread waitThreads[100];
  bool waitThreadResults[100] = {false};
  for (int i=0; i<100; ++i) {
    waitThreads[i].startFunc([i, &executor, &waitThreadResults] () {
      waitThreadResults[i] = executor.wait();
    });
  }

  // wait for 450ms
  usleep(410 * 1000);

  // spawn 100 killing threads
  Poco::Thread killThreads[100];
  for (int i=0; i<100; ++i) {
    killThreads[i].startFunc([i, &executor] () {
      executor.kill();
    });
  }

  // wait for the threads to exit
  runThread.join();
  for (auto &thread: waitThreads) {
    thread.join();
  }
  for (auto &thread: killThreads) {
    thread.join();
  }

  // check the results
  REQUIRE_EQUALS(executor.status(), EXITED);
  REQUIRE_FALSE(executor.exitCode() == 0);
  REQUIRE_OUTPUT_EQUALS(output, "0\n1\n2\n3\n"
                                "Traceback (most recent call last):\n"
                                "  File \"<string>\", line 4, in <module>\n"
                                "KeyboardInterrupt\n");
  for (int i=0; i<100; ++i) {
    REQUIRE(waitThreadResults[i]);
  }
}
