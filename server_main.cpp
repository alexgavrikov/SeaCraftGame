/*
 * server.cpp
 *
 *  Created on: 8 ���. 2016 �.
 *      Author: user
 */

#include <cstdlib>
#include "server.h"

int main(int argc, char** argv) {

#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(0x0202, &wsaData);
#endif

  if (argc != 2) {
    std::printf("Usage: %s port_to_bind\n", argv[0]);
    return 1;
  }

  int port_to_bind = std::atoi(argv[1]);
  Server s;
  s.Bind(port_to_bind, "");
  s.AcceptLoop();

  return 0;
}

