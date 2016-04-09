/*
 * TClient.h
 *
 *  Created on: 8 апр. 2016 г.
 *      Author: user
 */

#ifndef TCLIENT_H_
#define TCLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdlib>
#include <list>
#include <vector>
#include <mutex>
#include <atomic>

class TClient {
public:
  friend class Server;

  enum {
    SHIPPING, WAITING, MAKING_STEP, WAITING_STEP
  };

  TClient(int client_socket,
         std::list<TClient>::iterator opponent,
         size_t status)
      : client_socket_(client_socket), opponent_(opponent), status_(status) {
  }

  void Send(const char* data, size_t sz);

  // Just check if ships are correct.
  bool CorrectShips() const;

  enum {
    MISS, HALF, KILL, WIN
  };

  enum {
    WATER, SHIP_PIECE_OK, SHIP_PIECE_DEAD
  };

  // The following function does a change in ships-vector (if necessary)
  // and returns MISS, HALF, KILL or WIN.
  size_t GetShooting(const size_t x_coord, const size_t y_coord);

private:
  static const size_t kCorrectHitsForWin = 18;

  int client_socket_;
  std::list<TClient>::iterator opponent_;
  std::atomic<size_t> status_;
  std::vector<std::vector<size_t>> ships_;
  // When it is equal to kCorrectHitsForWin the game should end.
  size_t correct_hits_counter_ = 0;
  // We need send_mutex_ in function TClient::Send(). We don't want to mix messages
  // from different threads.
  std::mutex send_mutex_;
  // We need send_mutex_ in function Server::ReceiveShips(), because data race is possible
  // in section with changing statuses from WAITING to WAITING_STEP and MAKING_STEP.
  // Only one thread should do this section.
  std::mutex mutex_for_starting_game;
  bool gone = false;
};

#endif /* TCLIENT_H_ */
