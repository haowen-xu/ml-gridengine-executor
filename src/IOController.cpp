//
// Created by 许昊文 on 2018/11/18.
//

#include <Poco/Thread.h>
#include <Poco/Exception.h>
#include "AutoFreePtr.h"
#include "Logger.h"
#include "IOController.h"

IOController::IOController(ProgramExecutor *executor, OutputBuffer *outputBuffer, size_t bufferSize) :
  _executor(executor),
  _outputBuffer(outputBuffer),
  _bufferSize(bufferSize),
  _ioThread(new Poco::Thread()),
  _running(false)
{
}

IOController::~IOController() {
  if (_running) {
    join();
  }
  delete _ioThread;
}

void IOController::_run() {
  ssize_t nBytes;
  AutoFreePtr<void> buffer(malloc(_bufferSize));
  while ((nBytes = _executor->readOutput(buffer.ptr, _bufferSize)) > 0) {
    _outputBuffer->write(buffer.ptr, (size_t)nBytes);
  }
}

void IOController::start() {
  if (_running) {
    throw Poco::IllegalStateException("The IO thread has already started.");
  }
  _ioThread->startFunc([this] {
    this->_run();
  });
  _running = true;
  Logger::getLogger().info("IOController started.");
}

void IOController::join() {
  if (_running) {
    _ioThread->join();
  }
  _running = false;
  Logger::getLogger().info("IOController stopped.");
}
