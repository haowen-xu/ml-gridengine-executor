//
// Created by 许昊文 on 2018/11/20.
//

#include <Poco/Delegate.h>
#include <Poco/DirectoryWatcher.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/FileStream.h>
#include <Poco/Path.h>
#include "GeneratedFilesWatcher.h"
#include "Logger.h"

GeneratedFilesWatcher::GeneratedFilesWatcher(std::string const &workDir, FileWatcherHandler const &handler) :
  _workDir(workDir),
  _handler(handler),
  _watcher(nullptr),
  _fileNamesToTags({
     {"config.json", "config"},
     {"config.defaults.json", "defConfig"},
     {"result.json", "result"},
     {"webui.json", "webUI"}
  })
{
}

GeneratedFilesWatcher::~GeneratedFilesWatcher() {
  if (_watcher != nullptr) {
    stop();
  }
}

void GeneratedFilesWatcher::start() {
  _watcher = new Poco::DirectoryWatcher(_workDir);
  _watcher->itemAdded.add(Poco::delegate(this, &GeneratedFilesWatcher::_onFileUpdated));
  _watcher->itemModified.add(Poco::delegate(this, &GeneratedFilesWatcher::_onFileUpdated));
  _watcher->itemMovedTo.add(Poco::delegate(this, &GeneratedFilesWatcher::_onFileUpdated));
  Logger::getLogger().info("Generated files watcher installed.");
}

void GeneratedFilesWatcher::stop() {
  if (_watcher != nullptr) {
    delete _watcher;
    _watcher = nullptr;
    Logger::getLogger().info("Generated files watcher uninstalled.");
  }
}

Poco::JSON::Object::Ptr GeneratedFilesWatcher::_loadJsonFile(const Poco::Path &jsonFilePath) {
  Poco::JSON::Parser parser;
  Poco::FileInputStream is(jsonFilePath.toString());
  Poco::Dynamic::Var result = parser.parse(is);
  return result.extract<Poco::JSON::Object::Ptr>();
}

void GeneratedFilesWatcher::_onFileUpdated(Poco::DirectoryWatcher::DirectoryEvent const &e) {
  Poco::Path path(e.item.path());
  const std::string &fileName = path.getFileName();
  if (_fileNamesToTags.count(fileName) > 0) {
    _processFile(path, _fileNamesToTags[fileName]);
  }
}

void GeneratedFilesWatcher::_processFile(Poco::Path const &filePath, std::string const& fileTag) {
  try {
    Poco::JSON::Object::Ptr jsonObject = _loadJsonFile(filePath);
    _handler(fileTag, *jsonObject);
  } catch (Poco::Exception const& exc) {
    Logger::getLogger().error("Failed to process generated file %s: %s", filePath.toString(), exc.displayText());
  } catch (std::exception const& exc) {
    Logger::getLogger().error("Failed to process generated file %s: %s", filePath.toString(), std::string(exc.what()));
  }
}

void GeneratedFilesWatcher::collectAll() {
  for (auto const& it: _fileNamesToTags) {
    Poco::Path filePath = Poco::Path(_workDir).append(it.first);
    std::string fileTag = it.second;
    Poco::File file(filePath);
    if (file.exists() && file.isFile()) {
      _processFile(filePath, fileTag);
    }
  }
}
