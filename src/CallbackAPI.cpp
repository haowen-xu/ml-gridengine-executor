//
// Created by 许昊文 on 2018/11/19.
//

#include <sstream>
#include <Poco/URI.h>
#include <Poco/Base64Encoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/JSON/Object.h>
#include "CallbackAPI.h"
#include "Logger.h"

using namespace Poco::Net;

POCO_IMPLEMENT_EXCEPTION(HTTPError, Poco::ApplicationException, "HTTP Error");

namespace {
  std::string encodeToken(std::string const& token) {
    std::ostringstream ostream;
    Poco::Base64Encoder encoder(ostream);
    encoder << token;
    encoder.flush();
    ostream.flush();
    return ostream.str();
  }
}


CallbackAPI::CallbackAPI(std::string const &uri, std::string const& token, long timeout) :
  _uri(uri),
  _token(token),
  _timeout(timeout)
{
}

CallbackAPI::~CallbackAPI() {
}

void CallbackAPI::post(Poco::JSON::Object const &doc) {
  std::stringstream oss;
  doc.stringify(oss);
  post(oss.str());
}

void CallbackAPI::post(std::string const &doc) {
  // Parse the arguments.
  if (_uri.empty()) {
    throw Poco::IllegalStateException("uri is not configured.");
  }

  Poco::URI uri(_uri);
  std::string path(uri.getPathAndQuery());
  if (path.empty()) {
    path = "/";
  }
  std::string token(_token);
  long timeout(_timeout);

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
      throw HTTPError(Poco::format("callback API error: %d %s", (int)response.getStatus(), response.getReason()));
    } else {
      Logger::getLogger().info("Callback API response: %s", Poco::trim(oss.str()));
    }
  }
}
