//
// Created by 许昊文 on 2018/11/24.
//

#include <unistd.h>
#include <string.h>
#include <vector>
#include <Poco/Exception.h>
#include <Poco/Thread.h>
#include <boost/heap/pairing_heap.hpp>
#include <catch2/catch.hpp>
#include <src/Logger.h>
#include "src/OutputBuffer.h"
#include "src/Utils.h"
#include "macros.h"
#include "CapturingLogger.h"

namespace {
  std::vector<Byte> bytesRange(Byte start, size_t n) {
    std::vector<Byte> dst;
    dst.resize(n);
    for (size_t i=0; i<n; ++i) {
      dst[i] = (Byte)(start + i);
    }
    return std::move(dst);
  }

  std::vector<Byte> zeros(size_t n) {
    std::vector<Byte> dst;
    dst.resize(n);
    for (size_t i=0; i<n; ++i) {
      dst[i] = 0;
    }
    return std::move(dst);
  }

  std::vector<Byte> bytesConcat(std::vector<Byte> const& a, std::vector<Byte> const& b) {
    std::vector<Byte> dst;
    dst.resize(a.size() + b.size());
    memcpy(dst.data(), a.data(), a.size());
    memcpy(dst.data() + a.size(), b.data(), b.size());
    return std::move(dst);
  }

  bool bytesEqual(std::vector<Byte> const& a, std::vector<Byte> const& b) {
    return a.size() == b.size() &&
      strncmp((const char*)a.data(), (const char*)b.data(), a.size()) == 0;
  }

  void readAllBytes(ssize_t begin, OutputBuffer &buffer, std::vector<Byte> *dst) {
    Byte buf[256];
    dst->clear();
    for (;;) {
      ReadResult rr = buffer.tryRead(begin, buf, sizeof(buf));
      if (rr.isClosed || rr.isTimeout)
        break;
      size_t dst_size = dst->size();
      begin = rr.begin + rr.count;
      dst->resize(dst_size + rr.count);
      memcpy(dst->data() + dst_size, buf, rr.count);
    }
  }

  double randDouble() {
    return double(std::rand()) / (double(RAND_MAX) + 1);
  }
}

namespace {
  struct IntCompare {
    bool operator()(int a, int b) const {
      return a > b;
    }
  };
}

TEST_CASE("Boost heap behavior", "[OutputBuffer]") {
  typedef boost::heap::pairing_heap<int, boost::heap::compare<IntCompare>> HeapType;
  HeapType heap;
  HeapType::handle_type handles[10];

  for (int i=0; i<10; i+=2) {
    handles[i] = heap.push(i);
  }
  for (int i=9; i>0; i-=2) {
    handles[i] = heap.push(i);
  }
  REQUIRE_EQUALS(heap.top(), 0);
  heap.erase(handles[0]);
  REQUIRE_EQUALS(heap.top(), 1);
  heap.erase(handles[1]);
  REQUIRE_EQUALS(heap.top(), 2);
  heap.erase(handles[8]);
  REQUIRE_EQUALS(heap.top(), 2);
  heap.pop();
  for (int i=3; i<8; ++i) {
    REQUIRE_EQUALS(heap.top(), i);
    heap.pop();
  }
  REQUIRE_EQUALS(heap.top(), 9);
  heap.pop();
  REQUIRE(heap.empty());
}


TEST_CASE("Test write and read (no block)", "[OutputBuffer]") {
  char buf[1024] = {0};
  std::vector<Byte> content;
  OutputBuffer buffer(31, 11);

  // the buffer is empty
  REQUIRE_EQUALS(buffer.size(), 0);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 0);
  readAllBytes(0, buffer, &content);
  REQUIRE(content.empty());

  // something has written to the buffer, but the buffer is not expanded yet
  buffer.write(bytesRange(0, 10).data(), 10);
  REQUIRE_EQUALS(buffer.size(), 10);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 10);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(0, 10)));

  // the buffer should have expanded
  buffer.write(bytesRange(10, 2).data(), 2);
  REQUIRE_EQUALS(buffer.size(), 12);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 12);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(0, 12)));

  // the buffer should have been all filled up
  buffer.write(bytesRange(12, 19).data(), 19);
  REQUIRE_EQUALS(buffer.size(), 31);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 31);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(0, 31)));

  // something in the buffer should have been overwritten
  buffer.write(bytesRange(31, 9).data(), 9);
  REQUIRE_EQUALS(buffer.size(), 31);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 40);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(9, 31)));

  // all of the bytes in the buffer should now be overwritten
  buffer.write(bytesRange(40, 60).data(), 60);
  REQUIRE_EQUALS(buffer.size(), 31);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 100);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(69, 31)));

  // test read after all the bytes, will cause to wait and may timeout
  REQUIRE(buffer.read(100, buf, sizeof(buf), 1).isTimeout);

  // test read nothing
  buf[0] = 0;
  REQUIRE_EQUALS(buffer.read(5, buf, 0).count, 0);
  REQUIRE_EQUALS(buf[0], 0);

  // test read on closed buffer: will never block
  buffer.close();
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(69, 31)));
  REQUIRE(buffer.tryRead(100, buf, sizeof(buf)).isClosed);
  REQUIRE(buffer.read(100, buf, sizeof(buf), 1).isClosed);
  REQUIRE_NOTHROW([&] () {
    buffer.read(100, buf, sizeof(buf), 1);
  }());
}

