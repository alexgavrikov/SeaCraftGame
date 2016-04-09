/*
 * TClient.cpp
 *
 *  Created on: 9 апр. 2016 г.
 *      Author: user
 */

#include "TClient.h"


  void TClient::Send(const char* data, size_t sz) {

    // Here we may need some wrapping-routine. Maybe we need to wrap a data
    // into html-headers-wrapper
    // Maybe it is not necessary if we use Qt.
    // CODE HERE

    std::unique_lock<std::mutex> send_mutex_wrapper(send_mutex_);
    for (; sz > 0;) {
      int res = send(client_socket_, data, sz, 0);
      if (res <= 0)
        return;
      data += res;
      sz -= res;
    }
  }

  // Just check if ships are correct.
  bool TClient::CorrectShips() const {

    // CODE HERE

  }

  // The following function does a change in ships-vector (if necessary)
  // and returns MISS, HALF, KILL or WIN.
  size_t TClient::GetShooting(const size_t x_coord, const size_t y_coord) {

    // CODE HERE

  }

