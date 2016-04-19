/*
 * server.cpp
 *
 *  Created on: 8 апр. 2016 г.
 *      Author: user
 */

#include "server.h"

int main() {

#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(0x0202, &wsaData);
#endif

  Server s;
  s.Bind(1234, "");

  return 0;
}

