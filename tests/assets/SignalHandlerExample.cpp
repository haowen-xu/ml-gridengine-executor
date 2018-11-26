//
// Created by 许昊文 on 2018/11/26.
//

#include <unistd.h>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>
#include "src/SignalHandler.h"


int main() {
  int counter = 0;
  Poco::Mutex mutex;
  Poco::Condition cond;
  time_t deadTime = time(nullptr) + 2;

  {
    SignalHandler signalHandler1([&mutex, &cond, &counter](int signalValue) {
      Poco::Mutex::ScopedLock scopedLock1(mutex);
      ++counter;
      cond.broadcast();
    });

    {
      SignalHandler signalHandler2([&mutex, &cond, &counter](int signalValue) {
        Poco::Mutex::ScopedLock scopedLock1(mutex);
        ++counter;
        cond.broadcast();
      });

      while (counter < 2 && time(nullptr) < deadTime) {
        Poco::Mutex::ScopedLock scopedLock1(mutex);
        cond.tryWait(mutex, 100);
      }
    }
  }

  if (counter < 2) {
    exit(124);
  } else {
    exit(123);
  }
}
