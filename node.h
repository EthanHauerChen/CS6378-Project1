#ifndef NODE_H
#define NODE_H
#include <unordered_map>
#include <string>
#include <sys/socket.h>
#include <vector>

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
    Node(std::string, int);
    /** make private, called using setup() function */
    int listen_for_connections(int);
    int initiate_connections(int[], std::string[], int[], int);
    /************************************************************ */
    int setup(); //likely need to pass args needed for above 2 functions, maybe as a struct
    void become_active();
    void become_passive();
    void send_message(const std::string&);
    friend std::ostream& operator<<(std::ostream& os, const Node& node);
};

#endif