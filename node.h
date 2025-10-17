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
    bool isActive;
    struct Connection {
        int read_fd;
        int write_fd;
    };
    std::unordered_map<int, Connection> connections;

public:
    Node(const& config);
    /** make private, called using setup() function */
    int listen_for_connections(int);
    int initiate_connections(int[], std::string[], int[], int);
    /************************************************************ */
    int setup(const config& node_info); //likely need to pass args needed for above 2 functions, maybe as a struct
    void become_active();
    void become_passive();
    void send_message(const std::string&);
    friend std::ostream& operator<<(std::ostream& os, const Node& node);
};

#endif