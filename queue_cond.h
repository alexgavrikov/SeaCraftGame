/*
 * queue_cond.h
 *
 *  Created on: 23 апр. 2016 г.
 *      Author: user
 */

#ifndef QUEUE_COND_H_
#define QUEUE_COND_H_

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

struct QueryAndSocket {
  std::string message;
  int sock;

  QueryAndSocket(const std::string& message, int sock)
      : message(message), sock(sock) {
  }
};

template<typename T>
class QueueWithCondVar {
private:
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cond;

public:
  void enqueue(const T& item) {
    std::unique_lock<std::mutex> ulock(mutex);
    queue.push(item);
    ulock.unlock();
    cond.notify_one();
  }

  T dequeue() {
    std::unique_lock<std::mutex> ulock(mutex);
    while (queue.empty())
      cond.wait(ulock);
    T val = queue.front();
    queue.pop();
    return val;
  }

  QueueWithCondVar() = default;
  QueueWithCondVar(const QueueWithCondVar&) = delete;    // disable copying
  QueueWithCondVar& operator=(const QueueWithCondVar&) = delete; // disable assignment
};

#endif /* QUEUE_COND_H_ */
