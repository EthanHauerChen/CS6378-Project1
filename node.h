#ifndef NODE_H
#define NODE_H
#include <unordered_map>
#include <string>
#include <sys/socket.h>
#include <vector>
#include "config_parser.h"

class Node {
    int node_number;
    std::string hostname;
    int port;
    int maxNumber;
    int minPerActive, maxPerActive;
    long minSendDelay;
    bool isActive;
    struct Connection {
        int read_fd;
        int write_fd;
    };
    std::unordered_map<int, Connection> connections; //each key is node number
    int listen_for_connections(int);
    int initiate_connections(int[], std::string[], int[], int);
    int setup(const config& node_info);
    void send_message(int node, int msg_type, std::string msg);
    bool read_nonblocking(int fd, void* buf, size_t count);
    void begin_MAP();

public:
    Node(const config&);
    void become_active();
    void become_passive();
    void send_message(const std::string&);
    friend std::ostream& operator<<(std::ostream& os, const Node& node);
};

#endif