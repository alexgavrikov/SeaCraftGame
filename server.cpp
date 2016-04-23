/*
 * server.cpp
 *
 *  Created on: 9 àïð. 2016 ã.
 *      Author: user
 */

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "server.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include "thread_pool.h"
#include "thread_safe_print.h"

bool Server::ResolveHost(const std::string &host, int &addr) {
  hostent *ent = gethostbyname(host.c_str());
  if (ent == nullptr)
    return false;
  for (size_t i = 0; ent->h_addr_list[i]; ++i) {
    addr = *reinterpret_cast<int**>(ent->h_addr_list)[i];
    return true;
  }
  return false;
}

void Server::Bind(int port, const std::string &host) {
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (!host.empty()) {
    int addr;
    if (!ResolveHost(host, addr))
      throw std::runtime_error("can't resolve host");
    address.sin_addr.s_addr = addr;
  } else {
    address.sin_addr.s_addr = INADDR_ANY;
    std::cout << INADDR_ANY << std::endl;
  }
  if (bind(listener_socket_holder_->GetSocket(),
           reinterpret_cast<sockaddr*>(&address),
           sizeof(address)) < 0)
    throw std::runtime_error("can't bind");
  if (listen(listener_socket_holder_->GetSocket(), 1) < 0)
    throw std::runtime_error("can't start listening");
}

void Server::AcceptLoop() {
  static constexpr size_t kInitialThreadsCount = 3;
  ThreadPool threads_pool(kInitialThreadsCount);
  threads_pool.enqueue([this] () {
    LoopOfSendingHTML();
  });
  while (true) {
    int sock = accept(listener_socket_holder_->GetSocket(),
                      nullptr,
                      nullptr);
    if (sock < 0) {
      std::cout << "can't bind" << std::endl;
    } else {
      threads_pool.enqueue([this, sock] () {
        LoopOfListenToOneSocket(sock);
      });
    }
  }
}

void Server::LoopOfSendingHTML() {
  static constexpr size_t kInitialThreadsCount = 2;
  std::string html;
  std::ifstream html_file("../html/index.html", std::ios_base::binary);
  std::string file_line;
  std::string file_line_second;
  std::string file_line_third;
  std::getline(html_file, file_line, '\r');
  std::getline(html_file, file_line_second, '\r');
  while (std::getline(html_file, file_line_third, '\r')) {
    html.append(file_line);
    std::swap(file_line, file_line_second);
    std::swap(file_line_second, file_line_third);
  }
  std::cout << "here" << std::endl;

  ThreadPool threads_pool(kInitialThreadsCount);
  while (true) {
    auto query = queue_of_GET_queries.dequeue();
    if (query.message.find("GET / HTTP") != std::string::npos) {
      std::string html_with_login = html;
      std::stringstream strstream;
      strstream << "\n<div id=\"your_login\" class=\"";
      strstream << current_free_login;
      strstream << "\"></div>\n<div>";
      strstream << current_free_login <<"</div>";
      html_with_login.append(strstream.str());
      html_with_login.append(file_line);
      html_with_login.append(file_line_second);
      std::unique_lock<std::mutex> list_mutex_wrapper(list_mutex_);
      clients_.emplace_back(query.sock, clients_.end(), TClient::SHIPPING);
      Clients::iterator new_player_iter = --clients_.end();
      login_to_iterator_map[current_free_login - 100] = new_player_iter;
      ++current_free_login;
//    char buf[1024] = "";
      std::cout << "gere" << std::endl;
      clients_.back().PrepareMessage(html_with_login);
      clients_.back().SendMessages();
      std::cout << "tere" << std::endl;

      if (clients_.size() > 1) {
        Clients::iterator maybe_free_player_iter = --(--clients_.end());
        if (IsFree(maybe_free_player_iter)) {
          ConnectTwoClients(maybe_free_player_iter, new_player_iter);
        }
      }
      list_mutex_wrapper.unlock();
      auto future = threads_pool.enqueue([this, &new_player_iter] () {
        RecvLoop(new_player_iter);
      });
    } else {
      char okk_message[] =
          "HTTP/1.1 200 OK\nContent-Length: 3\nContent-Type: text/html\n\nOKK";
      send(query.sock, okk_message, sizeof(okk_message), 0);
    }
  }
}

