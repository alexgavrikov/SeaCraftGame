/*
 * server.cpp
 *
 *  Created on: 9 April 2016 year.
 *      Author: user
 */

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "server.h"
#include <cstdlib>
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
    if (!ResolveHost(host, addr)) {
      throw std::runtime_error("can't resolve host");
    }
    address.sin_addr.s_addr = addr;
  } else {
    address.sin_addr.s_addr = INADDR_ANY;
  }
  if (bind(listener_socket_holder_->GetSocket(), reinterpret_cast<sockaddr*>(&address),
      sizeof(address)) < 0) {
    throw std::runtime_error("can't bind");
  }
  if (listen(listener_socket_holder_->GetSocket(), 1) < 0) {
    throw std::runtime_error("can't start listening");
  }
}

void Server::AcceptLoop() {
  static constexpr size_t kInitialThreadsCount = 4;
  ThreadPool threads_pool(kInitialThreadsCount);
  threads_pool.enqueue([this] () {
    LoopOfSendingHTML();
  });
  while (true) {
    int sock = accept(listener_socket_holder_->GetSocket(), nullptr, nullptr);
    if (sock < 0) {
      std::cout << "can't bind" << std::endl;
    } else {
      std::shared_ptr<QueueWithCondVar<std::string>> packages(new QueueWithCondVar<std::string>);
      threads_pool.enqueue([this, packages, sock] () {
        LoopOfListenToOneSocket(packages, sock);
      });
      threads_pool.enqueue([this, packages, sock] () {
        LoopOfPreprocessingFromOneSocket(packages, sock);
      });
    }
  }
}

void Server::LoopOfSendingHTML() {
  static constexpr size_t kInitialThreadsCount = 2;
  std::string html;
  std::ifstream html_file("../html/index.html", std::ios_base::binary);
  std::string file_line, file_line_second, file_line_third;
  std::getline(html_file, file_line, kGetlineEnd);
  std::getline(html_file, file_line_second, kGetlineEnd);
  while (std::getline(html_file, file_line_third, kGetlineEnd)) {
    html.append(file_line + kHtmlLineEnd);
    std::swap(file_line, file_line_second);
    std::swap(file_line_second, file_line_third);
  }

  ThreadPool threads_pool(kInitialThreadsCount);
  while (true) {
    auto query = queue_of_GET_queries.dequeue();
    if (query.message.find("GET / HTTP") != std::string::npos) {
      std::stringstream strstream;
      strstream << "<div id=\"your_login\" class=\"" << current_free_login << "\">"
          << current_free_login << "</ div > " << kHtmlLineEnd;
      std::string html_with_login = html + strstream.str() + file_line + kHtmlLineEnd
          + file_line_second + kHtmlLineEnd;
      std::unique_lock<std::mutex> list_mutex_wrapper(list_mutex_);
      clients_.emplace_back(query.sock, clients_.end(), TClient::SHIPPING);
      Clients::iterator new_player_iter = --clients_.end();
      login_to_iterator_map[current_free_login++ - 100] = new_player_iter;
      clients_.back().PrepareMessage(html_with_login);
      clients_.back().SendMessages();

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
      const std::string answer_to_get_query_for_icon = "HTTP/1.1 200 OK" + kHttpLineEnd
          + "Content-Length: 3" + kHttpLineEnd + "Content-Type: text/html" + kHttpHeaderMessageDelimiter
          + "OKK";
      send(query.sock, answer_to_get_query_for_icon.c_str(), sizeof(answer_to_get_query_for_icon), 0);
    }
  }
}

bool Server::LoopOfListenToOneSocket(const std::shared_ptr<QueueWithCondVar<std::string>>& packages,
    int socket_i_listen) {
  while (true) {
    char buf[100000] = "";
    int size = recv(socket_i_listen, buf, sizeof(buf), 0);
    if (size <= 0) {
      packages->enqueue("END");
#ifdef _WIN32
      closesocket(socket_i_listen);
      WSACleanup();
#elif __unix__
      close(socket_i_listen);
#endif
      return false;
    }
    packages->enqueue(std::string(buf));
  }
}

