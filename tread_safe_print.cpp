/*
 * tread_safe_print.cpp
 *
 *  Created on: 23 апр. 2016 г.
 *      Author: user
 */

#include "thread_safe_print.h"
#include <iostream>
#include <mutex>

void ThreadSafePrint(const std::stringstream& strstream) {
  static std::mutex print_mutex;
  print_mutex.lock();
  std::cout << strstream.str();
  print_mutex.unlock();
}