bool Server::LoopOfListenToOneSocket(int socket_i_listen) {
  while (true) {
    char buf[100000] = "";
    std::cout <<"ff"<<std::endl;
    int size = recv(socket_i_listen, buf, sizeof(buf), 0);
    std::cout <<"gg"<<std::endl;
    if (size <= 0) {
      close(socket_i_listen);
      return false;
    }
    std::string message_with_headers(buf);
    if (message_with_headers.substr(0, 3) == "GET") {
      queue_of_GET_queries.enqueue(QueryAndSocket(message_with_headers,
                                                  socket_i_listen));
    } else {
      //Skipping HTTP header, its end is indicated by an empty line
      const char* buf_ptr = buf;
    std::cout <<"hh"<<std::endl;
      for (buf_ptr += 2, size -= 2;
          size > 0 && (*(buf_ptr - 1) != '\n' || *(buf_ptr - 2) != '\n');
          ++buf_ptr, --size) {
      }
    std::cout <<"ee"<<socket_i_listen<<message_with_headers<<"ee"<<std::endl;
      std::string message_itself(buf_ptr);
    std::cout <<"eee"<<message_itself<<"eee"<<std::endl;
      int login_end_pos = message_itself.find(':');
    std::cout <<"q"<<login_end_pos<<"q"<<std::endl;
      std::stringstream ss;
      ss << message_itself.substr(login_end_pos - 3, 3);
      int login;
      ss >> login;
      std::cout <<"dd"<<login<<"dd"<<std::endl;
      auto client_iterator = login_to_iterator_map[login - 100];
      client_iterator->queue_of_POST_queries_from_client.enqueue(QueryAndSocket(std::string(message_itself,
                                                                                            4),
                                                                                socket_i_listen));
    }
  }
}

// Returns true if connection was closed by handler, false if connection was closed by peer
bool Server::RecvLoop(Clients::iterator client) {
  while (true) {
    auto query = client->queue_of_POST_queries_from_client.dequeue();
    client->client_socket_ = query.sock;
    ParseData(query.message.c_str(), query.message.size(), client);
  }

  return true;
}

void Server::ParseData(const char* buf,
                       int size,
                       Clients::iterator client_iterator) {
  // Functions Server::RecieveShips(...) and Server::RecieveStep(...) contain only preparing messages 
  // (pushing them into the clients' messages_queues by calling TClient::PrepareMessage(...)). And after
  // that we send messages by calling TClient::SendMessages()

  if (RecieveShips(buf, size, client_iterator)) {
    client_iterator->SendMessages();
    return;
  }

  if (RecieveStep(buf, size, client_iterator)) {
    client_iterator->SendMessages();
    return;
  }

  client_iterator->SendMessages();
}

void Server::ConnectTwoClients(Clients::iterator free_player_iter_first,
                               Clients::iterator free_player_iter_second) {
  free_player_iter_first->PrepareMessage("opponent:came");
  free_player_iter_second->PrepareMessage("opponent:came");
  free_player_iter_first->opponent_ = free_player_iter_second;
  free_player_iter_second->opponent_ = free_player_iter_first;
  if (free_player_iter_first->status_ == TClient::WAITING) {
    free_player_iter_second->PrepareMessage("opponent:shipped");
  }
}

void Server::Disconnect(Clients::iterator client_iterator) {
  std::unique_lock<std::mutex> list_mutex_wrapper(list_mutex_);
  client_iterator->gone = true;
  if (client_iterator->opponent_ != clients_.end()) {
    if (client_iterator->opponent_->gone) {
      clients_.erase(client_iterator->opponent_);
      clients_.erase(client_iterator);
    } else {
      client_iterator->opponent_->PrepareMessage("end:");
    }
  } else {
    clients_.erase(client_iterator);
  }
}

bool Server::RecieveShips(const char* buf,
                          int size,
                          Clients::iterator client_iterator) {
  if (client_iterator->status_ != TClient::SHIPPING) {
    return false;
  }

  // Check if it is about receiving ships. if not return false
  // if yes, extracting ships from  buf into client_iterator->ships; i. e. parsing buf here
  // message about ships has to look like: "ships:1010..000" (maybe with some HTTP-wrapping)
  // One Hundred bits (zeros or ones) in message above! Развёртываются в двумерную таблицу:
  // первые 10 бит - первая строка (то есть ships[0]), вторые 10 бит - вторая строка (то есть ships[1]),
  // и так далее.

  const int ships_message_size = 106;
  if (size != ships_message_size) {
    return false;
  }

  if (std::string(buf, 6) != "ships:") {
    return false;
  }

  client_iterator->ships_.resize(10);
  buf += 6;
  for (int y_coord = 0; y_coord != 10; ++y_coord) {
    client_iterator->ships_[y_coord].resize(10);
    for (int x_coord = 0; x_coord != 10; ++x_coord) {
      if (*buf == '0') {
        client_iterator->ships_[y_coord][x_coord] = TClient::WATER;
      } else if (*buf == '1') {
        client_iterator->ships_[y_coord][x_coord] = TClient::SHIP_PIECE_OK;
      } else {
        client_iterator->ships_.clear();
        return false;
      }
    }
  }

  if (client_iterator->CorrectShips()) {
    client_iterator->PrepareMessage("shipping:ok");
    client_iterator->status_ = TClient::WAITING;
    if (client_iterator->opponent_ != clients_.end()) {
      client_iterator->opponent_->PrepareMessage("opponent:shipped");
      // The following section is critical. We need mutexing.
      std::lock(client_iterator->mutex_for_starting_game,
                client_iterator->opponent_->mutex_for_starting_game);
      if (client_iterator->opponent_->status_ == TClient::WAITING) {
        client_iterator->status_ = TClient::WAITING_STEP;
        client_iterator->opponent_->status_ = TClient::MAKING_STEP;
        client_iterator->opponent_->PrepareMessage("go1");
        client_iterator->PrepareMessage("go2");
      }
      client_iterator->mutex_for_starting_game.unlock();
      client_iterator->opponent_->mutex_for_starting_game.unlock();
    }
  } else {
    client_iterator->PrepareMessage("shipping:wrong");
  }

  return true;
}

