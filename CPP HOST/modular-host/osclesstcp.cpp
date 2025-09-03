#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <cstring>
#include "shared.cpp"

#pragma comment(lib, "ws2_32.lib")

#define PORT 9000
#define BUFFER_SIZE 1024

class TcpHost
{
public:
    TcpHost() : listenSocket(INVALID_SOCKET), clientSocket(INVALID_SOCKET) {}


    bool Start(uint16_t port = 54000)
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            std::cerr << "[SERVER] WSAStartup failed: " << result << "\n";
            return false;
        }

        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == INVALID_SOCKET)
        {
            std::cerr << "[SERVER] Socket creation failed\n";
            WSACleanup();
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
        serverAddr.sin_port = htons(port);

        if (bind(listenSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cerr << "[SERVER] Bind failed\n";
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            std::cerr << "[SERVER] Listen failed\n";
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        std::cout << "[SERVER] Listening on port " << port << "...\n";
        return true;
    }

    void AcceptClient()
    {
        while (true)
        {
            clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET)
            {
                std::cerr << "[SERVER] Accept failed\n";
                closesocket(listenSocket);
                WSACleanup();
                return;
            }
            std::cout << "[SERVER] Client connected!\n";

            std::thread(&TcpHost::HandleClient, this).detach();
        }
    }

    void Stop()
    {
        if (clientSocket != INVALID_SOCKET)
        {
            closesocket(clientSocket);
        }
        if (listenSocket != INVALID_SOCKET)
        {
            closesocket(listenSocket);
        }

        WSACleanup();
        std::cout << "[SERVER] Stopped.\n";
    }

private:
    void HandleClient()
    {
        Message msg{};
        while (true)
        {
            int bytesReceived = recv(clientSocket, (char *)&msg, sizeof(msg), 0);
            if (bytesReceived <= 0)
            {
                std::cout << "[SERVER] Client disconnected.\n";
                break;
            }

            if (msg.etData.leftEyeMiddleCanthusUvX == 0 && msg.etData.leftEyeMiddleCanthusUvY == 0 && msg.etData.leftEyeOpenness == 0 && msg.etData.leftEyePupilDilation == 0
                && msg.etData.rightEyeMiddleCanthusUvX == 0 && msg.etData.rightEyeMiddleCanthusUvY == 0 && msg.etData.rightEyeOpenness == 0 && msg.etData.rightEyePupilDilation == 0)
                continue;

            SendOscData(msg.etData);

            // std::cout << "[SERVER] Received: ID=" << msg.id
            //         //   << " Value=" << msg.value
            //         //   << " Text=" << msg.text
            //           << " Left: " << msg.etData.leftEyeMiddleCanthusUvX << ":" << msg.etData.leftEyeMiddleCanthusUvY << ":" << msg.etData.leftEyeOpenness << ":" << msg.etData.leftEyePupilDilation
            //           << " Right: " << msg.etData.rightEyeMiddleCanthusUvX << ":" << msg.etData.rightEyeMiddleCanthusUvY << ":" << msg.etData.rightEyeOpenness << ":" << msg.etData.rightEyePupilDilation
            //           << "\n";

            // std::cout << msg.etData.leftEyeMiddleCanthusUvX << "," << msg.etData.leftEyeMiddleCanthusUvY << "," << msg.etData.leftEyeOpenness << "," << msg.etData.leftEyePupilDilation << ","
            //           << msg.etData.rightEyeMiddleCanthusUvX << "," << msg.etData.rightEyeMiddleCanthusUvY << "," << msg.etData.rightEyeOpenness << "," << msg.etData.rightEyePupilDilation
            //           << "\n";
        }
        closesocket(clientSocket);
    }

    SOCKET listenSocket;
    SOCKET clientSocket;
    SOCKET oscSocket;
    sockaddr_in target;
};

// Example main for testing
int main()
{
    TcpHost server;
    if (!server.Start())
        return 1;

    server.StartOscSocket();
    server.AcceptClient();

    // Keep running until user stops it
    std::string cmd;
    while (cmd != "quit")
    {
        std::getline(std::cin, cmd);
    }

    server.Stop();
    return 0;
}