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
#include "thread_pool.h"

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
      reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    throw std::runtime_error("can't bind");
  if (listen(listener_socket_holder_->GetSocket(), 1) < 0)
    throw std::runtime_error("can't start listening");
}

void Server::AcceptLoop() {
  static constexpr size_t kInitialThreadsCount = 2;
  char html[1024];

  // Reading HTML-page from file
  // CODE HERE

  ThreadPool threads_pool(kInitialThreadsCount);
  while (true) {
    // std::cout << "main: " << "start while-iteraton" << std::endl;
    int sock = accept(listener_socket_holder_->GetSocket(), nullptr,
        nullptr);
    if (sock < 0) {
      std::cout << "can't bind" << std::endl;
    } else {
      std::unique_lock<std::mutex> list_mutex_wrapper(list_mutex_);
      clients_.emplace_back(sock, clients_.end(), TClient::SHIPPING);

      // Here we are still not sure about a way to send HTML-page.
      // Something like that:
      // clients_.back().PrepareMessage(html);
      // clients_.back().SendMessages();
      // As for wrapping-routine, I suppose it will be inside function TClient::SendMessages()
      // CODE HERE

      Clients::iterator new_player_iter = --clients_.end();
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
    }
  }
}

// Returns true if connection was closed by handler, false if connection was closed by peer
bool Server::RecvLoop(Clients::iterator client) {
  // std::cout << "  non-main: " << "just started" << std::endl;

  while (true) {
    // std::cout << "  non-main: " << "new while-iteration" << std::endl;
    char buf[1024];
    int res = recv(client->client_socket_, buf, sizeof(buf), 0);
    // std::cout << "  non-main: " << "received" << std::endl;
    if (res <= 0) {
      Disconnect(client);
      return false;
    }
    ParseData(buf, res, client);
  }

  return true;
}

void Server::ParseData(char* buf,
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

bool Server::RecieveShips(char* buf,
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
  // CODE HERE

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

bool Server::RecieveStep(char* buf,
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
  // CODE HERE

  size_t result_of_shooting = client_iterator->opponent_->GetShooting(
      x_coord, y_coord);

  char message_ending[7];
  sprintf(message_ending, ":%d:%d", x_coord, y_coord);

  switch (result_of_shooting) {
  case TClient::MISS: {
    char message_for_client[12] = "field2:miss";
    char message_for_opponent[12] = "field1:miss";
    ConcatenateAndSend(client_iterator, message_for_client,
        message_for_opponent, message_ending);
    client_iterator->status_ = TClient::WAITING_STEP;
    client_iterator->opponent_->status_ = TClient::MAKING_STEP;
    break;
  }

  case TClient::HALF: {
    char message_for_client[12] = "field2:half";
    char message_for_opponent[12] = "field1:half";
    ConcatenateAndSend(client_iterator, message_for_client,
        message_for_opponent, message_ending);
    break;
  }

  case TClient::KILL: {
    char message_for_client[12] = "field2:kill";
    char message_for_opponent[12] = "field1:kill";
    ConcatenateAndSend(client_iterator, message_for_client,
        message_for_opponent, message_ending);
    break;
  }

  case TClient::WIN: {
    char message_for_client[12] = "field2:kill";
    char message_for_opponent[12] = "field1:kill";
    ConcatenateAndSend(client_iterator, message_for_client,
        message_for_opponent, message_ending);
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

