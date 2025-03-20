#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <vertex_u> <vertex_v>\n";
        return 1;
    }

    const char *serverIp = argv[1];
    int serverPort = std::stoi(argv[2]);
    int vertex_u = std::stoi(argv[3]);
    int vertex_v = std::stoi(argv[4]);

    // Create socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating socket.\n";
        return 1;
    }

    // Connect to server
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIp, &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid address or address not supported.\n";
        close(clientSocket);
        return 1;
    }
    if (connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::cerr << "Connection failed.\n";
        close(clientSocket);
        return 1;
    }

    // Send message to server
    std::string message = std::to_string(vertex_u) + " " + std::to_string(vertex_v);
        if (send(clientSocket, message.c_str(), message.length(), 0) == -1) {
            std::cerr << "Failed to send message to server.\n";
            close(clientSocket);
            return 1;
        }


        // Receive and print response from server
        char buffer[1024];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Failed to receive response from server.\n";
            close(clientSocket);
            return 1;
        }
        std::cerr << std::string(buffer, bytesReceived) << "\n";


    // Close socket
    close(clientSocket);

    return 0;
}