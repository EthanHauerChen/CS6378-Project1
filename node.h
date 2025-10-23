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
    bool terminateProtocol = false;
    std::vector<int> clock;

    /* for implementing Chandy-Lamport global snapshot protocol */
    //local state of the node is the vector clock at that time
    std::vector<std::vector<string>> channel_state; //consists of all messages recorded since turning "red/active (not the same as isActive which is part of MAP protocol)"
    bool isRecording = false; //whether the node should be recording channel state or not
    int parent; //node from which received the first marker message

    struct Connection {
        int read_fd;
        int write_fd;
    };
    std::unordered_map<int, Connection> connections; //each key is node number
    int listen_for_connections(int);
    int initiate_connections(int[], std::string[], int[], int);
    int setup(const config& node_info);
    void send_message(int node, int msg_type, std::string msg);
    bool read_nonblocking(int fd, std::string& buf, size_t count);
    std::vector<int> extract_clock(std::string); //gets the clock values from a message
    void begin_MAP();
    void take_snapshot();

public:
    Node(const config&);
    void become_active();
    void become_passive();
    void send_message(const std::string&);
    friend std::ostream& operator<<(std::ostream& os, const Node& node);
};

#endif