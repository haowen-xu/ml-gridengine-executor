//
// Created by 许昊文 on 2018/11/18.
//

#ifndef ML_GRIDENGINE_EXECUTOR_IOCONTROLLER_H
#define ML_GRIDENGINE_EXECUTOR_IOCONTROLLER_H

#include "ProgramExecutor.h"
#include "OutputBuffer.h"

namespace Poco {
  class Thread;
}

class IOController {
private:
  ProgramExecutor *_executor;
  OutputBuffer *_outputBuffer;
  size_t _bufferSize;
  Poco::Thread *_ioThread;
  volatile bool _running;

  void _run();

public:
  explicit IOController(ProgramExecutor *executor, OutputBuffer *outputBuffer, size_t bufferSize=8192);

  ~IOController();

  void start();

  void join();
};


#endif //ML_GRIDENGINE_EXECUTOR_IOCONTROLLER_H
