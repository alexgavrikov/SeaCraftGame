/*
 * TClient.h
 *
 *  Created on: 8 àïð. 2016 ã.
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
#include <queue>

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
  size_t GetShooting(const size_t x_coord, const size_t y_coord);

private:
 
  // CorrectShips functions
  using Coordinate = std::pair<int, int>;
  Coordinate FindEndOfShip(const Coordinate ship_begin);
  Coordinate FindBeginOfShip(const Coordinate ship_begin);
  std::vector<Coordinate> GetShip(const Coordinate ship_begin,
                                  const Coordinate ship_end);
  bool IsInGrid(const Coordinate coordinate);
  bool IsInShip(const Coordinate coordinate, 
                const Coordinate ship_begin, const Coordinate ship_end);
  std::vector<Coordinate> SurroundingOfShip(const Coordinate ship_begin,
                                            const Coordinate ship_end);
  std::vector<Coordinate> GetInclusiveShip(Coordinate coordinate);
  // End of CorrectShips functions  
  
  static const size_t kCorrectHitsForWin = 18;

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
};

#endif /* TCLIENT_H_ */

