//
// Created by user on 29-May-25.
//

#ifndef PICONATIVEOPENXRSAMPLES_TCPCLIENTV2_H
#define PICONATIVEOPENXRSAMPLES_TCPCLIENTV2_H

#include "LogUtils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifndef SHARED_H
#define SHARED_H

#pragma pack(push, 1)
struct Message {
    int id;
    uint32_t value;
    char text[32];
};
#pragma pack(pop)

#endif

class TcpClient {
public:
    static void OpenConnection() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            PLOGE("[DEBUGGING] Socket creation failed");
            return;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(54000);

        if (inet_pton(AF_INET, "192.168.100.111", &serverAddr.sin_addr) <=
            0) { // Replace with your server's IP
            PLOGE("[DEBUGGING] Invalid address");
            close(sock);
            return;
        }

        if (connect(sock, (sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
            PLOGE("[DEBUGGING] Connection failed");
            close(sock);
            return;
        }
    }

    static void SendMessage(uint32_t value) {
        Message msg{};

        msg.id = counter++;
        msg.value = value;
        std::snprintf(msg.text, sizeof(msg.text), "Hello %d", msg.id);

        send(sock, &msg, sizeof(msg), 0);
    }

    static void CloseConnection() {
        close(sock);
    }

private:
    static int sock;
    static int counter;
};
int TcpClient::sock = -1;
int TcpClient::counter = 0;


#endif //PICONATIVEOPENXRSAMPLES_TCPCLIENTV2_H
