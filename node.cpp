#include "node.h"
#include "config_parser.h"

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
#include <algorithm>
#include <cstdio>


Node::Node(const config& node_info) {
    this->node_number = node_info.node_num;
    this->hostname = node_info.hostname;
    this->port = node_info.port;
    this->maxNumber = node_info.maxNumber;
    this->minPerActive = node_info.minPerActive;
    this->maxPerActive = node_info.maxPerActive;
    this->minSendDelay = node_info.minSendDelay;
    this->isActive = true;
    this->clock = std::vector<int>(node_info.nodes, 0); //initialize all 0s
    if (this->node_number == 0) this->isRecording = true;
    std::cout << "Node setup {\n\t" << 
    "Node number: " << node_number << "\n\t" <<
    "hostname: " << hostname << "\n\t" <<
    "port: " << port << "\n}\n";
    if (setup(node_info) > -1) {
        std::this_thread::sleep_for(std::chrono::seconds(10)); //wait for other processes to finish setup
        begin_MAP();
    }
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
    int sockfd = (this->connections).find(node)->second.write_fd;

    if (msg_type == 0) { //MAP protocol message. ie, application message
        std::string vector_clock = "";
        (this->clock)[node_number]++;
        for (int i = 0; i < (this->clock).size(); i++) {
            vector_clock += std::to_string((this->clock)[i]) + " ";
        }
        std::string message = "0 " + vector_clock;
        std::cout << "message size: " << message.size() << ", message being sent: " << &message[0] << "\nvector clock [" << vector_clock << "]\n" << std::flush;
        int len = message.size();
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(len_net));
        write(sockfd, &message[0], message.size());
    }
    else { //Chandy-Lamport message. ie, control/marker message
        std::string message = "1" + msg;
        std::cout << "message being sent: " << &message[0] << "\n" << std::flush;
        int len = message.size();
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(len_net));
        write(sockfd, &message[0], sizeof(char) * (message.size()));
    }
}

std::string Node::read_msg(int fd) {
    int len = 0;
    read(fd, &len, sizeof(int));
    len = ntohl(len);
    std::cout << "len is " << len << "\n" << std::flush;

    
    if (len > 0) { 
        std::string message(len, '\0');
        int n = read(fd, &message[0], len);
        if (n < 0) {
            perror("read failed");
            return "";
        }
        return message;
    }

    return "";
}

std::vector<int> Node::extract_clock(std::string msg) {
    std::vector<std::string> tokens = split(msg, " ");
    std::vector<int> clock_vals(tokens.size() - 1);
    // std::cout << "message to extract: " << msg << ". is the last char a space: " << msg.back() << " == " << "\" \"" << (msg.back() == ' ') << "\n";
    // std::cout << "tokens size: " << tokens.size() << "\ntokens: " << std::flush;
    for (int i = 1; i < (this->clock).size() + 1; i++) { //first token is msg_type, not part of vector clock
        //std::cout << tokens[i] << ", ";
        clock_vals[i - 1] = std::stoi(tokens[i]);
    }
    //std::cout << "\n" << std::flush;
    
    return clock_vals;
}

void Node::begin_MAP() {
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> num_messages(this->minPerActive, this->maxPerActive);
    std::uniform_int_distribution<> nodes(0, (this->connections).size() - 1);
    std::vector<int> temp_connections;
    for (const auto& pair : this->connections) temp_connections.push_back(pair.first); //in order to random access nodes to send messages to, construct vector of node_nums
    auto past = std::chrono::steady_clock::now();

    int messages_sent = 0;
    while (!(this->terminateProtocol)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - past);
        if (elapsed.count() > 17) return; //if doing nothing for long time, stop executing program
        if (messages_sent < this->maxNumber && (this->isActive)) {
            int num = num_messages(gen);
            for (int i = 0; i < num; i++) {
                int node_num = temp_connections[nodes(gen)];
                send_message(node_num, 0, "");
                messages_sent++;
                std::this_thread::sleep_for(std::chrono::milliseconds(this->minSendDelay));
            }
            this->become_passive();
        }

        //read, handle accordingly based on whether snapshot protocol or MAP protocol
        for (const auto& pair : this->connections) {
            std::string msg = this->read_msg(pair.second.read_fd);
            if (msg.size() == 0) std::cout << "size is 0\n" << std::flush;
            if (msg.size() > 0) { //if successful read of message
                if (msg[0] == '0') {
                    std::vector<int> temp_clock = this->extract_clock(msg);
                    for (int i = 0; i < this->clock.size(); i++) {
                        (this->clock)[i] = std::max((this->clock)[i], temp_clock[i]);
                    }
                    (this->clock)[this->node_number]++;
                    std::cout << "messaeg size: " << msg.size() << ". Node " << this->node_number << " received message [" << msg << "] from: " << pair.first << "\n" << std::flush;
                    std::cout << "vector clock: [";
                    for (int i = 0; i < (this->clock).size(); i++) {
                        std::cout << (this->clock)[i] << " ";
                    }
                    std::cout << "]\n" << std::flush;
                    this->become_active();
                }
                else { //CL protocol
                    if (!(this->isRecording)) {
                        this->isRecording = true;
                        this->parent = pair.first; //parent node, we will send our snapshot and other snapshots to parent
                        std::string message = std::to_string(this->node_number) + " ";

                        //broadcast marker messages along all channels
                        for (const auto& p : this->connections) {
                            send_message(p.first, 1, "");
                        }

                        //send snapshot to parent
                        for (int i = 0; i < (this->clock).size(); i++) {
                            message += std::to_string((this->clock)[i]) + " ";
                        }
                        send_message(this->parent, 1, message);
                    }
                    else if (msg.size() > 1 && this->node_number != 0) { //msg contains vector clock, ie snapshot
                        msg = msg.substr(2, msg.size()); //cut off first character cuz that gets added on by send_message
                        send_message(this->parent, 1, msg);
                    }
                }
            }
        }
        
        this->become_passive();
    }
}

std::ostream& operator<<(std::ostream& os, const Node& node) {
    os << "Node(hostname=" << node.hostname << ", port=" << node.port << ")\n";
    return os;
}