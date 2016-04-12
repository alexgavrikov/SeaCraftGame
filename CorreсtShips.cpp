/*




This is my realization of CorrectShips. Uploaded Just in case.









*/


#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <fstream>

using Coordinate = std::pair<int, int>;

Coordinate FindEndOfShip(const Coordinate ship_begin,
                         std::vector<std::vector<bool>>& grid) {
  // TRY TO GO DOWN
  auto next_state = std::make_pair(ship_begin.first + 1, ship_begin.second);
  if (next_state.first < 10 
      && grid[next_state.first][next_state.second]) {
    while (next_state.first < 10 
           && grid[next_state.first][next_state.second]) {
      next_state = std::make_pair(next_state.first + 1, next_state.second);
    }
    return std::make_pair(next_state.first - 1, next_state.second);
  }
  // TRY TO GO RIGHT
  next_state = std::make_pair(ship_begin.first, ship_begin.second + 1);
  if (next_state.second < 10 
      && grid[next_state.first][next_state.second]) {
    while (next_state.second < 10 
           && grid[next_state.first][next_state.second]) {
      next_state = std::make_pair(next_state.first, next_state.second + 1);
    }
    return std::make_pair(next_state.first, next_state.second - 1);
  }

  return ship_begin;                  
}

Coordinate FindBeginOfShip(const Coordinate ship_begin,
                         std::vector<std::vector<bool>>& grid) {
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

std::vector<Coordinate> GetShip(const Coordinate ship_begin,
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

bool IsInGrid(const Coordinate coordinate) {
  return -1 < coordinate.first && -1 < coordinate.second 
         && coordinate.first < 10 && coordinate.second < 10;
}

bool IsInShip(const Coordinate coordinate, 
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

std::vector<Coordinate> SurroundingOfShip(const Coordinate ship_begin,
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

bool IsGridCorrect(std::vector<std::vector<bool>>& grid) {
  std::vector<std::vector<bool>> is_visited(10, std::vector<bool>(10));
  std::vector<int> number_of_ships(4);
  
  for (int y_cord = 0; y_cord < 10; ++y_cord) {
    for (int x_cord = 0; x_cord < 10; ++x_cord) {
      if (is_visited[y_cord][x_cord]) {
        continue;
      }
      if (grid[y_cord][x_cord]) {
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
          if (grid[coordinate.first][coordinate.second]) {
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

std::vector<Coordinate> GetInclusiveShip(Coordinate coordinate,
                                         std::vector<std::vector<bool>>& grid) {
  auto begin = FindBeginOfShip(coordinate, grid);
  auto end = FindEndOfShip(coordinate, grid);
  return GetShip(begin, end);
}

int main() {
  std::ifstream ifs("B.in");
   
  std::vector<std::vector<bool>> grid(10, std::vector<bool>(10));
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      bool tmp;
      ifs >> tmp;
      grid[i][j] = tmp;
    }
  }
  ifs.close();
  
  std::cout << IsGridCorrect(grid);
   
  while (true) {
    int x, y;
    std::cin >> x >> y;
    for (auto z : GetInclusiveShip(std::make_pair(x, y), grid)) std::cout << z.first << " " << z.second << "\n";
  }

}
