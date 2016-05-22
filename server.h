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
      if (Socket < 0) {
        throw std::runtime_error("could not create socket");
      }
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
  Server()
      : listener_socket_holder_(new TSocketHolder) {
  }
  Server(int sock)
      : listener_socket_holder_(new TSocketHolder(sock)) {
  }

  int GetSocket() const {
    return listener_socket_holder_->GetSocket();
  }

  void Bind(int port, const std::string &host);
  void AcceptLoop(const std::string& html_input_file);

private:
  using TSocketPtr = std::shared_ptr<TSocketHolder>;
  using Clients = std::list<TClient>;

  static bool ResolveHost(const std::string &host, int &addr);
  void PrepareHTML(const std::string& html_input_file);

  bool IsFree(Clients::iterator client_iterator) const;
  static void ConnectTwoClients(Clients::iterator free_player_iter_first,
      Clients::iterator free_player_iter_second);
  void Disconnect(Clients::iterator client_iterator);

  bool LoopOfListenToOneSocket(int socket_i_listen);
  bool LoopOfPreprocessingFromOneSocket(QueueWithCondVar<std::string>* const packages,
      int source_socket);
  bool LoopOfDistributingQueries(
      QueueWithCondVar<std::string>* const queries, int source_socket);
  void SendHTML(const std::string& get_query, int source_socket);
  void SendIcon(const std::string& get_query, int source_socket);
  bool HandlePostQueryContent(Clients::iterator client);

  static const std::string kShippingHeader;
  static const size_t kShippingHeaderLen;
  static const size_t kShipsMessageSize;
  bool IsAboutShips(const std::string& message, Clients::iterator client_iterator);
  bool FetchShips(const std::string& message, Clients::iterator client_iterator);

  static const std::string kStepHeader;
  static const size_t kStepHeaderLen;
  bool IsAboutStep(const std::string& message, Clients::iterator client_iterator);
  bool FetchStep(const std::string& message, Clients::iterator client_iterator);
  static bool IsCoordinateCorrect(const size_t coordinate);

  TSocketPtr listener_socket_holder_;
  Clients clients_;
  std::mutex list_mutex_;
  Clients::iterator login_to_iterator_map[900];
  // Logins are in range from 100 to 999. Getting client_iterator from login:
  // login_to_iterator_map[login - 100]
  size_t current_free_login = 100;
  QueueWithCondVar<PostQuery> queue_of_GET_queries;
  std::string html, file_line, file_line_second;
};
