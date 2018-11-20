//
// Created by 许昊文 on 2018/11/17.
//

#include <vector>
#include <iostream>
#include <cassert>
#include <Poco/NumberParser.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/RegularExpression.h>
#include <Poco/String.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/IntValidator.h>
#include <Poco/Util/RegExpValidator.h>
#include "BaseApp.h"
#include "Logger.h"

using namespace Poco::Util;

#define BUFFER_SIZE_PATTERN "^(\\d+(?:\\.\\d*)?)\\s*([MKmk]?[Bb]?)$"


void BaseApp::displayHelp(std::ostream& out) {
  HelpFormatter helpFormatter(options());
  helpFormatter.setCommand(commandName());
  helpFormatter.setUsage("[OPTIONS] [--] ARGS...");
  helpFormatter.setHeader(Poco::format(
      "ML GridEngine user program executor.\n"
      "\n"
      "The program arguments should be specified at the end, after a \"--\" mark,\n"
      "for example:\n"
      "\n"
      "  %s -- python -u train.py\n"
      "\n"
      "Options:",
      commandName()));
  helpFormatter.setAutoIndent();
  helpFormatter.format(out);
}

void BaseApp::displayHelp() {
  displayHelp(std::cout);
}

void BaseApp::defineOptions(OptionSet &options) {
  Application::defineOptions(options);

  options.addOption(
      Option("help", "h", "Show this message and exit.")
          .required(false)
          .repeatable(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleHelp)));

  options.addOption(
      Option("work-dir", "w", "Set the program's working directory.")
          .argument("PATH")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetWorkDir)));

  options.addOption(
      Option("env", "e", "Set environmental variable NAME=VALUE.")
          .argument("NAME=VALUE")
          .required(false)
          .repeatable(true)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetEnviron))
          .validator(new RegExpValidator("^([^=]+)=(.*)$")));

  options.addOption(
      Option().fullName("no-exit")
          .description("Do not exit the executor after the program has finished. "
                       "Wait for termination signals.")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetNoExit)));

  options.addOption(
      Option().fullName("watch-generated")
          .description("Watch generated JSON files (\"config.json\", \"config.defaults.json\", "
                       "\"result.json\"), and submit them via the callback API.")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetWatchGenerated)));

  options.addOption(
      Option("server-host", "h", "The listening host for the executor server.")
          .argument("HOST")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetServerHost)));

  options.addOption(
      Option("server-port", "p", "The listening port for the executor server.")
          .argument("PORT")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetServerPort))
          .validator(new IntValidator(0, 65535)));

  options.addOption(
      Option().fullName("buffer-size")
          .description("Set the memory buffer size.")
          .argument("BUFFER-SIZE")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetBufferSize))
          .validator(new RegExpValidator(BUFFER_SIZE_PATTERN)));

  options.addOption(
      Option().fullName("callback-api")
          .description("Set the URI of the callback API.")
          .argument("URI")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetCallbackAPI))
          .validator(new RegExpValidator("^http://.+$")));

  options.addOption(
      Option().fullName("callback-token")
          .description("Set the auth token of the callback API.")
          .argument("TOKEN")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetCallbackToken)));

  options.addOption(
      Option().fullName("save-output")
          .description("Save program output to this path.")
          .argument("PATH")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetSaveOutput))
          .validator(new RegExpValidator("^.+$")));

  options.addOption(
      Option().fullName("run-after")
          .description("Run shell command after the program has executed. "
                       "It will always run under the working directory of the executor, rather than "
                       "that specified by \"--work-dir\". Also, the failure of this command will not "
                       "affect the final status of the user program.")
          .argument("COMMAND")
          .required(false)
          .callback(OptionCallback<BaseApp>(this, &BaseApp::handleSetRunAfter))
          .validator(new RegExpValidator("^.+$")));
}

void BaseApp::handleHelp(const std::string &name, const std::string &value) {
  _helpRequested = true;
  displayHelp();
  stopOptionsProcessing();
}

void BaseApp::handleSetWorkDir(const std::string &name, const std::string &value) {
  _workDir = value;
}

void BaseApp::handleSetEnviron(const std::string &name, const std::string &value) {
  ssize_t pos = value.find('=');
  assert(pos >= 1);
  std::string eName = value.substr(0, (size_t) pos);
  std::string eValue = value.substr((size_t) pos + 1);
  _environ[eName] = eValue;
}

void BaseApp::handleSetNoExit(const std::string &name, const std::string &value) {
  _noExit = true;
}

void BaseApp::handleSetWatchGenerated(const std::string &name, const std::string &value) {
  _watchGenerated = true;
}

void BaseApp::handleSetServerHost(const std::string &name, const std::string &value) {
  _serverHost = value;
}

void BaseApp::handleSetServerPort(const std::string &name, const std::string &value) {
  _serverPort = Poco::NumberParser::parse(value);
}

void BaseApp::handleSetBufferSize(const std::string &name, const std::string &value) {
  Poco::RegularExpression p(BUFFER_SIZE_PATTERN);
  Poco::RegularExpression::MatchVec m;
  p.match(value, 0, m);

  std::string sValue = value.substr(m.at(1).offset, m.at(1).length);
  std::string sUnit = Poco::toUpper(value.substr(m.at(2).offset, m.at(2).length));
  double bufferSize = Poco::NumberParser::parseFloat(sValue);
  if (sUnit == "M" || sUnit == "MB") {
    bufferSize *= 1024 * 1024;
  } else if (sUnit == "K" || sUnit == "KB") {
    bufferSize *= 1024;
  }

  _bufferSize = (size_t)bufferSize;
}

void BaseApp::handleSetCallbackAPI(const std::string &name, const std::string &value) {
  _callbackAPI = value;
}

void BaseApp::handleSetCallbackToken(const std::string &name, const std::string &value) {
  _callbackToken = value;
}

void BaseApp::handleSetSaveOutput(const std::string &name, const std::string &value) {
  _saveOutput = value;
}

void BaseApp::handleSetRunAfter(const std::string &name, const std::string &value) {
  _runAfter = value;
}

int BaseApp::main(const std::vector<std::string> &args) {
  // check the arguments.
  if (_helpRequested) {
    return Application::EXIT_OK;
  }
  if (args.empty()) {
    fprintf(stderr, "Error: You must specify the program arguments.\n");
    exit(1);
  }
  _args = args;
  if (_workDir.empty()) {
    _workDir = Poco::Path(Poco::Path::current()).absolute().toString();
  } else {
    _workDir = Poco::Path(_workDir).absolute().toString();
  }

  // run the main application.
  try {
    runApp();
    return Application::EXIT_OK;
  } catch (Poco::Exception const& exc) {
    Logger::getLogger().error(exc.displayText());
    return Poco::Util::Application::EXIT_SOFTWARE;
  } catch (std::exception const& exc) {
    Logger::getLogger().error(exc.what());
    return Poco::Util::Application::EXIT_SOFTWARE;
  }
}
