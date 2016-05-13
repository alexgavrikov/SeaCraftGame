#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#elif __unix__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <cstring>
#include <cstdio>
#include <exception>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include "TClient.h"
#include "queue_cond.h"

class Server {
private:
  class TSocketHolder {
  private:
    int Socket;

    TSocketHolder(const TSocketHolder &) = delete;
    TSocketHolder(TSocketHolder &&) = delete;
    TSocketHolder &operator =(const TSocketHolder &) = delete;
    TSocketHolder &operator =(TSocketHolder &&) = delete;

  public:
    TSocketHolder()
        : Socket(socket(AF_INET, SOCK_STREAM, 0)) {
      if (Socket < 0)
        throw std::runtime_error("could not create socket");
    }
    TSocketHolder(int socket)
        : Socket(socket) {
    }
    ~TSocketHolder() {
#ifdef _WIN32
      closesocket(Socket);
      WSACleanup();
#elif __unix__
      close(Socket);
#endif
    }
    int GetSocket() const {
      return Socket;
    }
  };

public:
  // Helps for debugging.
  typedef std::chrono::microseconds Microseconds;
  typedef std::chrono::steady_clock Clock;
  typedef Clock::time_point Time;
  void sleep(unsigned milliseconds) const {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  }

  Server() : listener_socket_holder_(new TSocketHolder) {}
  Server(int sock) : listener_socket_holder_(new TSocketHolder(sock)) {}
  void Bind(int port, const std::string &host);
  void AcceptLoop();
  int GetSocket() const { return listener_socket_holder_->GetSocket(); }

private:
  using TSocketPtr = std::shared_ptr<TSocketHolder>;
  using Clients = std::list<TClient>;

  static bool ResolveHost(const std::string &host, int &addr);
  static void ConnectTwoClients(Clients::iterator free_player_iter_first,
                         Clients::iterator free_player_iter_second);

  // Returns true if connection was closed by handler, false if connection was closed by peer
  bool RecvLoop(Clients::iterator client);
  void LoopOfSendingHTML();
  bool LoopOfListenToOneSocket(const std::shared_ptr<
                                                            QueueWithCondVar<
                                                                std::string>>& packages,
                                                        int socket_i_listen);
  bool LoopOfPreprocessingFromOneSocket(const std::shared_ptr<
                                                  QueueWithCondVar<
                                                      std::string>>& packages,
                                              int source_socket);
  void ParseData(const char* buf, int size, Clients::iterator client_iterator);
  void Disconnect(Clients::iterator client_iterator);
  bool RecieveShips(const char* buf,
                    int size,
                    Clients::iterator client_iterator);
  bool RecieveStep(const char* buf,
                   int size,
                   Clients::iterator client_iterator);
  bool IsFree(Clients::iterator client_iterator) const;

  TSocketPtr listener_socket_holder_;
  Clients clients_;
  std::mutex list_mutex_;
  Clients::iterator login_to_iterator_map[900];
  // Logins are in range from 100 to 999. To get client_iterator from login:
  // login_to_iterator_map[login - 100]
  size_t current_free_login = 100;
  QueueWithCondVar<QueryAndSocket> queue_of_GET_queries;
};
