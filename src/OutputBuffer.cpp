//
// Created by 许昊文 on 2018/11/9.
//

#include <memory>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <queue>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include "AutoFreePtr.h"
#include "OutputBuffer.h"
#include "Logger.h"


namespace {
  struct ReadRequest {
    Poco::Condition cond;
    ReadResult result;
    Byte* target;
    size_t count;
    size_t begin;
    bool cancelled;

    ReadRequest(size_t begin, Byte* target, size_t count):
      cond(), result(), begin(begin), target(target), count(count), cancelled(false)
    {
    }

    void cancel() {
      cancelled = true;
    }
  };

  typedef std::shared_ptr<ReadRequest> ReadRequestPtr;

  struct ReaderOrdering {
    bool operator ()(ReadRequestPtr const& left, ReadRequestPtr const& right) const {
      return left->begin > right->begin;
    }
  };
}

class OutputBuffer::ReaderList :
  public std::priority_queue<ReadRequestPtr, std::vector<ReadRequestPtr>, ReaderOrdering> {
};

OutputBuffer::OutputBuffer(size_t maxCapacity, size_t initialCapacity) :
  _buffer((Byte*)malloc(maxCapacity)),
  _capacity(std::min(initialCapacity, maxCapacity)),
  _maxCapacity(maxCapacity),
  _size(0),
  _head(0),
  _writtenBytes(0),
  _mutex(new Poco::Mutex()),
  _closed(false),
  _readerList(new ReaderList())
{
}

OutputBuffer::~OutputBuffer() {
  if (!_closed) {
    close();
  }

  delete _readerList;
  delete _mutex;
  free(_buffer);
  _readerList = nullptr;
  _mutex = nullptr;
  _buffer = nullptr;
}

void OutputBuffer::_expandBuffer() {
  size_t newCapacity = std::min(_capacity << 1, _maxCapacity);
  if (newCapacity > _capacity) {
    AutoFreePtr<Byte> autoFree((Byte*)malloc(newCapacity));
    if (_head + _size > _capacity) {
      size_t rightSize = _capacity - _head;
      std::memcpy(autoFree.ptr, _buffer + _head, rightSize);
      std::memcpy(autoFree.ptr + rightSize, _buffer, _size - rightSize);
    } else {
      std::memcpy(autoFree.ptr, _buffer + _head, _size);
    }
    autoFree.swap(&_buffer);
    _capacity = newCapacity;
    _head = 0;
  }
}

size_t OutputBuffer::_circularWrite(const Byte *data, size_t count) {
  size_t room = _capacity - _size;
  size_t bytesToWrite = std::min(count, room);
  size_t tail = (_head + _size) % _capacity;
  size_t rightSize = _capacity - tail;

  if (bytesToWrite <= rightSize) {
    std::memcpy(_buffer + tail, data, bytesToWrite);
  } else {
    std::memcpy(_buffer + tail, data, rightSize);
    std::memcpy(_buffer, data + rightSize, bytesToWrite - rightSize);
  }

  _size += bytesToWrite;
  return bytesToWrite;
}

size_t OutputBuffer::_overwrite(const Byte *data, size_t count) {
  if (_size + count > _capacity && _capacity < _maxCapacity) {
    _expandBuffer();
  }

  size_t overwrittenLen = 0;
  if (count >= _capacity) {
    // When more bytes are to be written than the capacity, we just write the
    // last `_capacity` bytes into the buffer, starting from the head.
    overwrittenLen = _size + (count - _capacity);
    _size = 0;
    _circularWrite(data + count - _capacity, _capacity);
  } else {
    // There are fewer bytes than the capacity to be written, so some of
    // the bytes already in this buffer will be preserved.  We should calculate
    // how many bytes should be preserved, and how many should be overwritten.
    size_t preserveLen = _capacity - count;
    if (preserveLen < _size) {
      overwrittenLen = _size - preserveLen;
      _size -= overwrittenLen;
      _head = (_head + overwrittenLen) % _capacity;
    }
    _circularWrite(data, count);
  }

  _writtenBytes += count;
  return overwrittenLen;
}

size_t OutputBuffer::_circularRead(Byte *target, size_t count, size_t start) {
  start = std::max(start, (size_t)0);
  size_t readSize;

  if (_size > start) {
    readSize = std::min(count, _size - start);
    size_t front = (_head + start) % _capacity;
    size_t rightSize = _capacity - front;

    if (readSize <= rightSize) {
      std::memcpy(target, _buffer + front, readSize);
    } else {
      std::memcpy(target, _buffer + front, rightSize);
      std::memcpy(target + rightSize, _buffer, readSize - rightSize);
    }
  } else {
    readSize = 0;
  }

  return readSize;
}

void OutputBuffer::write(const void *data, size_t count) {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);

  if (_closed) {
    throw Poco::IllegalStateException("The buffer has been closed.");
  }

  // write data into the buffer, potentially overwriting the existing contents
  size_t oldWrittenBytes = _writtenBytes;
  _overwrite((Byte*)data, count);

  // wake up all waiting readers
  while (!_readerList->empty() && _readerList->top()->begin < _writtenBytes) {
    ReadRequestPtr const& reader = _readerList->top();
    if (!reader->cancelled) {
      assert(reader->begin >= oldWrittenBytes);
      size_t itmCount = std::min(count, _writtenBytes - reader->begin);
      reader->result = ReadResult(reader->begin, itmCount);
      memcpy(reader->target, (Byte*)data + (reader->begin - oldWrittenBytes), itmCount);
      reader->cond.signal();
    }
    _readerList->pop();
  }
}

ReadResult OutputBuffer::read(size_t begin, void *target, size_t count, long timeout) {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);
  ReadResult result;

  if (_tryRead(begin, target, count, &result)) {
    return result;
  } else {
    std::shared_ptr<ReadRequest> reader(new ReadRequest(begin, (Byte *) target, count));
    _readerList->push(reader);
    if (timeout <= 0) {
      reader->cond.wait(*_mutex);
    } else {
      try {
        reader->cond.wait(*_mutex, timeout);
      } catch (...) {
        reader->cancel();
        throw;
      }
    }
    return reader->result;
  }
}

bool OutputBuffer::_tryRead(size_t begin, void *target, size_t count, ReadResult *result) {
  if (begin < _writtenBytes) {
    // if begin < writtenBytes, return the available bytes immediately
    size_t minBegin = _writtenBytes - _size;
    size_t localStart = (begin <= minBegin) ? 0 : begin - minBegin;
    *result = ReadResult(_writtenBytes - _size + localStart, _circularRead((Byte*)target, count, localStart));
    return true;
  } else if (_closed) {
    // if begin >= writtenBytes but the buffer has closed, return Closed result immediately
    *result = ReadResult::Closed();
    return true;
  } else {
    // cannot get the result
    return false;
  }
}

bool OutputBuffer::tryRead(size_t begin, void *target, size_t count, ReadResult *result) {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);
  return _tryRead(begin, target, count, result);
}

void OutputBuffer::close() {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);

  if (!_closed) {
    while (!_readerList->empty()) {
      ReadRequestPtr const& itm = _readerList->top();
      if (!itm->cancelled) {
        itm->result = ReadResult::Closed();
        itm->cond.signal();
      }
      _readerList->pop();
    }
    _closed = true;
  }
}
