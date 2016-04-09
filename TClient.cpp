/*
 * TClient.cpp
 *
 *  Created on: 9 àïð. 2016 ã.
 *      Author: user
 */

#include "TClient.h"

void TClient::SendMessages() {

  // Here we may need some wrapping-routine. Maybe we need to wrap a data
  // into html-headers-wrapper
  // Maybe it is not necessary if we use Qt.
  // CODE HERE

  std::string whole_message;
  while (!messages_queue.empty()) {
    whole_message += messages_queue.dequeue();
    whole_message += ";";
  }
  if(whole_message.empty()) {
    whole_message = "OK";
  }
  const char* data = whole_message.c_str();
  size_t sz = whole_message.size();
  for (; sz > 0;) {
    int res = send(client_socket_, data, sz, 0);
    if (res <= 0)
      return;
    data += res;
    sz -= res;
  }
}

void TClient::PrepareMessage(const std::string& message) {
  messages_queue.enqueue(message);
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


