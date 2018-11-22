#include <utility>

#include <memory>
#include <iostream>
#include <signal.h>
#include <Poco/ErrorHandler.h>
#include <Poco/String.h>
#include <Poco/File.h>
#include <Poco/FileStream.h>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include <Poco/JSON/Object.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPServer.h>
#include "version.h"
#include "Utils.h"
#include "Logger.h"
#include "BaseApp.h"
#include "ProgramExecutor.h"
#include "OutputBuffer.h"
#include "WebServerFactory.h"
#include "IOController.h"
#include "AutoFreePtr.h"
#include "GeneratedFilesWatcher.h"
#include "PersistAndCallbackManager.h"

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
  struct SignalHandlerMutexAndCondition {
    Poco::Mutex mutex;
    Poco::Condition cond;
  };
  class ScopedSignalHandler;
  std::vector<ScopedSignalHandler*> scopedSignalHandlers;

  /** Type of callback function on handling signal. */
  typedef std::function<void()> SignalCallbackType;

  /**
   * The scoped signal handling manager.
   */
  class ScopedSignalHandler {
  private:
    volatile bool _destroying = false;
    SignalCallbackType _callback;
    Poco::Mutex _mutex;
    Poco::Condition _cond;
    Poco::Thread _waitSignalThread;

  public:
    explicit ScopedSignalHandler(SignalCallbackType callback) : _callback(std::move(callback)) {
      scopedSignalHandlers.push_back(this);

      // create a background waiting thread, which will call {@arg callback}
      // on the signal.
      _waitSignalThread.startFunc([this] {
        Poco::Mutex::ScopedLock scopedLock(_mutex);
        _cond.wait(_mutex);
        if (!_destroying) {
          this->_callback();
        }
      });
    }

    void waitForTermination() {
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      if (!interrupted && !_destroying) {
        _cond.wait(_mutex);
      }
    }

    void notify() {
      Poco::Mutex::ScopedLock scopedLock(_mutex);
      _cond.broadcast();
    }

    ~ScopedSignalHandler() {
      // force all threads waiting on the signal to wake up
      {
        Poco::Mutex::ScopedLock scopedLock(_mutex);
        _destroying = true;
        _cond.broadcast();
      }

      // join on the background thread if it has started
      _waitSignalThread.join();
    }
  };

  void globalSignalHandler(int signal_value) {
    switch (signal_value) {
      case SIGINT:
      case SIGTERM:
        interrupted = true;
        if (!scopedSignalHandlers.empty()) {
          scopedSignalHandlers.back()->notify();
        } else {
          exit(0);
        }
        break;
      default:
        break;
    }
  }

  void installGlobalSignalHandler() {
    struct sigaction action;
    action.sa_handler = globalSignalHandler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
  }


  template <typename T>
  class AutoPtr {
    T* _ptr;

  public:
    explicit AutoPtr(T *ptr) : _ptr(ptr) {}
    ~AutoPtr() { delete _ptr; _ptr = nullptr; }

    T& operator*() { return *_ptr; }
    T* operator->() { return _ptr; }
  };
}


