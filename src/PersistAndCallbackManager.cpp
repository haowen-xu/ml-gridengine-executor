//
// Created by 许昊文 on 2018/11/21.
//

#include <Poco/URI.h>
#include <Poco/Base64Encoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/JSON/Object.h>
#include <Poco/FileStream.h>
#include "HTTPError.h"
#include "Logger.h"
#include "PersistAndCallbackManager.h"

using namespace Poco::Net;

namespace {
  std::string encodeToken(std::string const& token) {
    std::ostringstream ostream;
    Poco::Base64Encoder encoder(ostream);
    encoder << token;
    encoder.flush();
    ostream.flush();
    return ostream.str();
  }

  std::string jsonToString(Poco::JSON::Object const& doc) {
    std::stringstream oss;
    doc.stringify(oss);
    return oss.str();
  }

  void saveFile(std::string const& path, std::string const& doc) {
    Poco::FileOutputStream outStream(path, std::ios::out | std::ios::trunc);
    outStream.write(doc.c_str(), doc.length());
  }

  void post(Poco::URI const& uri, std::string const& token, long timeout, std::string const& doc) {
    std::string path(uri.getPathAndQuery());
    if (path.empty()) {
      path = "/";
    }

    // Initialize the HTTP connection
    HTTPClientSession session(uri.getHost(), uri.getPort());
    session.setTimeout(Poco::Timespan((Poco::Int64)timeout * 1000));

    // Prepare for the request
    HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    request.setMethod("POST");
    request.setContentType("application/json");
    request.setContentLength(doc.length());
    if (!token.empty()) {
      request.add("Authentication", Poco::format("TOKEN %s", encodeToken(token)));
    }

    // Send the request
    auto &os = session.sendRequest(request);
    os.write(doc.c_str(), doc.length());

    // Now get the response
    HTTPResponse response;
    auto &is = session.receiveResponse(response);
    {
      std::ostringstream oss;
      Poco::StreamCopier::copyStream(is, oss);
      std::string responseLogLevel;
      if (response.getStatus() != 200) {
        std::string const& msg = Poco::format("callback API error: %d %s", (int)response.getStatus(), response.getReason());
        throw HTTPError(response.getStatus(), msg);
      } else {
        Logger::getLogger().info("Callback API response: %s", Poco::trim(oss.str()));
      }
    }
  }
}

PersistAndCallbackManager::PersistAndCallbackManager(std::string const &statusFile, std::string const &uri,
                                                     std::string const &token, long timeout) :
  _statusFile(statusFile),
  _uri(uri),
  _token(token),
  _timeout(timeout) {
}

PersistAndCallbackManager::~PersistAndCallbackManager() {
}

void PersistAndCallbackManager::_postEvent(std::string const& eventType, Poco::JSON::Object const &doc) {
  if (!_uri.empty()) {
    Poco::JSON::Object payload;
    payload.set("eventType", eventType);
    payload.set("data", doc);
    post(Poco::URI(_uri), _token, _timeout, jsonToString(payload));
  }
}

void PersistAndCallbackManager::programStarted(std::string const &hostName, int port)
{
  Logger::getLogger().info("statusUpdated: RUNNING");
  _hostName = hostName;
  _port = port;

  // assemble the document
  Poco::JSON::Object doc;
  doc.set("executor.hostname", _hostName);
  doc.set("executor.port", _port);
  doc.set("status", "RUNNING");

  // save to file if configured.
  if (!_statusFile.empty()) {
    saveFile(_statusFile, jsonToString(doc));
  }

  // post to callback API if configured
  if (!_uri.empty()) {
    _postEvent("statusUpdated", doc);
  }
}

void PersistAndCallbackManager::fileGenerated(std::string const& fileTag, Poco::JSON::Object const& jsonObject) {
  if (!_uri.empty()) {
    std::string serializedDoc = jsonToString(jsonObject);
    if (!_lastPostedGeneratedFiles.count(fileTag) || _lastPostedGeneratedFiles[fileTag] != serializedDoc) {
      Logger::getLogger().info("fileGenerated:%s: %s", fileTag, serializedDoc);
      _postEvent(Poco::format("fileGenerated:%s", fileTag), jsonObject);
      _lastPostedGeneratedFiles[fileTag] = serializedDoc;
    }
  }
}

void PersistAndCallbackManager::programFinished(ProgramExecutor const& executor) {
  // assemble the document
  std::string programStatus;
  Poco::JSON::Object doc;
  doc.set("executor.hostname", _hostName);
  doc.set("executor.port", _port);
  switch (executor.status()) {
    case EXITED:
      programStatus = "EXITED";
      doc.set("status", programStatus);
      doc.set("exitCode", executor.exitCode());
      break;
    case SIGNALLED:
      programStatus = "SIGNALLED";
      doc.set("status", programStatus);
      doc.set("exitCode", executor.exitSignal());
      break;
    case CANNOT_KILL:
      programStatus = "CANNOT_KILL";
      doc.set("status", programStatus);
      break;
    default:
      Logger::getLogger().warn("Invalid executor status after it is completed.");
      break;
  }
  Logger::getLogger().info("statusUpdated: %s", programStatus);

  // save to file if configured.
  if (!_statusFile.empty()) {
    saveFile(_statusFile, jsonToString(doc));
  }

  // post to callback API if configured
  if (!_uri.empty()) {
    _postEvent("statusUpdated", doc);
  }
}

void PersistAndCallbackManager::wait() {
}

void PersistAndCallbackManager::interrupt() {
}
