#ifndef NODE_H
#define NODE_H
#include <unordered_set>
#include <string>

class Node {
    //connection to other nodes, should be in the form of a list of file descriptors but obtained from sockets
    std::unordered_set<int> connection_fds;
    std::string hostname;
    int port;
    bool isActive;


public:
    int listen_for_connections();
    int initiate_connections();
    void become_active();
    void become_passive();
};

#endif