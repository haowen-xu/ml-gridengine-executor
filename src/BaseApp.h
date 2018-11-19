//
// Created by 许昊文 on 2018/11/17.
//

#ifndef ML_GRIDENGINE_EXECUTOR_BASEAPP_H
#define ML_GRIDENGINE_EXECUTOR_BASEAPP_H

#include <string>
#include <Poco/Util/Application.h>
#include "ProgramExecutor.h"

class BaseApp : public Poco::Util::Application {
protected:
  bool _helpRequested = false;
  ArgList _args;
  Path _workDir;
  EnvironMap _environ;
  std::string _serverHost;
  Poco::UInt16 _serverPort = 0;
  size_t _bufferSize = 4 * 1024UL * 1024UL;
  std::string _callbackAPI;
  std::string _saveOutput;

  void displayHelp(std::ostream& out);

  void displayHelp();

  void defineOptions(Poco::Util::OptionSet &options);

  void handleHelp(const std::string &name, const std::string &value);

  void handleSetWorkDir(const std::string &name, const std::string &value);

  void handleSetEnviron(const std::string &name, const std::string &value);

  void handleSetServerHost(const std::string &name, const std::string &value);

  void handleSetServerPort(const std::string &name, const std::string &value);

  void handleSetBufferSize(const std::string &name, const std::string &value);

  void handleSetCallbackAPI(const std::string &name, const std::string &value);

  void handleSetSaveOutput(const std::string &name, const std::string &value);

  virtual int runApp() = 0;

  int main(const std::vector<std::string> &args);
};


#endif //ML_GRIDENGINE_EXECUTOR_BASEAPP_H