bool Server::LoopOfPreprocessingFromOneSocket(
    const std::shared_ptr<QueueWithCondVar<std::string>>& packages, int source_socket) {
  std::string current_unfinished_message;
  std::string package;
  while (true) {
    if (package.empty()) {
      package = packages->dequeue();
    }
    if (package == "END") {
      return true;
    }
    current_unfinished_message += package;
    package.clear();
    if (current_unfinished_message.substr(0, 3) == "GET") {
      size_t end_pos = current_unfinished_message.find(kHttpHeaderMessageDelimiter);
      if (end_pos != std::string::npos) {
        package = current_unfinished_message.substr(end_pos + kHttpHeaderMessageDelimiterLen);
        queue_of_GET_queries.enqueue(
            QueryAndSocket(
                current_unfinished_message.substr(0, end_pos + kHttpHeaderMessageDelimiterLen),
                source_socket));
        current_unfinished_message.clear();
      }
    } else if (current_unfinished_message.find("POST") != std::string::npos) {
      size_t pos = current_unfinished_message.find("Content-Length: ");
      size_t pos2 = current_unfinished_message.find(kHttpHeaderMessageDelimiter);
      if (pos == std::string::npos || pos2 == std::string::npos) {
        continue;
      }
      size_t pos3 = current_unfinished_message.find(kHttpLineEnd, pos);
      int content_length = std::atoi(
          current_unfinished_message.substr(pos + 16, pos3 - (pos + 16)).c_str());
      std::string post_query = current_unfinished_message.substr(0,
          (pos2 + kHttpHeaderMessageDelimiterLen) + content_length);

      //Skipping HTTP header, its end is indicated by an empty line
      int begposition_of_message_itself = post_query.find(kHttpHeaderMessageDelimiter)
          + kHttpHeaderMessageDelimiterLen;
      std::string message_itself(post_query.substr(begposition_of_message_itself));
      size_t login = std::atoi(message_itself.substr(0, 3).c_str());
      auto client_iterator = login_to_iterator_map[login - 100];
      client_iterator->queue_of_POST_queries_from_client.enqueue(
          QueryAndSocket(message_itself.substr(4), source_socket));
      package = current_unfinished_message.substr(
          (pos2 + kHttpHeaderMessageDelimiterLen) + content_length);
      current_unfinished_message.clear();
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

void Server::ParseData(const char* buf, const size_t size, Clients::iterator client_iterator) {
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

bool Server::IsCoordinateCorrect(const size_t coordinate) {
  static constexpr size_t kMinimalCoordinate = 1;
  static constexpr size_t kMaximalCoordinate = 10;
  return coordinate >= kMinimalCoordinate && coordinate <= kMaximalCoordinate;
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

bool Server::RecieveShips(const char* buf, const size_t size, Clients::iterator client_iterator) {
  static const std::string kShippingHeader("shipping:");
  static const size_t kShippingHeaderLen = kShippingHeader.size();
  static const size_t kShipsMessageSize = 100 + kShippingHeaderLen;

  if (client_iterator->status_ != TClient::SHIPPING) {
    return false;
  }

  // Check if it is about receiving ships. if not return false
  // if yes, extracting ships from  buf into client_iterator->ships; i. e. parsing buf here
  // message about ships has to look like: "shipping:1010..000"
  // One Hundred bits (zeros or ones) in message above!

  if (size != kShipsMessageSize) {
    return false;
  }

  if (std::string(buf, kShippingHeaderLen) != kShippingHeader) {
    return false;
  }

  buf += kShippingHeaderLen;
  client_iterator->ships_.resize(10);
  for (int y_coord = 0; y_coord != 10; ++y_coord) {
    client_iterator->ships_[y_coord].resize(10, TClient::WATER);
    for (int x_coord = 0; x_coord != 10; ++x_coord, ++buf) {
      if (*buf == '1') {
        client_iterator->ships_[y_coord][x_coord] = TClient::SHIP_PIECE_OK;
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

// Check if buf's message is about receiving step. if not return false
// if true: coordinates of his step should be in x_coord and y_coord
// (let numeration be from 1 to 10).
// message about step has to look like: "step:5:7"
// 5 in this example means 5th row, 7 means 7th column.
// In the example: x_coord = 7, y_coord = 5
bool Server::RecieveStep(const char* buf, const size_t size, Clients::iterator client_iterator) {
  static const std::string kStepHeader("step:");
  static const size_t kStepHeaderLen = kStepHeader.size();

  if (client_iterator->status_ != TClient::MAKING_STEP) {
    return false;
  }

  if (std::string(buf, kStepHeaderLen) != kStepHeader) {
    return false;
  }

  std::stringstream stream(buf + kStepHeaderLen);
  int y_coord = 0, x_coord = 0;
  char delimiter = 0;
  stream >> y_coord >> delimiter >> x_coord;
  if (!IsCoordinateCorrect(y_coord) || !IsCoordinateCorrect(x_coord) || delimiter != ':'
      || !stream.eof()) {
    return false;
  }

  std::vector<TClient::Coordinate> pieces_of_killed;
  const size_t result_of_shooting = client_iterator->opponent_->GetShooting(x_coord, y_coord,
      pieces_of_killed);

  std::stringstream stream_for_message_ending_if_not_killed;
  stream_for_message_ending_if_not_killed << ":" << y_coord << ":" << x_coord;
  std::stringstream stream_for_message_ending_if_killed;
  for (const auto& killed_piece : pieces_of_killed) {
    stream_for_message_ending_if_killed << ":" << killed_piece.first << ":" << killed_piece.second;
  }

  std::string message_for_client, message_for_opponent;
  switch (result_of_shooting) {
    case TClient::MISS: {
      message_for_client = "field2:miss" + stream_for_message_ending_if_not_killed.str();
      message_for_opponent = "field1:miss" + stream_for_message_ending_if_not_killed.str();
      client_iterator->status_ = TClient::WAITING_STEP;
      client_iterator->opponent_->status_ = TClient::MAKING_STEP;
      break;
    }

    case TClient::HALF: {
      message_for_client = "field2:half" + stream_for_message_ending_if_not_killed.str();
      message_for_opponent = "field1:half" + stream_for_message_ending_if_not_killed.str();
      break;
    }

    case TClient::KILL: {
      message_for_client = "field2:kill" + stream_for_message_ending_if_killed.str();
      message_for_opponent = "field1:kill" + stream_for_message_ending_if_killed.str();
      break;
    }

    case TClient::WIN: {
      message_for_client = "field2:kill" + stream_for_message_ending_if_killed.str();
      message_for_opponent = "field1:kill" + stream_for_message_ending_if_killed.str();
      break;
    }
  }

  client_iterator->PrepareMessage(message_for_client);
  client_iterator->opponent_->PrepareMessage(message_for_opponent);
  if (result_of_shooting == TClient::WIN) {
    client_iterator->PrepareMessage("won");
    client_iterator->opponent_->PrepareMessage("lost");
  }

  return true;
}

bool Server::IsFree(Clients::iterator client_iterator) const {
	return client_iterator->opponent_ == clients_.end();
}

