//
// Created by 许昊文 on 2018/11/14.
//

#ifndef ML_GRIDENGINE_EXECUTOR_WEBSERVER_H
#define ML_GRIDENGINE_EXECUTOR_WEBSERVER_H

#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include "ProgramExecutor.h"
#include "OutputBuffer.h"


class WebServerFactory : public Poco::Net::HTTPRequestHandlerFactory {
private:
  ProgramExecutor *_executor;
  OutputBuffer *_outputBuffer;
  size_t _requestBufferSize;

public:
  explicit WebServerFactory(ProgramExecutor *executor, OutputBuffer *outputBuffer, size_t requestBufferSize=65536);

  virtual Poco::Net::HTTPRequestHandler* createRequestHandler(Poco::Net::HTTPServerRequest const& request);

  ProgramExecutor *executor() const { return _executor; }
  OutputBuffer *outputBuffer() const { return _outputBuffer; }
  size_t requestBufferSize() const { return _requestBufferSize; }
};


#endif //ML_GRIDENGINE_EXECUTOR_WEBSERVER_H
