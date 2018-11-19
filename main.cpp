#include <utility>
#include <iostream>
#include <signal.h>
#include <Poco/ErrorHandler.h>
#include <Poco/String.h>
#include <Poco/File.h>
#include <Poco/FileStream.h>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include <Poco/Net/HTTPServer.h>
#include "src/version.h"
#include "src/FormatUtils.h"
#include "src/Logger.h"
#include "src/BaseApp.h"
#include "src/ProgramExecutor.h"
#include "src/OutputBuffer.h"
#include "src/WebServerFactory.h"
#include "src/IOController.h"
#include "src/AutoFreePtr.h"

using namespace Poco::Net;


namespace {
  /**
   * Poco error handler for background threads.
   */
  class ErrorHandler : public Poco::ErrorHandler {
  public:
    void exception(const Poco::Exception &exc) {
      log(exc.displayText());
    }

    void exception(const std::exception &exc) {
      log(exc.what());
    }

    void exception() {
      log("unknown exception");
    }

    void log(const std::string &message) {
      Logger::getLogger().error("A thread was terminated by an unhandled exception: %s", message);
    }
  };

  /**
   * Ensure to kill the executor when exiting the scope.
   */
  class ExecutorScope {
  private:
    ProgramExecutor *_executor;

  public:
    explicit ExecutorScope(ProgramExecutor *executor) : _executor(executor) {}
    ~ExecutorScope() { _executor->kill(); }
  };

  // ----------------------------------------
  // Global SIGINT and SIGTERM signal handler
  // ----------------------------------------
  volatile bool interrupted = false;
  Poco::Mutex *signalMutex = nullptr;
  Poco::Condition *signalCond = nullptr;

  void signalHandler(int signal_value) {
    switch (signal_value) {
      case SIGINT:
      case SIGTERM:
        {
          Poco::Mutex::ScopedLock scopedLock(*signalMutex);
          interrupted = true;
          signalCond->broadcast();
        }
        break;
      default:
        break;
    }
  }

  /** Type of callback function on handling signal. */
  typedef std::function<void()> SignalCallbackType;

  /**
   * The scoped signal handling manager.
   */
  class ScopedSignalHandler {
  private:
    volatile bool _destroying = false;
    Poco::Thread _waitSignalThread;
    SignalCallbackType _callback;

    void _installSignalHandler() {
      signalMutex = new Poco::Mutex();
      signalCond = new Poco::Condition();

      // install the signal handler
      struct sigaction action;
      action.sa_handler = signalHandler;
      action.sa_flags = 0;
      sigemptyset(&action.sa_mask);
      sigaction(SIGINT, &action, NULL);
      sigaction(SIGTERM, &action, NULL);
    }

  public:
    explicit ScopedSignalHandler(SignalCallbackType callback) : _callback(std::move(callback)) {
      _installSignalHandler();

      // create a background waiting thread, which will call {@arg callback}
      // on the signal.
      _waitSignalThread.startFunc([this] {
        Poco::Mutex::ScopedLock scopedLock(*signalMutex);
        signalCond->wait(*signalMutex);
        if (!_destroying) {
          this->_callback();
        }
      });
    }

    void waitForTermination() {
      Poco::Mutex::ScopedLock scopedLock(*signalMutex);
      if (!interrupted) {
        signalCond->wait(*signalMutex);
      }
    }

    ~ScopedSignalHandler() {
      // uninstall the signal handler
      struct sigaction action;
      action.sa_handler = SIG_DFL;
      action.sa_flags = 0;
      sigemptyset(&action.sa_mask);
      sigaction(SIGINT, &action, NULL);
      sigaction(SIGTERM, &action, NULL);

      // force all threads waiting on the signal to wake up
      {
        Poco::Mutex::ScopedLock scopedLock(*signalMutex);
        _destroying = true;
        signalCond->broadcast();
      }

      // join on the background thread if it has started
      _waitSignalThread.join();

      // now delete the signal facilities
      delete signalMutex;
      delete signalCond;
      signalMutex = nullptr;
      signalCond = nullptr;
    }
  };
}


