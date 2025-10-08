#include "node.h"
#include <thread>

void list(Node a, int num_neighbors) { //wrapper around Node::listen_for_connections
    a.listen_for_connections(num_neighbors);
}

void con(Node a, int* nodes, std::string* hostnames, int* ports, int size) { //wrapper around Node::initiate_connections
    a.initiate_connections(nodes, hostnames, ports, size);
}

int main(int argc, char** argv) {
    Node a{"localhost", std::stoi(argv[1])};
    int nodes[] = {1};
    std::string hostnames[] = {"localhost"};
    int ports[1];
    if (argv[1] == "8000") {
        ports[0] = 8001;
    }
    else ports[0] = 8000;

    std::thread l(list, a,  1);
    std::thread c(con, a, nodes, hostnames, ports, 1);

    l.join();
    c.join();

    // a.listen_for_connections(1);
    // a.initiate_connections(nodes, hostnames, ports, 1);
    return 0;
}