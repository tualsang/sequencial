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
const int MAX_THREADS = 8;    // Maximum number of threads.

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

// Mutex for synchronizing access to shared resources
mutex mtx;

// Function to proces nodes in parallel
void process_nodes(CURL* curl, const vector<string>& nodes, unordered_set<string>& visited, vector<string>& next_level) {
  for (const auto& node : nodes) {
    try {
      for (const auto& neighbor : get_neighbors(fetch_neighbors(curl, node))) {
        lock_guard<mutex> lock(mtx);
        if (!visited.count(neighbor)) {
          visited.insert(neighbor);
          next_level.push_back(neighbor);
        }
      }
    } catch (const ParseException& e) {
      cerr << "Error while fetching neighbors of: " << node << endl;
      throw e;
    }
  }
}

// PARALLEL BFS Traversal Function
vector<vector<string>> parallel_bfs(CURL* curl, const string& start, int depth) {
  vector<vector<string>> levels;
  unordered_set<string> visited;
  
  levels.push_back({start});
  visited.insert(start);

  for (int d = 0;  d < depth; d++) {
    if (debug)
      std::cout<<"starting level: "<<d<<"\n";

    vector<string> current_level = levels[d];
    vector<string> next_level;
    vector<thread> threads;

    int num_nodes = current_level.size();
    int num_threads = (num_nodes < MAX_THREADS)? num_nodes : MAX_THREADS; // Replace std::min with conditional
    //int num_threads = min(MAX_THREADS, numnodes); would also suffice.

    // Distribute nodes among threads
    int nodes_per_thread = num_nodes / num_threads;
    int remaining_nodes = num_nodes % num_threads;

    for (int i = 0; i < num_threads; i++) {
      int start_idx = i * nodes_per_thread;
      int end_idx = start_idx + nodes_per_thread + (i < remaining_nodes ? 1 : 0);
      vector<string> thread_nodes(current_level.begin() + start_idx, current_level.begin() + end_idx);
      threads.emplace_back(process_nodes, curl, thread_nodes, ref(visited), ref(next_level));
    }

    for (auto& t : threads) {
      t.join();
    }

  levels.push_back(next_level);
  
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
    
    for (const auto& n : parallel_bfs(curl, start_node, depth)) {
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
