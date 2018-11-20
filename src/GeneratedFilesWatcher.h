//
// Created by 许昊文 on 2018/11/20.
//

#ifndef ML_GRIDENGINE_EXECUTOR_GENERATEDFILESWATCHER_H
#define ML_GRIDENGINE_EXECUTOR_GENERATEDFILESWATCHER_H

#include <string>
#include <functional>
#include <Poco/Path.h>
#include <Poco/JSON/Object.h>
#include <Poco/DirectoryWatcher.h>

/** Handler of generated files, void(tag, jsonDoc). */
typedef std::function<void(std::string const&, Poco::JSON::Object const&)> FileWatcherHandler;

class GeneratedFilesWatcher {
private:
  std::string _workDir;
  FileWatcherHandler _handler;
  Poco::DirectoryWatcher *_watcher;

  Poco::JSON::Object::Ptr _loadJsonFile(Poco::Path const& jsonFilePath);

  void _onFileUpdated(Poco::DirectoryWatcher::DirectoryEvent const &e);

  void _processFile(Poco::Path const& filePath);

public:
  explicit GeneratedFilesWatcher(std::string const& workDir, FileWatcherHandler const& handler);

  void collectAll();

  void start();

  void stop();

  ~GeneratedFilesWatcher();
};


#endif //ML_GRIDENGINE_EXECUTOR_GENERATEDFILESWATCHER_H
