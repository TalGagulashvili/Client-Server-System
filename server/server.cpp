#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <mutex>

std::mutex for_mute;

// Define a structure to represent a graph
struct Graph {
    std::unordered_map< int, std::unordered_set<int> > adjacencyList;
    std::unordered_map< std::string, std::vector<int> > Last_10_calls;
    std::vector<std::string > order_of_10calls;


    // Function to add a vertex to the graph
    void addVertex(int v) {
        if (adjacencyList.find(v) == adjacencyList.end()) {
            adjacencyList[v] = std::unordered_set<int>();
        }
    }

    // Function to add an edge to the graph
    void addEdge(int u, int v) {
        addVertex(u);
        addVertex(v);
        adjacencyList[u].insert(v);
        adjacencyList[v].insert(u); // Assuming it's an undirected graph
    }

    void add_path_to_memory(std::string key , std::vector<int> path){
        if(order_of_10calls.size() < 10) {
            Last_10_calls[key] = path;
            order_of_10calls.push_back(key);
        }
        else{
           std::string very_first_key = order_of_10calls[0];
           Last_10_calls.erase(very_first_key);
           order_of_10calls.erase(order_of_10calls.begin());
           Last_10_calls[key] = path;
           order_of_10calls.push_back(key);
        }
    }


    // Function to find the shortest path between two vertices using BFS
    std::vector<int> shortestPath(int u, int w) {
        for_mute.lock();
        std::unordered_map<int, int> parent; // Map to store parent vertices
        std::queue<int> q;
        std::unordered_set<int> visited;
        std::vector<int> path;


        std::string str_key = std::to_string(u) + " " + std::to_string(w);
        std::string tup_to_find(str_key);
        if( !(Last_10_calls.find(tup_to_find) == Last_10_calls.end()) ){
            //std::cerr << "Its From List!: \n"; // used for debugging
            for_mute.unlock();
            return Last_10_calls[tup_to_find];
        }

        if(adjacencyList.find(u) == adjacencyList.end() or adjacencyList.find(w) == adjacencyList.end()){
            //std::cerr << "u or w not exist\n"; // used for debugging
            add_path_to_memory(tup_to_find, path);
            for_mute.unlock();
            return path;
        }



        // Initialize BFS
        q.push(u);
        visited.insert(u);
        parent[u] = -1; // Parent of starting vertex is set to -1

        // Perform BFS
        while (!q.empty()) {
            int curr = q.front();
            q.pop();

            if (curr == w) {
                break; // Found the destination vertex
            }

            for (int neighbor : adjacencyList[curr]) {
                if (visited.find(neighbor) == visited.end()) {
                    q.push(neighbor);
                    visited.insert(neighbor);
                    parent[neighbor] = curr; // Set the parent of neighbor
                }
            }
        }

        // Reconstruct the shortest path using parent map

        int currentVertex = w;
        while (currentVertex != -1) {
            path.push_back(currentVertex);
            currentVertex = parent[currentVertex];
        }


        // Reverse the path to get it in the correct order
        std::reverse(path.begin(), path.end());

        add_path_to_memory(tup_to_find, path);
        for_mute.unlock();


        return path;
    }
};


// Function to read from a CSV file and build a graph
Graph buildGraphFromCSV(const std::string& filename) {
    Graph graph;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return graph;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;

        // Read two integers representing vertices for an edge
        std::getline(ss, cell, ' ');
        int u = std::stoi(cell);
        std::getline(ss, cell);
        int v = std::stoi(cell);

        // Add the edge to the graph
        graph.addEdge(u, v);
    }

    file.close();
    return graph;
}



void handleClient(int clientSocket, Graph* graph) {
    while(true) {
        char buffer[1024];

        // Read message from client
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived < 0) {
            std::cerr << "Failed to receive message from client.\n";
            close(clientSocket);
            return;
        }
        else if(bytesReceived == 0){
            //std::cerr << "client is disconnected\n"; // used for debugging
            close(clientSocket);
            return;
        }
        std::string message(buffer, bytesReceived);

        // Parse the message to get the two integers
        std::istringstream iss(message);
        int u, v;
        if (!(iss >> u >> v)) {
            std::cerr << "Invalid message format.\n";
            close(clientSocket);
            return;
        }

        // Find the shortest path between the two vertices
        std::vector<int> shortestPath = graph->shortestPath(u, v);

        // Construct response message
        std::ostringstream oss;
        // oss << "Shortest path between " << u << " and " << v << ": "; // used for make nicer
        if (shortestPath.empty()) {
            oss << "No path found.";
        } else {
            for (int vertex: shortestPath) {
                oss << vertex << " ";
            }
        }
        std::string response = oss.str();

        // Send response back to client
        send(clientSocket, response.c_str(), response.length(), 0);
    }
}

int main(int argc, char *argv[]) {
    int port = std::stoi(argv[2]);

    // Create socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error creating socket.\n";
        return 1;
    }

    // Bind socket to port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::cerr << "Error binding socket to port " << port << ".\n";
        close(serverSocket);
        return 1;
    }

    // Reading from CSV file
    std::string CSV_file_name = argv[1];
    Graph graph = buildGraphFromCSV(CSV_file_name);

    // Listen for connections
    if (listen(serverSocket, 10) == -1) {
        std::cerr << "Error listening for connections.\n";
        close(serverSocket);
        return 1;
    }

    //std::cout << "Server listening on port " << port << "...\n"; // used for debugging

    // Accept connections and handle clients
    std::vector<std::thread> clientThreads;
    while (true) {
        sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
        if (clientSocket == -1) {
            std::cerr << "Error accepting connection.\n";
            continue;
        }

        // Handle client in separate thread
        clientThreads.emplace_back(handleClient, clientSocket, &graph);
    }


    // Close server socket
    close(serverSocket);

    return 0;
}
