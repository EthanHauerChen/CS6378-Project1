#ifndef NODE_H
#define NODE_H
#include <unordered_set>
#include <string>
#include <sys/socket.h>
#include <vector>

class Node {
    std::string hostname;
    int port;
    bool isActive;

    struct Connection {
        int fd;
        struct sockaddr_in address;
    }

    std::unordered_set<Connection> connections;

public:
    Node(std::string hostname, int port);
    int listen_for_connections();
    int initiate_connections();
    void become_active();
    void become_passive();
};

#endif