class MainApp : public BaseApp {
protected:
  int runApp() override {
    // Install the global error handler
    Poco::ErrorHandler::set(new ErrorHandler());

    // Install the global signal handler
    installGlobalSignalHandler();

    // Get the system shell configuration
    std::string shell = getenv("SHELL");
    if (shell.empty()) {
      shell = "sh";
    }

    // Ensure the status file not exist
    if (!_statusFile.empty() && Poco::File(_statusFile).exists()) {
      Logger::getLogger().error("The status file already exists: %s", _statusFile);
      return Application::EXIT_SOFTWARE;
    }

    // Get the server hostname
    std::string hostName = Poco::Net::DNS::hostName();

    // Display the configurations specified by the CLI arguments.
    auto& logger = Logger::getLogger();
    logger.info("ML GridEngine Executor " APP_VERSION);
    logger.info("Shell: %s", shell);
    logger.info("Hostname: %s", hostName);
    logger.info("Wait termination: %s", std::string(_noExit ? "yes" : "no"));
    logger.info("Watch generated files: %s", std::string(_watchGenerated ? "yes" : "no"));
    logger.info("Memory buffer size: %z (%s)", _bufferSize, Utils::formatSize(_bufferSize));
    logger.info("Working dir: %s", _workDir);
    if (!_callbackAPI.empty()) {
      logger.info("Callback API: %s", _callbackAPI);
    }
    if (!_callbackToken.empty()) {
      logger.info("Callback Token: %s", _callbackToken);
    }
    if (!_outputFile.empty()) {
      logger.info("Output file: %s", _outputFile);
    }
    if (!_statusFile.empty()) {
      logger.info("Status file: %s", _statusFile);
    }
    if (!_runAfter.empty()) {
      logger.info("Run-after command: %s", _runAfter);
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

    // Prepare for the working directory
    Poco::File workDir(_workDir);
    if (!workDir.exists()) {
      workDir.createDirectories();
    }

    // Initialize all related objects.
    PersistAndCallbackManager persistAndCallback(_statusFile, _callbackAPI, _callbackToken);
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
    Logger::getLogger().info("HTTP server started at http://%s:%?u", hostName, server.socket().address().port());

    // Run the main user program.
    {
      std::shared_ptr<GeneratedFilesWatcher> filesWatcher;
      if (_watchGenerated && persistAndCallback.enabled()) {
        filesWatcher = std::make_shared<GeneratedFilesWatcher>(
            _workDir,
            [&persistAndCallback] (std::string const& fileTag, Poco::JSON::Object const& jsonObject) {
              persistAndCallback.fileGenerated(fileTag, jsonObject);
            });
        filesWatcher->start();
      }
      {
        ExecutorScope mainExecutorScope(&executor);
        executor.start();
        ioController.start();

        // Notify the server that we've started the program.
        if (persistAndCallback.enabled()) {
          persistAndCallback.programStarted(hostName, server.socket().address().port());
        }
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
      if (filesWatcher) {
        filesWatcher->stop();
        filesWatcher->collectAll(); // force reading all watched files before exit.
      }
    }

    // Wait for the IO controller to stop.
    ioController.join();
    outputBuffer.close();
    Logger::getLogger().info("Total number of bytes output by the program: %z (%s)",
        outputBuffer.writtenBytes(), Utils::formatSize(outputBuffer.writtenBytes()));

    // Save the output if required
    if (!_outputFile.empty()) {
      try {
        const size_t bufferSize = 8192;
        size_t begin = 0;
        ReadResult readResult;
        AutoFreePtr<char> buffer((char*)malloc(bufferSize));
        Poco::FileStream outStream(_outputFile, std::ios::out | std::ios::trunc);

        if (outputBuffer.writtenBytes() > outputBuffer.size()) {
          size_t dSize = outputBuffer.writtenBytes() - outputBuffer.size();
          std::string dSizeFormatted = Utils::formatSize(dSize);
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
              Utils::formatSize(outputBuffer.size()), _outputFile);
        } else {
          Logger::getLogger().info("All output saved to: %s", _outputFile);
        }
      } catch (Poco::Exception const& exc) {
        Logger::getLogger().error("Failed to save the output to %s:\n%s", _outputFile, exc.displayText());
      }
    }

    // notify the callback API that the program has completed
    if (persistAndCallback.enabled()) {
      persistAndCallback.programFinished(executor);
    }

    // run command after execution
    if (!_runAfter.empty() && !interrupted) {
      // prepare for the command
      ArgList runAfterArgs = {shell, "-c", _runAfter};
      EnvironMap environ(_environ);
      environ[ML_GRIDENGINE_ENV_PREFIX "PROGRAM_WORK_DIR"] = _workDir;
      switch (executor.status()) {
        case EXITED:
          environ[ML_GRIDENGINE_ENV_PREFIX "PROGRAM_EXIT_STATUS"] = "EXITED";
          environ[ML_GRIDENGINE_ENV_PREFIX "PROGRAM_EXIT_CODE"] = Poco::format("%d", executor.exitCode());
          break;
        case SIGNALLED:
          environ[ML_GRIDENGINE_ENV_PREFIX "PROGRAM_EXIT_STATUS"] = "SIGNALLED";
          environ[ML_GRIDENGINE_ENV_PREFIX "PROGRAM_EXIT_SIGNAL"] = Poco::format("%d", executor.exitSignal());
          break;
        case CANNOT_KILL:
          environ[ML_GRIDENGINE_ENV_PREFIX "PROGRAM_EXIT_STATUS"] = "CANNOT_KILL";
          break;
        default:
          break;
      }

      // run the command
      ProgramExecutor runAfterExecutor(runAfterArgs, environ, std::string(), false, "Run-after command");
      runAfterExecutor.start();
      ScopedSignalHandler scopedSignalHandler([&runAfterExecutor] {
        Logger::getLogger().info("Termination signal received, kill \"run after\" command.");
        runAfterExecutor.kill();
      });
      runAfterExecutor.wait();
    }

    // wait for the persist and callback manager to finish
    if (persistAndCallback.enabled()) {
      if (_noExit && !interrupted) {
        ScopedSignalHandler scopedSignalHandler([&persistAndCallback] {
          Logger::getLogger().info("Termination signal received, stop the persist and callback manager ...");
          persistAndCallback.interrupt();
        });
        persistAndCallback.wait();
      } else if (!interrupted) {
        persistAndCallback.interrupt();
        persistAndCallback.wait();
      } else {
        persistAndCallback.wait();
      }
      Logger::getLogger().info("PersistAndCallbackManager has stopped.");
    }

    // If no exit, wait for termination
    if (_noExit && !interrupted) {
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

int main(int argc, char** argv)
{
  Poco::AutoPtr<MainApp> pApp(new MainApp());
  try {
    pApp->init(argc, argv);
  } catch (Poco::Exception &exc) {
    Logger::getLogger().error("Configuration error: " + exc.displayText());
    return Poco::Util::Application::EXIT_CONFIG;
  }
  return pApp->run();
}