bool Server::RecieveStep(const char* buf,
                         int size,
                         Clients::iterator client_iterator) {
  if (client_iterator->status_ != TClient::MAKING_STEP) {
    return false;
  }

  //use of uninitialized local variable is an error in Visual Studio
  size_t x_coord = 7;
  size_t y_coord = 5;
  // Check if buf's message is about receiving step. if not return false
  // if true: coordinates of his step should be in x_coord and y_coord
  // (let numeration be from 1 to 10).
  // message about step has to look like: "step:5:7" (maybe with some HTTP-wrapping)
  // 5 in this example means 5th row, 7 means 7th column.
  // In the example: x_coord = 7, y_coord = 5

  const int min_step_message_size = 8;
  const int max_step_message_size = 10;
  if (size < min_step_message_size || size > max_step_message_size) {
    return false;
  }

  if (std::string(buf, 5) != "step:") {
    return false;
  }
  buf += 5;
  size -= 5;

  if (buf[1] == ':') {
    if (buf[0] < '1' || buf[0] > '9') {
      return false;
    } else {
      y_coord = buf[0] - '0';
      buf += 2;
      size -= 2;
    }
  } else {
    if (buf[0] != '1' || buf[1] != '0' || buf[2] != ':') {
      return false;
    } else {
      y_coord = 10;
      buf += 3;
      size -= 3;
    }
  }

  switch (size) {
  case 1:
    if (buf[0] < '1' || buf[0] > '9') {
      return false;
    } else {
      x_coord = buf[0] - '0';
    }
    break;

  case 2:
    if (buf[0] != '1' || buf[1] != '0') {
      return false;
    } else {
      x_coord = 10;
    }
  }

  size_t result_of_shooting =
      client_iterator->opponent_->GetShooting(x_coord, y_coord);

  char message_ending[7];
  sprintf(message_ending, ":%d:%d", x_coord, y_coord);

  switch (result_of_shooting) {
  case TClient::MISS: {
    char message_for_client[12] = "field2:miss";
    char message_for_opponent[12] = "field1:miss";
    ConcatenateAndSend(client_iterator,
                       message_for_client,
                       message_for_opponent,
                       message_ending);
    client_iterator->status_ = TClient::WAITING_STEP;
    client_iterator->opponent_->status_ = TClient::MAKING_STEP;
    break;
  }

  case TClient::HALF: {
    char message_for_client[12] = "field2:half";
    char message_for_opponent[12] = "field1:half";
    ConcatenateAndSend(client_iterator,
                       message_for_client,
                       message_for_opponent,
                       message_ending);
    break;
  }

  case TClient::KILL: {
    char message_for_client[12] = "field2:kill";
    char message_for_opponent[12] = "field1:kill";
    ConcatenateAndSend(client_iterator,
                       message_for_client,
                       message_for_opponent,
                       message_ending);
    break;
  }

  case TClient::WIN: {
    char message_for_client[12] = "field2:kill";
    char message_for_opponent[12] = "field1:kill";
    ConcatenateAndSend(client_iterator,
                       message_for_client,
                       message_for_opponent,
                       message_ending);
    client_iterator->PrepareMessage("won");
    client_iterator->opponent_->PrepareMessage("lost");
    break;
  }
  }

  return true;
}

void Server::ConcatenateAndSend(Clients::iterator client_iterator,
                                char* message_for_client,
                                char* message_for_opponent,
                                char* message_ending) {
  strcat(message_for_client, message_ending);
  strcat(message_for_opponent, message_ending);
  client_iterator->PrepareMessage(message_for_client);
  client_iterator->PrepareMessage(message_for_opponent);
}

bool Server::IsFree(Clients::iterator client_iterator) const {
  return client_iterator->opponent_ == clients_.end();
}

