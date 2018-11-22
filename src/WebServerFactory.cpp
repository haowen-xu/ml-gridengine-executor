//
// Created by 许昊文 on 2018/11/14.
//

#include <Poco/URI.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/NumberParser.h>
#include "AutoFreePtr.h"
#include "WebServerFactory.h"
#include "Logger.h"

using namespace Poco::Net;

#define HANDLER_CONSTRUCTOR(CLASS_NAME)                                             \
  protected:                                                                        \
    Poco::URI _uri;                                                                 \
    ProgramExecutor *_executor;                                                     \
    OutputBuffer *_outputBuffer;                                                    \
    size_t _requestBufferSize;                                                      \
  public:                                                                           \
    explicit CLASS_NAME(Poco::URI uri, WebServerFactory *factory) :                 \
      _uri(uri),                                                                    \
      _executor(factory->executor()),                                               \
      _outputBuffer(factory->outputBuffer()),                                       \
      _requestBufferSize(factory->requestBufferSize())

namespace {
  class NotFoundHandler : public HTTPRequestHandler {
  public:
    virtual void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) {
      response.setStatus(HTTPResponse::HTTPStatus::HTTP_NOT_FOUND);
      auto &r = response.send();
      r << "<h1>Not Found</h1>";
    }
  };

  class OutputStreamHandler : public HTTPRequestHandler {
    HANDLER_CONSTRUCTOR(OutputStreamHandler) {}
  private:

  public:
    virtual void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) {
      size_t begin = 0;
      for (auto const& it: _uri.getQueryParameters()) {
        if (it.first == "begin") {
          Poco::UInt64 beginValue;
          if (!Poco::NumberParser::tryParseUnsigned64(it.second, beginValue)) {
            response.setStatus(HTTPResponse::HTTPStatus::HTTP_BAD_REQUEST);
            response.send() << "<h1>Bad Request</h1>" << std::endl;
            return;
          }
          begin = beginValue;
        }
      }

      AutoFreePtr<char> buffer((char*)malloc(_requestBufferSize));
      ReadResult result;

      // Read the first piece of output, to determine the response status.
      result = _outputBuffer->read(begin, buffer.ptr, _requestBufferSize);
      if (result.isClosed) {
        response.setStatus(HTTPResponse::HTTPStatus::HTTP_GONE);
        response.send() << "<h1>Program exited.</h1>" << std::endl;
        return;
      }

      // There is content to send back to client, send the begin mark and the first piece of output now.
      response.setStatus(HTTPResponse::HTTPStatus::HTTP_OK);
      response.setChunkedTransferEncoding(true);
      auto &r = response.send();

      std::string beginStr = Poco::format("%?x\n", result.begin);
      r.write(beginStr.c_str(), beginStr.length());
      r.write(buffer.ptr, result.count);
      r.flush();
      begin = result.begin + result.count;

      // Now send the following content
      while (r.good()) {
        result = _outputBuffer->read(begin, buffer.ptr, _requestBufferSize);

        if (result.isClosed) {
          break;

        } else if (result.begin != begin) {
          break;

        } else {
          r.write(buffer.ptr, result.count);
          r.flush();
        }
        begin = result.begin + result.count;
      }
    }
  };

  class KillHandler : public HTTPRequestHandler {
    HANDLER_CONSTRUCTOR(KillHandler) {}
  public:
    virtual void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) {
      _executor->kill();

      if (_executor->status() != EXITED && _executor->status() != SIGNALLED && _executor->status() != CANNOT_KILL) {
        response.setStatus(HTTPResponse::HTTPStatus::HTTP_INTERNAL_SERVER_ERROR);
      } else {
        response.setStatus(HTTPResponse::HTTPStatus::HTTP_OK);
        response.setContentType("text/json");

        if (_executor->status() == EXITED) {
          response.send() << "{\"status\": \"exited\", \"exitCode\": " << _executor->exitCode() << "}";
        } else if (_executor->status() == SIGNALLED) {
          response.send() << "{\"status\": \"signalled\", \"exitSignal\": " << _executor->exitCode() << "}";
        } else {
          response.send() << "{\"status\": \"cannot_kill\"}";
        }
      }
    }
  };
}

WebServerFactory::WebServerFactory(ProgramExecutor *executor, OutputBuffer *outputBuffer, size_t requestBufferSize) :
    _executor(executor),
    _outputBuffer(outputBuffer),
    _requestBufferSize(requestBufferSize)
{

}

HTTPRequestHandler *WebServerFactory::createRequestHandler(HTTPServerRequest const &request) {
  Poco::URI uri(request.getURI());
  if (uri.getPath() == "/output/_stream") {
    return new OutputStreamHandler(uri, this);
  } else if (uri.getPath() == "/_kill") {
    return new KillHandler(uri, this);
  } else {
    return new NotFoundHandler();
  }
}