TEST_CASE("Test write into buffer with content larger than double current capacity", "[OutputBuffer]") {
  std::vector<Byte> content;
  OutputBuffer buffer(31, 11);
  buffer.write(bytesRange(0, 31).data(), 31);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(0, 31)));
}

TEST_CASE("Test write into buffer with content larger than maximum capacity", "[OutputBuffer]") {
  std::vector<Byte> content;
  OutputBuffer buffer(31, 11);
  buffer.write(bytesRange(0, 100).data(), 100);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(69, 31)));
}

TEST_CASE("Test read with negative position", "[OutputBuffer]") {
  std::vector<Byte> content;
  OutputBuffer buffer(31, 11);
  buffer.write(bytesRange(0, 100).data(), 100);
  buffer.write(bytesRange(0, 31).data(), 31);

  // test read by negative positions
  for (int i=-40; i<-31; ++i) {
    readAllBytes(i, buffer, &content);
    REQUIRE(bytesEqual(content, bytesRange(0, 31)));
  }
  for (int i=-31; i<0; ++i) {
    readAllBytes(i, buffer, &content);
    REQUIRE(bytesEqual(content, bytesRange(31+i, -i)));
  }
}

TEST_CASE("Test blocking read", "[OutputBuffer]") {
  std::vector<Byte> content1, content2, content3, content4, content;
  OutputBuffer buffer(31, 11);

  // test to read on the tail with potentially larger count than capacity,
  // and potentially further than the tail.
  Poco::Thread th1, th2, th3, th4;
  bool timeout4[1];
  th1.startFunc([&] () {
    content1.resize(31);
    buffer.read(0, content1.data(), 31);
  });
  th2.startFunc([&] () {
    content2.resize(50);
    buffer.read(5, content2.data(), 50);
  });
  th3.startFunc([&] () {
    content3 = zeros(200);
    buffer.read(0, content3.data(), 200);
  });
  th4.startFunc([&] () {
    content4.resize(1);
    timeout4[0] = buffer.read(5, content4.data(), 1, 100).isTimeout;
  });
  usleep(500 * 1000); // sleep for 500 ms
  buffer.write(bytesRange(0, 100).data(), 100);
  th1.join();
  th2.join();
  th3.join();
  th4.join();

  // the read request should capture anything they want (and available)
  REQUIRE(bytesEqual(content1, bytesRange(0, 31)));
  REQUIRE(bytesEqual(content2, bytesRange(5, 50)));
  REQUIRE(bytesEqual(content3, bytesConcat(bytesRange(0, 100), zeros(100))));
  REQUIRE(timeout4[0]);
  REQUIRE_EQUALS(content4.at(0), 0);  // a timeout request should not modify the buffer

  // the buffer should now carry the last 31 bytes
  REQUIRE_EQUALS(buffer.size(), 31);
  REQUIRE_EQUALS(buffer.capacity(), 31);
  REQUIRE_EQUALS(buffer.writtenBytes(), 100);
  readAllBytes(0, buffer, &content);
  REQUIRE(bytesEqual(content, bytesRange(69, 31)));
}

