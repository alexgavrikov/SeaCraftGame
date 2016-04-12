/*
 * TClient.cpp
 *
 *  Created on: 9 àïð. 2016 ã.
 *      Author: user
 */

#include "TClient.h"

void TClient::SendMessages() {

  // Here we may need some wrapping-routine. Maybe we need to wrap a data
  // into html-headers-wrapper
  // Maybe it is not necessary if we use Qt.
  // CODE HERE

  std::string whole_message;
  while (!messages_queue.empty()) {
    whole_message += messages_queue.dequeue();
    whole_message += ";";
  }
  if(whole_message.empty()) {
    whole_message = "OK";
  }
  const char* data = whole_message.c_str();
  size_t sz = whole_message.size();
  for (; sz > 0;) {
    int res = send(client_socket_, data, sz, 0);
    if (res <= 0)
      return;
    data += res;
    sz -= res;
  }
}

void TClient::PrepareMessage(const std::string& message) {
  messages_queue.enqueue(message);
}

using Coordinate = std::pair<int, int>;

// Just check if ships are correct.
bool TClient::CorrectShips() const {
  std::vector<std::vector<bool>> is_visited(10, std::vector<bool>(10));
  std::vector<int> number_of_ships(4);
  
  for (int y_cord = 0; y_cord < 10; ++y_cord) {
    for (int x_cord = 0; x_cord < 10; ++x_cord) {
      if (is_visited[y_cord][x_cord]) {
        continue;
      }
      if (ships_[y_cord][x_cord] == TClient::SHIP_PIECE_OK) {
        auto begin = std::make_pair(y_cord, x_cord);
        auto end = FindEndOfShip(begin, grid);
        int ship_size = GetShip(begin, end).size();
        if (ship_size > 4) {
          return false;
        }
        number_of_ships[ship_size - 1] += 1;
        
        for (auto coordinate : GetShip(begin, end)) {
          is_visited[coordinate.first][coordinate.second] = true;
        }
        
        for (auto coordinate : SurroundingOfShip(begin, end)) {
          if (ships_[coordinate.first][coordinate.second] == TClient::SHIP_PIECE_OK) {
            return false;
          }
          is_visited[coordinate.first][coordinate.second] = true;  
        }
      } else {
        is_visited[y_cord][x_cord] = true;
      }
    }
  }
  
  return number_of_ships[0] == 4 && number_of_ships[1] == 3 
         && number_of_ships[2] == 2 && number_of_ships[3] == 1;
}

Coordinate TClient::FindBeginOfShip(const Coordinate ship_begin) {
  // TRY TO GO DOWN
  auto next_state = std::make_pair(ship_begin.first - 1, ship_begin.second);
  if (next_state.first >= 0 
      && grid[next_state.first][next_state.second]) {
    while (next_state.first >= 0 
           && grid[next_state.first][next_state.second]) {
      next_state = std::make_pair(next_state.first - 1, next_state.second);
    }
    return std::make_pair(next_state.first + 1, next_state.second);
  }
  // TRY TO GO RIGHT
  next_state = std::make_pair(ship_begin.first, ship_begin.second - 1);
  if (next_state.second >= 0 
      && grid[next_state.first][next_state.second]) {
    while (next_state.second >= 0 
           && grid[next_state.first][next_state.second]) {
      next_state = std::make_pair(next_state.first, next_state.second - 1);
    }
    return std::make_pair(next_state.first, next_state.second + 1);
  }

  return ship_begin;
}

Coordinate TClient::FindEndOfShip(const Coordinate ship_begin) {
  // TRY TO GO DOWN
  auto next_state = std::make_pair(ship_begin.first + 1, ship_begin.second);
  if (next_state.first < 10 
      && ships_[next_state.first][next_state.second] == TClient::SHIP_PIECE_OK) {
    while (next_state.first < 10 
           && ships_[next_state.first][next_state.second] == TClient::SHIP_PIECE_OK) {
      next_state = std::make_pair(next_state.first + 1, next_state.second);
    }
    return std::make_pair(next_state.first - 1, next_state.second);
  }
  // TRY TO GO RIGHT
  next_state = std::make_pair(ship_begin.first, ship_begin.second + 1);
  if (next_state.second < 10 
      && ships_[next_state.first][next_state.second] == TClient::SHIP_PIECE_OK) {
    while (next_state.second < 10 
           && ships_[next_state.first][next_state.second] == TClient::SHIP_PIECE_OK) {
      next_state = std::make_pair(next_state.first, next_state.second + 1);
    }
    return std::make_pair(next_state.first, next_state.second - 1);
  }

  return ship_begin;                  
}

std::vector<Coordinate> TClient::GetShip(const Coordinate ship_begin,
                                         const Coordinate ship_end) {
  std::vector<Coordinate> result;
  bool is_vertical = (ship_begin.second == ship_end.second);
  Coordinate current_state = ship_begin;
  
  while (current_state <= ship_end) {
    result.push_back(std::make_pair(current_state.first, current_state.second));
    
    if (is_vertical) {
      ++current_state.first;
    } else {
      ++current_state.second;
    }
  }
  return result;
}

bool TClient::IsInGrid(const Coordinate coordinate) {
  return -1 < coordinate.first && -1 < coordinate.second 
         && coordinate.first < 10 && coordinate.second < 10;
}

bool TClient::IsInShip(const Coordinate coordinate, 
                       const Coordinate ship_begin, const Coordinate ship_end) {
  if (ship_begin.first == ship_end.first) {  // Horizontal ship
    return coordinate.first == ship_begin.first 
           && coordinate.second >= ship_begin.second 
           && coordinate.second <= ship_end.second;
  } else { // Vertical ship
    return coordinate.second == ship_begin.second 
           && coordinate.first >= ship_begin.first 
           && coordinate.first <= ship_end.first;  
  }           
}

std::vector<Coordinate> TClient::SurroundingOfShip(const Coordinate ship_begin,
                                                   const Coordinate ship_end) {
  std::vector<Coordinate> surrounding_ships;
  
  bool is_vertical = (ship_begin.second == ship_end.second);
  Coordinate current_state = ship_begin;
  
  while (current_state <= ship_end) {
    for (int x_diff = -1; x_diff <= 1; ++x_diff) {
      for (int y_diff = -1; y_diff <= 1; ++y_diff) {
        auto current_neighbour = std::make_pair(current_state.first + x_diff,
                                                current_state.second + y_diff);
        if (IsInGrid(current_neighbour) && !IsInShip(current_neighbour, 
                                                     ship_begin, ship_end)) {
          surrounding_ships.push_back(current_neighbour);
        }
      }
    }
    
    if (is_vertical) {
      ++current_state.first;
    } else {
      ++current_state.second;
    }
  }
  
  std::sort(surrounding_ships.begin(), surrounding_ships.end());
  std::vector <Coordinate> result;
  result.push_back(surrounding_ships.front());
  
  for (int index = 1; index < surrounding_ships.size(); ++index) {
    if (surrounding_ships[index] != surrounding_ships[index - 1]) {
      result.push_back(surrounding_ships[index]);
    }
  }
  surrounding_ships = result;
  
  return surrounding_ships;
}

std::vector<Coordinate> TClient::GetInclusiveShip(Coordinate coordinate) {
  auto begin = FindBeginOfShip(coordinate);
  auto end = FindEndOfShip(coordinate);
  return GetShip(begin, end);
}

// End of CorrectShips realization


// The following function does a change in ships-vector (if necessary)
// and returns MISS, HALF, KILL or WIN.
size_t TClient::GetShooting(const size_t x_coord, const size_t y_coord) {

  // CODE HERE

}


