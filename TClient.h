/*
 * TClient.h
 *
 *  Created on: 8 Г ГЇГ°. 2016 ГЈ.
 *      Author: user
 */

#ifndef TCLIENT_H_
#define TCLIENT_H_

#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")

static const std::string kLineEnd = "\r\n";
static const char kGetlineEnd = '\r';
static const size_t kLineEndLen = 2;
#elif __unix__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

static const std::string kLineEnd = "\n";
static const char kGetlineEnd = '\n';
static const size_t kLineEndLen = 1;
#endif

#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <list>
#include <mutex>
#include <vector>
#include <queue>
#include "queue_cond.h"

class ThreadSafeQueue {
public:
  ThreadSafeQueue() {
  }

  void enqueue(const std::string& message) {
    std::unique_lock<std::mutex> queue_mutex_wrapper(queue_mutex);
    messages.push(message);
  }

  std::string dequeue() {
    std::unique_lock<std::mutex> queue_mutex_wrapper(queue_mutex);
    std::string result = messages.front();
    messages.pop();

    return result;
  }

  bool empty() const {
    return messages.empty();
  }

private:
  std::mutex queue_mutex;
  std::queue<std::string> messages;
};

class TClient {
public:
  friend class Server;
  using Coordinate = std::pair<int, int>;

  enum {
    SHIPPING, WAITING, MAKING_STEP, WAITING_STEP
  };

  TClient(int client_socket,
          std::list<TClient>::iterator opponent,
          size_t status)
      : client_socket_(client_socket), opponent_(opponent), status_(status) {
  }

  void SendMessages();

  void PrepareMessage(const std::string& message);

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
  size_t GetShooting(const size_t x_coord,
                     const size_t y_coord,
                     std::vector<Coordinate>& pieces_of_killed);

private:
  // CorrectShips functions
  Coordinate FindEndOfShip(const Coordinate ship_begin) const;
  Coordinate FindBeginOfShip(const Coordinate ship_begin) const;
  std::vector<Coordinate> GetShip(const Coordinate ship_begin,
                                  const Coordinate ship_end) const;
  bool IsInGrid(const Coordinate coordinate) const;
  bool IsInShip(const Coordinate coordinate, 
                const Coordinate ship_begin, const Coordinate ship_end) const;
  std::vector<Coordinate> SurroundingOfShip(const Coordinate ship_begin,
                                            const Coordinate ship_end) const;
  std::vector<Coordinate> GetInclusiveShip(Coordinate coordinate) const;
  // End of CorrectShips functions  
  
  static const size_t kCorrectHitsForWin = 20;

  int client_socket_;
  std::list<TClient>::iterator opponent_;
  std::atomic<size_t> status_;
  std::vector<std::vector<size_t>> ships_;
  // When it is equal to kCorrectHitsForWin the game should end.
  size_t correct_hits_counter_ = 0;
  // Messages to send (will be sent in function TClient::Send())
  ThreadSafeQueue messages_queue;
  // We need mutex_for_starting_game in function Server::ReceiveShips(), because data race is possible
  // in section with changing statuses from WAITING to WAITING_STEP and MAKING_STEP.
  // Only one thread should do this section.
  std::mutex mutex_for_starting_game;bool gone = false;
  QueueWithCondVar<QueryAndSocket> queue_of_POST_queries_from_client;
};

#endif /* TCLIENT_H_ */
