#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <cstring>
#include "shared.cpp"

#pragma comment(lib, "ws2_32.lib")

#define PORT 9000
#define BUFFER_SIZE 1024

class OscServer
{
public:
    OscServer() : oscSocket(INVALID_SOCKET) {}

private:
    // Helper: pad string to multiple of 4 bytes (OSC requirement)
    int pad4(char *buffer, const char *str)
    {
        int len = std::strlen(str) + 1;
        int paddedLen = ((len + 3) / 4) * 4;
        std::memcpy(buffer, str, len);
        for (int i = len; i < paddedLen; ++i)
            buffer[i] = '\0';
        return paddedLen;
    }

    // Helper: append float in network byte order (big-endian)
    int appendFloat(char *buffer, float value)
    {
        uint32_t net;
        std::memcpy(&net, &value, sizeof(float));
        net = htonl(net); // simple big-endian conversion
        std::memcpy(buffer, &net, sizeof(float));
        return sizeof(float);
    }

    // Send OSC message
    void sendOSC(SOCKET sock, sockaddr_in &addr, const char *address, float *values, int count)
    {
        char buffer[BUFFER_SIZE];
        int offset = 0;

        offset += pad4(buffer + offset, address);

        // Type tag string
        char types[32] = ",";
        for (int i = 0; i < count; ++i)
            types[i + 1] = 'f';
        types[count + 1] = '\0';
        offset += pad4(buffer + offset, types);

        for (int i = 0; i < count; ++i)
            offset += appendFloat(buffer + offset, values[i]);

        sendto(sock, buffer, offset, 0, (sockaddr *)&addr, sizeof(addr));
    }

public:
    void StartOscSocket()
    {
        oscSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (oscSocket == INVALID_SOCKET)
        {
            std::cerr << "[OSC] Socket error\n";
            return;
        }

        target = sockaddr_in{};
        target.sin_family = AF_INET;
        target.sin_port = htons(PORT);
        InetPtonA(AF_INET, "127.0.0.1", &target.sin_addr);
    }

    void SendOscData(EtData etData) {
        while (true)
        {
            float eyesClosed[2] = {etData.leftEyeOpenness, etData.rightEyeOpenness};
            float vec[4] = {etData.leftEyeMiddleCanthusUvX, etData.leftEyeMiddleCanthusUvY,
                            etData.rightEyeMiddleCanthusUvX, etData.rightEyeMiddleCanthusUvY};

            sendOSC(oscSocket, target, "/tracking/eye/EyesClosedAmount", eyesClosed, 2);
            sendOSC(oscSocket, target, "/tracking/eye/LeftRightVec", vec, 4);
        }

    }

    void Stop() {
    if (oscSocket != INVALID_SOCKET) 
        {
            closesocket(oscSocket);
        }   
    }


private:
    SOCKET oscSocket;
    sockaddr_in target;

};