class MainApp : public BaseApp {
protected:
  int runApp() override {
    // Install the global error handler
    Poco::ErrorHandler::set(new ErrorHandler());

    // Display the configurations specified by the CLI arguments.
    auto& logger = Logger::getLogger();
    logger.info("ML GridEngine Executor " APP_VERSION);
    logger.info("Server host: %s", _serverHost);
    logger.info("Server port: %?d", _serverPort);
    logger.info("Memory buffer size: %z (%s)", _bufferSize, FormatUtils::formatSize(_bufferSize));
    logger.info("Working dir: %s", _workDir);
    if (!_callbackAPI.empty()) {
      logger.info("Callback API: %s", _callbackAPI);
    }
    if (!_saveOutput.empty()) {
      logger.info("Save output to: %s", _saveOutput);
    }
    logger.info("Program arguments:\n  %s",
        Poco::cat(std::string("\n  "), _args.begin(), _args.end()));
    if (!_environ.empty()) {
      std::vector<std::string> environList;
      for (auto const& it: _environ) {
        environList.push_back(Poco::format("%s=%s", it.first, it.second));
      }
      logger.info("Environmental variables:\n  %s",
          Poco::cat(std::string("\n  "), environList.begin(), environList.end()));
    }

    // Initialize all related objects.
    ProgramExecutor executor(_args, _environ, _workDir);
    OutputBuffer outputBuffer(_bufferSize);
    IOController ioController(&executor, &outputBuffer);
    SocketAddress serverAddr;
    if (!_serverHost.empty()) {
      serverAddr = SocketAddress(_serverHost, _serverPort);
    } else {
      serverAddr = SocketAddress(_serverPort);
    }
    HTTPServer server(
        new WebServerFactory(&executor, &outputBuffer),
        ServerSocket(serverAddr),
        new HTTPServerParams());
    server.start();
    Logger::getLogger().info("HTTP server started at http://%s", server.socket().address().toString());

    // Run the main user program.
    {
      ExecutorScope mainExecutorScope(&executor);
      executor.start();
      ioController.start();
      {
        // This nested scope must exist, such that the child process will not install
        // this signal handler.
        ScopedSignalHandler scopedSignalHandler([&executor] {
          Logger::getLogger().info("Termination signal received, kill the user program ...");
          executor.kill();
        });
        executor.wait();
      }
    }

    // Wait for the IO controller to stop.
    ioController.join();
    outputBuffer.close();
    Logger::getLogger().info("Total number of bytes output by the program: %z (%s)",
        outputBuffer.writtenBytes(), FormatUtils::formatSize(outputBuffer.writtenBytes()));

    // Save the output if required
    if (!_saveOutput.empty()) {
      try {
        const size_t bufferSize = 8192;
        size_t begin = 0;
        ReadResult readResult;
        AutoFreePtr<char> buffer((char*)malloc(bufferSize));
        Poco::FileStream outStream(_saveOutput, std::ios::out | std::ios::trunc);

        if (outputBuffer.writtenBytes() > outputBuffer.size()) {
          size_t dSize = outputBuffer.writtenBytes() - outputBuffer.size();
          std::string dSizeFormatted = FormatUtils::formatSize(dSize);
          if (!dSizeFormatted.empty() && dSizeFormatted.at(dSizeFormatted.size() - 1) != 'B') {
            outStream << Poco::format("[%z (%s) bytes discarded]", dSize, dSizeFormatted) << std::endl;
          } else {
            outStream << Poco::format("[%z bytes discarded]", dSize) << std::endl;
          }
        }

        while (!(readResult = outputBuffer.read(begin, buffer.ptr, bufferSize)).isClosed) {
          outStream.write(buffer.ptr, readResult.count);
          begin = readResult.begin + readResult.count;
        }

        if (outputBuffer.writtenBytes() > outputBuffer.size()) {
          Logger::getLogger().info("The last %s output saved to: %s",
              FormatUtils::formatSize(outputBuffer.size()), _saveOutput);
        } else {
          Logger::getLogger().info("All output saved to: %s", _saveOutput);
        }
      } catch (Poco::Exception const& exc) {
        Logger::getLogger().error("Failed to save the output to %s:\n%s", _saveOutput, exc.displayText());
      }
    }

    // If no exit, wait for termination
    if (_noExit) {
      ScopedSignalHandler scopedSignalHandler([] {
        Logger::getLogger().info("Termination signal received.");
      });
      scopedSignalHandler.waitForTermination();
    }

    // Stop the server
    Logger::getLogger().info("HTTP server shutdown ...");
    server.stop();

    return Application::EXIT_OK;
  }
};

POCO_APP_MAIN(MainApp)
