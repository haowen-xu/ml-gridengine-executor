#include <iostream>
#include <Poco/ErrorHandler.h>
#include <Poco/String.h>
#include <Poco/File.h>
#include <Poco/FileStream.h>
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


class ErrorHandler : public Poco::ErrorHandler {
public:
  void exception(const Poco::Exception& exc)
  {
    Logger::getLogger().error("A thread was terminated by an unhandled exception: %s", exc.displayText());
  }

  void exception(const std::exception& exc)
  {
    Logger::getLogger().error("A thread was terminated by an unhandled exception: %s", exc.what());
  }

  void exception()
  {
    Logger::getLogger().error("A thread was terminated by an unhandled exception");
  }

  void log(const std::string& message)
  {
    Logger::getLogger().error("A thread was terminated by an unhandled exception: %s", message);
  }
};


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

    // Initialize the program executor and the buffer, and start the IO controller
    ProgramExecutor executor(_args, _environ, _workDir);
    OutputBuffer outputBuffer(_bufferSize);
    IOController ioController(&executor, &outputBuffer);
    executor.start();
    ioController.start();

    // Start the HTTP server
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

    // Wait for the program to exit, and the IO controller to stop.
    executor.wait();
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

    // Stop the server
    Logger::getLogger().info("HTTP server shutdown ...");
    server.stop();

    return Application::EXIT_OK;
  }
};

POCO_APP_MAIN(MainApp)
