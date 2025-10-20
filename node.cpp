#include "node.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <fcntl.h>

Node::Node(const config& node_info) {
    this->node_number = node_info.node_num;
    this->hostname = node_info.hostname;
    this->port = node_info.port;
    this->maxNumber = node_info.maxNumber;
    this->minPerActive = node_info.minPerActive;
    this->maxPerActive = node_info.maxPerActive;
    this->minSendDelay = node_info.minSendDelay;
    std::cout << "Node setup {\n\t" << 
    "Node number: " << node_number << "\n\t" <<
    "hostname: " << hostname << "\n\t" <<
    "port: " << port << "\n}\n";
    if (setup(node_info) > -1)
        begin_MAP();
}

int Node::listen_for_connections(int num_neighbors) { 
    int sockfd = -1;
    struct sockaddr_in address;

    /* create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd <= 0) {
        fprintf(stderr, "error: unable to create socket\n");
        return -1;
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "error: unable to bind socket to port %d\n", port);
        return -2;
    }

    /* listen on port */
    if (listen(sockfd, SOMAXCONN) < 0) {
        fprintf(stderr, "error: unable to listen on port\n");
        return -3;
    }
    printf("ready and listening on %s:%d\n", hostname.c_str(), port);
    
    int num_connections = 0;
    while (num_connections < num_neighbors)
    {
        /* accept incoming connections */
        int connection_fd = accept(sockfd, NULL, NULL);
        if (connection_fd > 0) {
            num_connections++;
            /* determine which node has connected */
            int node;
            read(connection_fd, &node, sizeof(int));
            std::cout << "Server " << node_number << " connected with Client " << node << "\n";

            /* change read_fd to be nonblocking */
            int flags = fcntl(connection_fd, F_GETFL, 0);
            fcntl(connection_fd, F_SETFL, flags | O_NONBLOCK);

            /* place associated socket fd into connections hash table */
            if (connections.find(node) == connections.end()) { //same as connections.contains(node), but contains only available for c++20 
                connections.insert({node, {connection_fd, -1}});
            }
            else {
                connections.find(node)->second.read_fd = connection_fd; //find returns an iterator that points to std::pair, and the struct is the second of the pair, hence second.read_fd
            }
        }
    }
    
    return 1; //success
}

/** obtain list of hostnames and ports, create multiple sockets for each and attempt to connecdt */
int Node::initiate_connections(int* nodes, std::string* hostnames, int* ports, int size) {
    for (int i = 0; i < size; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in address;
        struct hostent * host;
        
        if (sockfd <= 0) {
            fprintf(stderr, "error: unable to create socket\n");
            return -1;
        }

        /* connect to server */
        address.sin_family = AF_INET;
        address.sin_port = htons(ports[i]);
        host = gethostbyname(hostnames[i].c_str());
        std::cout << "Client " << node_number << " connecting to hostname: " << hostnames[i] << ", port: " << ports[i] << "\n" << std::flush;
        
        if (!host) {
            fprintf(stderr, "error: unknown host %s\n", hostnames[i].c_str());
            return -2;
        }
        memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
        int counter = 0; //quit connection attempt after 30 times
        while (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) != 0) {
            fprintf(stderr, "error: cannot connect to host %s\n", hostnames[i].c_str());
            counter++;
            if (counter > 15) return -3;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::cout << "Client " << node_number << " connected to Server " << hostnames[i] << ":" << ports[i] << "\n";

        /* place associated socket fd into connections hash table */
        if (connections.find(nodes[i]) == connections.end()) { //same as connections.contains(node), but contains only available for c++20 
            connections.insert({nodes[i], {-1, sockfd}});
        }
        else {
            connections.find(nodes[i])->second.write_fd = sockfd;
        }

        /* inform server which node has made the connection */
        write(sockfd, &node_number, sizeof(int));
    }

    return 1; //success
}

int Node::setup(const config& node_info) {
    int size = node_info.neighbors.size();
    //variable length arrays are not allowed in C++ standard so the following are vectors instead
    std::vector<int> neighbors(size);
    std::vector<std::string> hostnames(size);
    std::vector<int> ports(size);
    for (int i = 0; i < size; i++) {
        neighbors[i] = node_info.neighbors[i].nodenum;
        hostnames[i] = node_info.neighbors[i].hostname;
        ports[i] = node_info.neighbors[i].port;
    }

    std::thread l(&Node::listen_for_connections, this, size); //runnable func needs to be non-static, therefore use pointer to function as arg0, pointer to obj the function belongs to as arg1
    std::thread c(&Node::initiate_connections, this, neighbors.data(), hostnames.data(), ports.data(), size);
    l.join();
    c.join();
    return 0;
}

void Node::become_active() { isActive = true; }
void Node::become_passive() { isActive = false; }

void Node::send_message(int node, int msg_type, std::string msg) {
    if (msg_type == 0) { //MAP protocol message. ie, application message
        int sockfd = (this->connections).find(node)->second.write_fd;
        int message = htonl(0);
        write(sockfd, &message, sizeof(int));
    }
    else {
        //Chandy-Lamport message. ie, control message
    }
}

void Node::begin_MAP() {
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> num_messages(this->minPerActive, this->maxPerActive);
    std::uniform_int_distribution<> nodes(0, (this->connections).size() - 1);
    std::vector<int> temp_connections();
    for (const auto& pair : this->connections) temp_connections.push_back(pair.first); //in order to random access nodes to send messages to, construct vector of node_nums

    int messages_sent = 0;
    while (messages_sent < this->maxNumber) {
        if ((this->isActive)) {
            int num = num_messages(gen);
            for (int i = 0; i < num_messages; i++) {
                int node_num = temp_connections[nodes(gen)];
                send_message(node_num, 0, "");
                messages_sent++;
                std::this_thread::sleep_for(std::chrono::milliseconds(this->minSendDelay));
            }
            this->become_passive();
        }
        else {
            //for each connection, check the read_fd to see if application message was received

        }
    }
}

std::ostream& operator<<(std::ostream& os, const Node& node) {
    os << "Node(hostname=" << node.hostname << ", port=" << node.port << ")\n";
    return os;
}