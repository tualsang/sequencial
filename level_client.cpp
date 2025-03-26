#include <iostream>
#include <string>
#include <queue>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <stdexcept>
#include "rapidjson/error/error.h"
#include "rapidjson/reader.h"

#include <thread>   // Thread
#include <mutex>    // Mutex

const int MAX_THREADS = 16; // Adjustable based on system capabilities
std::unordered_set<std::string> visited;
std::vector<std::string> nextLevelNodes;
std::mutex mtx; // Mutex for synchronizing access to shared variables

struct ParseException : std::runtime_error, rapidjson::ParseResult {
    ParseException(rapidjson::ParseErrorCode code, const char* msg, size_t offset) : 
        std::runtime_error(msg), 
        rapidjson::ParseResult(code, offset) {}
};

#define RAPIDJSON_PARSE_ERROR_NORETURN(code, offset) \
    throw ParseException(code, #code, offset)

#include <rapidjson/document.h>
#include <chrono>

using namespace std;
using namespace rapidjson;

bool debug = false;

// Updated service URL
const string SERVICE_URL = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

// Function to HTTP ecnode parts of URLs. for instance, replace spaces with '%20' for URLs
string url_encode(CURL* curl, string input) {
  char* out = curl_easy_escape(curl, input.c_str(), input.size());
  string s = out;
  curl_free(out);
  return s;
}

// Callback function for writing response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Function to fetch neighbors using libcurl with debugging
string fetch_neighbors(CURL* curl, const string& node) {

    string url = SERVICE_URL + url_encode(curl, node);
    string response;

    if (debug)
      cout << "Sending request to: " << url << endl;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Verbose Logging

    // Set a User-Agent header to avoid potential blocking by the server
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: C++-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        cerr << "CURL error: " << curl_easy_strerror(res) << endl;
    } else {
      if (debug)
        cout << "CURL request successful!" << endl;
    }

    // Cleanup
    curl_slist_free_all(headers);

    if (debug) 
      cout << "Response received: " << response << endl;  // Debug log

    return (res == CURLE_OK) ? response : "{}";
}

// Function to parse JSON and extract neighbors
vector<string> get_neighbors(const string& json_str) {
    vector<string> neighbors;
    try {
      Document doc;
      doc.Parse(json_str.c_str());
      
      if (doc.HasMember("neighbors") && doc["neighbors"].IsArray()) {
        for (const auto& neighbor : doc["neighbors"].GetArray())
	  neighbors.push_back(neighbor.GetString());
      }
    } catch (const ParseException& e) {
      std::cerr<<"Error while parsing JSON: "<<json_str<<std::endl;
      throw e;
    }
    return neighbors;
}

// BFS Traversal Function
vector<vector<string>> bfs(CURL* curl, const string& start, int depth) {
  vector<vector<string>> levels;
  levels.push_back({start});
  visited.insert(start);

  for (int d = 0; d < depth; d++) {
      if (debug)
          cout << "starting level: " << d << "\n";
      nextLevelNodes.clear(); // Reset for the next level

      int numNodes = levels[d].size();
      int numThreads = min(numNodes, MAX_THREADS);

      thread threads[numThreads];
      int nodesPerThread = numNodes / numThreads;
      int remainingNodes = numNodes % numThreads;

      int startNode = 0;
      for (int t = 0; t < numThreads; t++) {
          int endNode = startNode + nodesPerThread + (t < remainingNodes? 1 : 0);
          threads[t] = thread([curl, &levels, d, startNode, endNode, &visited, &nextLevelNodes, &mtx]() {
              for (int i = startNode; i < endNode; i++) {
                  try {
                      string neighborsJson = fetch_neighbors(curl, levels[d][i]);
                      vector<string> neighbors = get_neighbors(neighborsJson);
                      {
                          lock_guard<mutex> lock(mtx);
                          for (const auto& neighbor : neighbors) {
                              if (visited.find(neighbor) == visited.end()) {
                                  visited.insert(neighbor);
                                  nextLevelNodes.push_back(neighbor);
                              }
                          }
                      }
                  } catch (const ParseException& e) {
                      cerr << "Error while fetching neighbors of: " << levels[d][i] << endl;
                      throw e;
                  }
              }
          });
          startNode = endNode;
      }

      for (auto& th : threads) th.join();

      levels.push_back(nextLevelNodes);
  }

  return levels;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <node_name> <depth>\n";
        return 1;
    }

    string start_node = argv[1];     // example "Tom%20Hanks"
    int depth;
    try {
        depth = stoi(argv[2]);
    } catch (const exception& e) {
        cerr << "Error: Depth must be an integer.\n";
        return 1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        cerr << "Failed to initialize CURL" << endl;
        return -1;
    }


    const auto start{std::chrono::steady_clock::now()};
    
    
    for (const auto& n : bfs(curl, start_node, depth)) {
      for (const auto& node : n)
	cout << "- " << node << "\n";
      std::cout<<n.size()<<"\n";
    }
    
    const auto finish{std::chrono::steady_clock::now()};
    const std::chrono::duration<double> elapsed_seconds{finish - start};
    std::cout << "Time to crawl: "<<elapsed_seconds.count() << "s\n";
    
    curl_easy_cleanup(curl);

    
    return 0;
}