TEST_CASE("Random test on read and write", "[OutputBuffer]") {
  struct ThreadConfig {
    long timeout;
    ssize_t begin;
    size_t count;
    char* buffer;
  };
  static const int N_THREADS = 1000;
  static const int BYTES_TO_WRITE = 256;

  OutputBuffer buffer(BYTES_TO_WRITE, 1);
  std::vector<Byte> payload = bytesRange(0, BYTES_TO_WRITE);

  // spawn 1000 threads to read
  auto *threads = new Poco::Thread[N_THREADS];
  auto *configs = new ThreadConfig[N_THREADS];
  auto *results = new ReadResult[N_THREADS];

  for (int i=0; i<N_THREADS; ++i) {
    configs[i].timeout = (randDouble() > .5) ? (long)(randDouble() * 500) : 0;
    configs[i].begin = (ssize_t)(randDouble() * 256);
    configs[i].count = (size_t)(randDouble() * 300);
    configs[i].buffer = (char*)malloc(configs[i].count);
    threads[i].startFunc([i, &configs, &buffer, &results] () {
      results[i] = buffer.read(configs[i].begin, configs[i].buffer, configs[i].count, configs[i].timeout);
    });
  }

  // write contents into the buffer
  int bytesWritten = 0;
  while (bytesWritten < BYTES_TO_WRITE) {
    int bytesToWrite = std::min((int)(randDouble() * 20), BYTES_TO_WRITE - bytesWritten);
    buffer.write(bytesRange(bytesWritten, bytesToWrite).data(), bytesToWrite);
    bytesWritten += bytesToWrite;
    usleep((int)(randDouble() * 50 * 1000));  // at most 50ms
  }

  // close the buffer and join all threads
  buffer.close();
  for (int i=0; i<N_THREADS; ++i) {
    threads[i].join();
  }

  // check all the results
  for (int i=0; i<N_THREADS; ++i) {
    REQUIRE_FALSE(results[i].isClosed);
    if (!configs[i].timeout) {
      REQUIRE_FALSE(results[i].isTimeout);
    }
    if (!results[i].isTimeout) {
      int expectedCount = std::min(BYTES_TO_WRITE - (int) configs[i].begin, (int) configs[i].count);
      assert(expectedCount >= 0);
      REQUIRE_EQUALS(results[i].begin, configs[i].begin);
      REQUIRE(results[i].count <= expectedCount);
      REQUIRE(strncmp(configs[i].buffer, (char *) bytesRange(configs[i].begin, results[i].count).data(), results[i].count) == 0);
    }
  }

  // now dispose the resources
  for (int i=0; i<N_THREADS; ++i) {
    free(configs[i].buffer);
  }
  delete [] results;
  delete [] configs;
  delete [] threads;
}

TEST_CASE("Test re-allocate reader list", "[OutputBuffer]") {
  CapturingLogger capturingLogger;
  Logger::ScopedRootLogger scopedRootLogger(&capturingLogger);
  static const int N_THREADS = 1002;
  static const int SPECIAL_INDEX = 500;

  OutputBuffer buffer(1);
  auto *threads = new Poco::Thread[N_THREADS];
  auto *targets = new char*[N_THREADS];
  auto *results = new ReadResult[N_THREADS];
  for (int i=0; i<N_THREADS; ++i) {
    targets[i] = (char*)malloc(1);
  }

  for (int i=0; i<N_THREADS; ++i) {
    if (i != SPECIAL_INDEX) {
      threads[i].startFunc([i, &buffer, &targets, &results]() {
        results[i] = buffer.read(0, targets[i], 1, 100);
      });
    } else {
      threads[i].startFunc([i, &buffer, &targets, &results] () {
        results[i] = buffer.read(0, targets[i], 1);
      });
    }
  }

  usleep(500 * 1000);
  buffer.write(bytesRange(123, 1).data(), 1);
  buffer.close();

  for (int i=0; i<1002; ++i) {
    threads[i].join();
  }
  REQUIRE_EQUALS(capturingLogger.capturedLogs().size(), 1);
  REQUIRE_EQUALS(capturingLogger.capturedLogs().at(0),
      CapturedLog("INFO", "Waiting queue for output buffer readers has been re-allocated."));

  for (int i=0; i<1002; ++i) {
    if (i != 500) {
      REQUIRE_FALSE(results[i].isClosed);
      REQUIRE(results[i].isTimeout);
    } else {
      REQUIRE_FALSE(results[i].isClosed);
      REQUIRE_FALSE(results[i].isTimeout);
      REQUIRE_EQUALS(results[i].count, 1);
      REQUIRE_EQUALS(targets[i][0], 123);
    }
  }

  for (int i=0; i<1002; ++i) {
    free(targets[i]);
  }
  delete [] results;
  delete [] targets;
  delete [] threads;
}
