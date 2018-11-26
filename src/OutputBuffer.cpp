//
// Created by 许昊文 on 2018/11/9.
//

#include <memory>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <queue>
#include <boost/heap/pairing_heap.hpp>
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
      cond(), result(), begin(begin), target(target), count(count), cancelled(false) {}

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

  template <typename T>
  struct AutoDelete {
    T* ptr;
    AutoDelete(T* ptr) : ptr(ptr) {}
    ~AutoDelete() { delete ptr; }
    void swap(T** dst) {
      T* tmp = *dst;
      *dst = ptr;
      ptr = tmp;
    }
  };
}

class OutputBuffer::ReaderList {
  typedef boost::heap::pairing_heap<ReadRequestPtr, boost::heap::compare<ReaderOrdering>> HeapType;

private:
  HeapType *_heap;
  size_t _heapSize;
  size_t _activeCount;

public:
  ReaderList() : _heap(new HeapType), _heapSize(0), _activeCount(0) {}

  ~ReaderList() { delete _heap; }

  void add(ReadRequestPtr const& ptr) {
    _heap->push(ptr);
    ++_heapSize;
    ++_activeCount;
  }

  void cancel(ReadRequestPtr const& ptr) {
    --_activeCount;
    ptr->cancel();
    if (_activeCount * 8 < _heapSize && _heapSize > 1000) {
      AutoDelete<HeapType> newHeap(new HeapType);
      for (auto const &itm: *_heap) {
        if (!itm->cancelled) {
          newHeap.ptr->push(itm);
        }
      }
      newHeap.swap(&_heap);
      size_t oldHeapSize = _heapSize;
      _heapSize = _activeCount;
      Logger::getLogger().info(
          "Waiting queue for output buffer readers has been re-allocated (%z -> %z).", oldHeapSize, _heapSize);
    }
  }

  void close() {
    while (!_heap->empty()) {
      ReadRequestPtr const& itm = _heap->top();
      itm->result = ReadResult::Closed();
      itm->cond.signal();
      _heap->pop();
    }
    _heapSize = _activeCount = 0;
  }

  template <typename Function>
  void process(size_t writtenBytes, Function const& f) {
    while (!_heap->empty() && _heap->top()->begin < writtenBytes) {
      ReadRequestPtr const& reader = _heap->top();
      if (!reader->cancelled) {
        f(reader);
        reader->cond.signal();
        --_activeCount;
      }
      _heap->pop();
      --_heapSize;
    }
  }
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

void OutputBuffer::_expandBuffer(size_t desiredCapacity) {
  size_t newCapacity = std::min(std::max(_capacity << 1, desiredCapacity), _maxCapacity);
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
  size_t desiredCapacity = _size + count;
  if (desiredCapacity > _capacity && _capacity < _maxCapacity) {
    _expandBuffer(desiredCapacity);
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
  _readerList->process(_writtenBytes, [&] (ReadRequestPtr const& reader) {
    assert(reader->begin >= oldWrittenBytes);
    size_t itmCount = std::min(reader->count, _writtenBytes - reader->begin);
    reader->result = ReadResult(reader->begin, itmCount);
    memcpy(reader->target, (Byte*)data + (reader->begin - oldWrittenBytes), itmCount);
  });
}

ReadResult OutputBuffer::_tryRead(size_t begin, void *target, size_t count) {
  if (begin < _writtenBytes) {
    // if begin < writtenBytes, return the available bytes immediately
    size_t minBegin = _writtenBytes - _size;
    size_t localStart = (begin <= minBegin) ? 0 : begin - minBegin;
    return ReadResult(_writtenBytes - _size + localStart, _circularRead((Byte*)target, count, localStart));
  } else if (_closed) {
    // if begin >= writtenBytes but the buffer has closed, return Closed result immediately
    return ReadResult::Closed();
  } else {
    // cannot get the result currently
    return ReadResult::Timeout();
  }
}

ReadResult OutputBuffer::read(ssize_t begin, void *target, size_t count, long timeout) {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);
  size_t positiveBegin = translateNegativeBegin(begin);
  ReadResult result = _tryRead(positiveBegin, target, count);

  if (!result.isTimeout) {
    return result;
  } else {
    std::shared_ptr<ReadRequest> reader(new ReadRequest(positiveBegin, (Byte *) target, count));
    _readerList->add(reader);
    if (timeout <= 0) {
      reader->cond.wait(*_mutex);
    } else {
      try {
        reader->cond.wait(*_mutex, timeout);
      } catch (Poco::TimeoutException const&) {
        _readerList->cancel(reader);
        return ReadResult::Timeout();
      } catch (...) {
        _readerList->cancel(reader);
        throw;
      }
    }
    return reader->result;
  }
}

ReadResult OutputBuffer::tryRead(ssize_t begin, void *target, size_t count) {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);
  return _tryRead(translateNegativeBegin(begin), target, count);
}

void OutputBuffer::close() {
  Poco::Mutex::ScopedLock scopedLock(*_mutex);

  if (!_closed) {
    _readerList->close();
    _closed = true;
  }
}
