//
// Created by 许昊文 on 2018/11/9.
//

#ifndef ML_GRIDENGINE_EXECUTOR_OUTPUTBUFFER_H
#define ML_GRIDENGINE_EXECUTOR_OUTPUTBUFFER_H

#include <stddef.h>
#include <sys/types.h>

typedef unsigned char Byte;

namespace Poco {
  class Mutex;
}

/** Result of a read request. */
struct ReadResult {
  /** Whether or not the buffer has closed. */
  bool isClosed;
  /** Whether or not the read request is timeout. */
  bool isTimeout;
  /** Actual beginning of the contents read from the buffer. */
  size_t begin;
  /** Actual count of the contents read from the buffer. */
  size_t count;

  ReadResult() : ReadResult(0, 0) {}
  ReadResult(size_t begin, size_t count) : isClosed(false), isTimeout(false), begin(begin), count(count) {}

  static ReadResult Closed() {
    ReadResult r;
    r.isClosed = true;
    return r;
  }

  static ReadResult Timeout() {
    ReadResult r;
    r.isTimeout = true;
    return r;
  }
};

/**
 * Class for buffering program outputs in the memory.
 *
 * Other components in the executor (e.g., client readers) cannot manipulate
 * the internal buffer directly.  Instead, they should send reading request
 * to this class, and waiting for the result to come.
 */
class OutputBuffer {
private:
  Byte* _buffer;        // the circular buffer
  size_t _capacity;     // current circular buffer capacity
  size_t _maxCapacity;  // maximum circular buffer capacity
  size_t _size;         // number of bytes currently stored in the circular buffer
  size_t _head;         // position of the first byte in the circular buffer
  size_t _writtenBytes; // number of bytes ever written into the circular buffer

  Poco::Mutex *_mutex;  // Lock of this object.
  bool _closed;         // whether or not the output buffer has been closed

  // waiting list for the readers
  class ReaderList;
  ReaderList *_readerList;

  /** Expand the buffer capacity, keeping all contents. */
  void _expandBuffer(size_t desiredCapacity);

  /**
   * Write a number of bytes into the circular buffer.
   *
   * This method does not overwrite existing bytes in the buffer.  It will not increase the buffer
   * capacity.  If there's no room for writing new contents in the buffer, it will just write part
   * of the data into the buffer, discarding the remaining.
   *
   * @return The number of bytes successfully written.
   */
  size_t _circularWrite(const Byte *data, size_t count);

  /**
   * Write a number of bytes into the output buffer, overwriting existing content if required.
   *
   * This method will attempt to increase the buffer capacity, if there's no room to write all the
   * contents, and `_capacity < _maxCapacity`.
   *
   * @return Number of bytes having been overwritten.
   */
  size_t _overwrite(const Byte *data, size_t count);

  /**
   * Read a number of bytes from the circular buffer.
   *
   * @param target The target array.
   * @param count Length of contents to read.
   * @param start Instead of starting from the head (position 0), starting from this position.
   *              If {@code start >= _size}, it will cause zero bytes to be read.
   * @return Actual number of bytes read.
   */
  size_t _circularRead(Byte* target, size_t count, size_t start);

  /** Translate a negatively indexed "begin" to its current positive index. */
  inline size_t translateNegativeBegin(ssize_t begin) const {
    if (begin < 0) {
      begin += _writtenBytes;
    }
    if (begin < 0) {
      begin = 0;
    }
    return (size_t)begin;
  }

  ReadResult _tryRead(size_t begin, void *target, size_t count);

public:
  inline size_t size() const { return _size; }
  /** Maximum number of bytes which can be stored in this buffer. */
  inline size_t capacity() const { return _maxCapacity; }
  inline size_t writtenBytes() const { return _writtenBytes; }

  /**
   * Construct a new {@class OutputBuffer}.
   *
   * @param maxCapacity The maximum capacity of this output buffer.
   * @param initialCapacity The initial capacity of this output buffer.
   */
  explicit OutputBuffer(size_t maxCapacity, size_t initialCapacity=64 * 1024);

  /** Destroy the {@class OutputBuffer}. */
  ~OutputBuffer();

  /**
   * Write a number of bytes into the output buffer.
   *
   * This method will overwrite existing contents in the output buffer, if the room is
   * not sufficient to hold all contents.  This method may block.
   *
   * @param data Byte data array.
   * @param count Number of bytes to write.
   *
   * @throws Poco::RuntimeError If the buffer has closed.
   */
  void write(const void* data, size_t count);

  /**
   * Read at most {@arg count} number of bytes from the output buffer.
   *
   * @param begin Beginning position of the output to read, counted from the very beginning when the program started.
   *              If negative, it will be first added by {@code writtenBytes()}, then used as the position.
   * @param target Target array, where to put the contents.
   * @param count Maximum number of bytes to read.
   * @param timeout Number of milliseconds to wait before timeout.  Specify <= 0 to wait forever.
   *
   * @return The result of the read request.
   */
  ReadResult read(ssize_t begin, void *target, size_t count, long timeout = 0);

  /**
   * Try read at most {@arg count} number of bytes from the output buffer.
   *
   * @param begin Beginning position of the output to read, counted from the very beginning when the program started.
   *              If negative, it will be first added by {@code writtenBytes()}, then used as the position.
   * @param target Target array, where to put the contents.
   * @param count Maximum number of bytes to read.
   *
   * @return The result of the read request.  If there's no more content in the buffer currently,
   *         returns {@code ReadResult::Timeout()}.
   */
  ReadResult tryRead(ssize_t begin, void* target, size_t count);

  /**
   * Close the output buffer.
   *
   * All preceding read and write requests will terminate immediately.
   */
  void close();
};


#endif //ML_GRIDENGINE_EXECUTOR_OUTPUTBUFFER_H
