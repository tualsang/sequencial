#include <iostream>
#include <string>
#include <queue>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <stdexcept>
#include "rapidjson/document.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>

using namespace std;
using namespace rapidjson;

// Define constants
const int MAX_THREADS = 8; // Maximum number of threads
const string SERVICE_URL = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

// Define a struct for parse exceptions
struct ParseException : public runtime_error, public ParseResult {
    ParseException(ParseErrorCode code, const char* msg, size_t offset)
        : runtime_error(msg), ParseResult(code, offset) {}
};

// Define a function to URL encode a string
string url_encode(CURL* curl, const string& input) {
    char* out = curl_easy_escape(curl, input.c_str(), input.size());
    string s = out;
    curl_free(out);
    return s;
}

// Define a callback function for writing response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// Define a function to fetch neighbors using libcurl
string fetch_neighbors(CURL* curl, const string& node) {
    string url = SERVICE_URL + url_encode(curl, node);
    string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res!= CURLE_OK) {
        throw runtime_error("CURL error: " + string(curl_easy_strerror(res)));
    }

    return response;
}

// Define a function to parse JSON and extract neighbors
vector<string> get_neighbors(const string& json_str) {
    vector<string> neighbors;
    try {
        Document doc;
        doc.Parse(json_str.c_str());

        if (doc.HasMember("neighbors") && doc["neighbors"].IsArray()) {
            for (const auto& neighbor : doc["neighbors"].GetArray()) {
                neighbors.push_back(neighbor.GetString());
            }
        }
    } catch (const ParseException& e) {
        throw runtime_error("Error while parsing JSON: " + json_str);
    }
    return neighbors;
}

// Define a mutex for synchronizing access to shared resources
mutex mtx;

// Define a function to process nodes in parallel
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
        } catch (const exception& e) {
            cerr << "Error while fetching neighbors of: " << node << endl;
            throw;
        }
    }
}

// Define a function for parallel BFS traversal
vector<vector<string>> parallel_bfs(CURL* curl, const string& start, int depth) {
    vector<vector<string>> levels;
    unordered_set<string> visited;

    levels.push_back({start});
    visited.insert(start);

    for (int d = 0; d < depth; d++) {
        vector<string> current_level = levels[d];
        vector<string> next_level;
        vector<thread> threads;

        int num_nodes = current_level.size();
        int num_threads = (num_nodes < MAX_THREADS)? num_nodes : MAX_THREADS;

        int nodes_per_thread = num_nodes / num_threads;
        int remaining_nodes = num_nodes % num_threads;

        for (int i = 0; i < num_threads; i++) {
            int start_idx = i * nodes_per_thread;
            int end_idx = start_idx + nodes_per_thread + (i < remaining_nodes? 1 : 0);
            vector<string> thread_nodes(current_level.begin() + start_idx, current_level.begin() + end_idx);
            threads.emplace_back(process_nodes, curl, thread_nodes, ref(visited), ref(next_level));
        }

        for (auto& t : threads) {
            t.join();
        }

        levels.push_back(next_level);
    }

    return levels;
}

int main(int argc, char* argv[]) {
    if (argc!= 3) {
        cerr << "Usage: " << argv[0] << " <node_name> <depth>\n";
        return 1; 
    }

    string start_node = argv[1];
    int depth;
    try {
        depth = stoi(argv[2]);
    } catch (const exception& e) {
        cerr << "Error: Depth must be an integer.\n";
        return 1;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        cerr << "Failed to initialize CURL\n";
        return -1;
    }

    try {
        const auto start{chrono::steady_clock::now()};

        for (const auto& n : parallel_bfs(curl, start_node, depth)) {
            for (const auto& node : n) {
                cout << "- " << node << "\n";
            }
            cout << n.size() << "\n";
        }

        const auto finish{chrono::steady_clock::now()};
        const chrono::duration<double> elapsed_seconds{finish - start};
        cout << "Time to crawl: " << elapsed_seconds.count() << "s\n";
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    curl_easy_cleanup(curl);

    return 0;
}