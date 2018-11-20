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
  _watcher(nullptr)
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
  Logger::getLogger().info("Generated files watcher installed: %s", _workDir);
}

void GeneratedFilesWatcher::stop() {
  if (_watcher != nullptr) {
    delete _watcher;
    _watcher = nullptr;
    Logger::getLogger().info("Generated files watcher uninstalled: %s", _workDir);
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
  _processFile(path);
}

void GeneratedFilesWatcher::_processFile(Poco::Path const &filePath) {
  try {
    std::string fileTag;

    if (filePath.getFileName() == "config.json") {
      fileTag = "config";
    } else if (filePath.getFileName() == "config.defaults.json") {
      fileTag = "defConfig";
    } else if (filePath.getFileName() == "result.json") {
      fileTag = "result";
    }

    if (!fileTag.empty()) {
      Poco::JSON::Object::Ptr jsonObject = _loadJsonFile(filePath);
      _handler(fileTag, *jsonObject);
    }
  } catch (Poco::Exception const& exc) {
    Logger::getLogger().error("Failed to process generated file %s: %s", filePath.toString(), exc.displayText());
  } catch (std::exception const& exc) {
    Logger::getLogger().error("Failed to process generated file %s: %s", filePath.toString(), std::string(exc.what()));
  }
}

namespace {
  const char* fileNames[] = {
      "config.json",
      "config.defaults.json",
      "result.json",
      nullptr
  };
}

void GeneratedFilesWatcher::collectAll() {
  for (size_t i=0; fileNames[i]; ++i) {
    const char* fileName = fileNames[i];
    Poco::Path filePath = Poco::Path(_workDir).append(fileName);
    Poco::File file(filePath);
    if (file.exists() && file.isFile()) {
      _processFile(filePath);
    }
  }
}
