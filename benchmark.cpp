////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#define CSV_IO_NO_THREAD

#include "arango/mmfiles-geo-index.h"
#include "csv-parser/csv.h"
#include "vptree/vptree.hpp"
#include <sys/time.h>

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <iostream>

struct City {
  std::string country;
  std::string name;
  double latitude;
  double longitude;
};

/// parsing is the same for everyone
static std::vector<City> parseCities(char const* citiesFile) {
  std::vector<City> cities;

  io::CSVReader<4> in(citiesFile);
  //Country,City,Accent City,Region,Latitude,Longitude
  in.read_header(io::ignore_extra_column, "Country", "Accent City", "Latitude", "Longitude");
  std::string country, name; double lat; double lng;
  while(in.read_row(country, name, lat, lng)){
    cities.push_back(City{country, name, lat, lng});
  }
  return cities;
}

static std::vector<City> randomCities(int n) {
  std::vector<City> cities;
  for (int i = 0; i < n; i++) {
    double lat = ((double)(rand() % 180) - 90.0);
    double lng = ((double)(rand() % 360) - 180.0);
    cities.push_back(City{"", "", lat, lng});
  }
  return cities;
}

// should be in meters?
static void benchmarkArango(std::vector<City> const& cities, 
                            int lookups, int nearest) {
  std::cout << std::endl
            << "ArangoDB " << std::endl 
            << "==================" << std::endl; 

  GeoIdx* geoIdx = GeoIndex_new();
  GeoCoordinate gc;

  auto start = std::chrono::steady_clock::now();

  uint64_t i = 0;// data 
  for (City const& c : cities) {
    gc.latitude = c.latitude;
    gc.longitude = c.longitude;
    gc.data = i++;
    GeoIndex_insert(geoIdx, &gc);
  }

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Building index took " << elapsed.count() << " s\n";

  start = std::chrono::steady_clock::now();
  for (int i = 0; i < lookups; i++) {

    // just look near some random city
    City const& target = cities[rand() % cities.size()];
    gc.latitude = target.latitude;
    gc.longitude = target.longitude;//*/
    /*gc.latitude = 50.935173;
    gc.longitude = 6.953101;//*/

    GeoCoordinates* cc = GeoIndex_NearestCountPoints(geoIdx, &gc, nearest);
    if (cc != nullptr) {
      GeoIndex_CoordinatesFree(cc);
    }
  
    /*GeoCoordinates* cc = GeoIndex_PointsWithinRadius(geoIdx, &gc, radius);
    if (cc != nullptr) {
      for (size_t x = 0; x < cc->length; x++) {
        uint64_t i = cc->coordinates[x].data;
        City const& c = cities[i];
        std::cout << "Found " << c.name << std::endl;  
      }
      GeoIndex_CoordinatesFree(cc);
    }*/
  }

  end = std::chrono::steady_clock::now();
  elapsed = end - start;
  std::cout << "Query duration: " << elapsed.count() << " s"<< std::endl;
  
  GeoIndex_free(geoIdx);
}

void benchmarkVPTree(std::vector<City> const& cities, int lookups, int nearest) {
  std::cout << std::endl << std::endl
            << "VP-Tree " << std::endl 
            << "==================" << std::endl; 

  // TODO: count this for insertion?
  std::vector<std::vector<double>> points;
  points.reserve(cities.size());
  for (City const& c : cities) {
    points.push_back({c.latitude, c.longitude});
  }

  auto start = std::chrono::steady_clock::now();
  vpt::VpTree geoIdx(points); // create a tree

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  std::cout << "Building index took " << elapsed.count() << " s\n";

  start = std::chrono::steady_clock::now();  
  for (int i = 0; i < lookups; i++) {
    // just look near some random city
    City const& target = cities[rand() % cities.size()];
    std::vector<double> xy = {target.latitude, target.longitude};
    //std::vector<double> xy = {50.935173, 6.953101}; 

    std::vector<double> distances;
    std::vector<int> indices;
    std::tie(distances, indices) = geoIdx.getNearestNeighbors(xy, nearest);
    /*for (int i : indices) {
      City const& c = cities[i];
      std::cout << "Found " << c.name << std::endl;  
    }*/
  }

  end = std::chrono::steady_clock::now();
  elapsed = end - start;
  std::cout << "Query duration: " << elapsed.count() << " s"<< std::endl;
}

int main(int argc, char ** argv) {
  if (argc < 4) {
    std::cout << "Usage either " << std::endl
               << argv[0] << " -random <num-dataset> <num-lookups> <num-nearest>"
               << std::endl << "or" << std::endl 
               << argv[0] << " -cities <num-lookups> <num-nearest>" << std::endl;
    return 1;
  }

  srand(time(NULL));

  std::vector<City> cities;
  int lookups, nearest;
  if (strcmp(argv[1], "-random") == 0) {
    if (argc != 5) {
      std::cout << "Usage: " << argv[0] 
                 << "-random <num-dataset> <num-lookups> <num-nearest> |"
                 << " -cities <num-lookups> <num-nearest>";
      return 1;
    }

    int dataset = std::max(1, atoi(argv[2]));
    lookups = std::max(1, atoi(argv[3]));
    nearest = std::max(1, atoi(argv[4]));
    cities = randomCities(dataset);

  } else if (strcmp(argv[1], "-cities") == 0) {

    lookups = std::max(1, atoi(argv[2]));
    nearest = std::max(1, atoi(argv[3]));
    cities = parseCities("cities.txt");
    if (cities.empty()) {
      std::cout << "Cannot read cities file";
      return 1;
    }

  } else {
    std::cout << "Specify either '-random' or '-cities'" << std::endl;
    return 1;
  }

  benchmarkArango(cities, lookups, nearest);
  benchmarkVPTree(cities, lookups, nearest);
  
  return 0;
}
