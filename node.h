#ifndef NODE_H
#define NODE_H
#include <unordered_set>

class Node {
    //connection to other nodes, should be in the form of a list of file descriptors but obtained from sockets
    std::unordered_set<int> connection_fds;

    template <typename Container> 
    int initiateConnections(const auto& /*TODO*/)
};

#endif