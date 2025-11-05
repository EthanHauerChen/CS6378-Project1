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
#include <unordered_set>
#include <fstream>

void Node::debug_msg(int other, bool sending, std::string msg) {
    std::string send_or_rcv;
    if (sending) send_or_rcv = "SEND";
    else send_or_rcv = "RECEIVE";
    std::cout << "{" << this->node_number << " " <<  send_or_rcv << " message to/from " 
    << other << "} MSG = " << msg << "\n" << std::flush;
}

int Node::get_node_num(int fd) { //returns corresponding node number of a given file descriptor
    for (const auto& pair : this->connections) {
        if (pair.second.read_fd == fd || pair.second.write_fd == fd)
            return pair.first;
    }
    return -1;
}

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
    if (this->node_number == 0) {
        this->isRecording = true;
        snapshot = std::vector<std::vector<int>>(connections.size(), std::vector<int>(clock.size(), 0));
    }
    std::cout << "Node setup {\n\t" << 
    "Node number: " << node_number << "\n\t" <<
    "hostname: " << hostname << "\n\t" <<
    "port: " << port << "\n}\n";
    if (setup(node_info) > -1) {
        do_MAP();
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
            std::cout << "Server " << node_number << " connected with Client " << node << "\n" << std::flush;

            //make nonblocking
            fcntl(connection_fd, F_SETFL, O_NONBLOCK);

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
        std::cout << "Client " << node_number << " connected to Server " << hostnames[i] << ":" << ports[i] << "\n" << std::flush;

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
        //std::cout << "message size: " << message.size() << ", message being sent: " << &message[0] << "\nvector clock [" << vector_clock << "]\n" << std::flush;
        debug_msg(node, true, message);
        int len = message.size();
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(len_net));
        //std::cout << "MAP message. Node " << this->node_number << " wrote ||||||len=||||||" << len << " to Node " << node << " connection\n" << std::flush;
        write(sockfd, &message[0], message.size());
    }
    else if (msg_type == 1) { //Chandy-Lamport message. ie, control/marker message
        std::string message = "1" + msg;
        //std::cout << "message being sent: " << &message[0] << "\n" << std::flush;
        debug_msg(node, true, message);
        int len = message.size();
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(len_net));
        //std::cout << "CL message. Node " << this->node_number << " wrote ||||||len=||||||" << len << " to Node " << node << " connection\n" << std::flush;
        write(sockfd, &message[0], sizeof(char) * (message.size()));
    }
    else if (msg_type == 2) { //termination message
        // int len = 1;
        // int msg = 2;
        // int len_net = htonl(len);
        // int msg_net = htonl(msg);
        char msg = '2';
        int len = sizeof(char);
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(int));
        //std::cout << "Termination message. Node " << this->node_number << " wrote ||||||len=||||||" << len << " to Node " << node << " connection\n" << std::flush;
        debug_msg(node, true, std::string(1, msg));
        write(sockfd, &msg, sizeof(char));
    }
    else if (msg_type == 3) { //start message
        char msg = '3';
        int len = sizeof(char);
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(int));
        //std::cout << "Termination message. Node " << this->node_number << " wrote ||||||len=||||||" << len << " to Node " << node << " connection\n" << std::flush;
        debug_msg(node, true, std::string(1, msg));
        write(sockfd, &msg, sizeof(char));
    }
    else if (msg_type == 4) { //start message ACK
        char msg = '4';
        int len = sizeof(char);
        int len_net = htonl(len);
        write(sockfd, &len_net, sizeof(int));
        //std::cout << "Termination message. Node " << this->node_number << " wrote ||||||len=||||||" << len << " to Node " << node << " connection\n" << std::flush;
        debug_msg(node, true, std::string(1, msg));
        write(sockfd, &msg, sizeof(char));
    }
}

std::string Node::read_msg(int fd) {
    int len = 0;
    int returnval = read(fd, &len, sizeof(int));
    len = ntohl(len);
    size_t total_read = 0;
    char buffer[len];
    //std::cout << "len is " << len << "\n" << std::flush;
    if (returnval == 0) { //socket connection closed, abort
        int nodenum = -1; 
        for (const auto& p : this->connections) { //obtain nodenum
            if (p.second.read_fd == fd) {
                nodenum = p.first;
                break;
            }
            send_message(p.first, 2, ""); //send termination messages to neighbors
        }
        //std::cerr << "socket connection with " << nodenum << " closed. aborting" << " len is " << len << "\n" << std::flush;
        debug_msg(get_node_num(fd), false, "socket connection closed. aborting. len is " + std::to_string(len));
        this->destroy = true;
        return "2"; // have MAP protocol handle the termination, sending termination messages to all neighbors
    }
    else if (returnval < 0) {
        // no data to read right now
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ""; // no data available
        if (errno == EINTR)
            return read_msg(fd); // interrupted, retry
        //std::cout << "read length failed\n" << std::flush;
        return "";
    }

    while (total_read < len) {
        ssize_t n = read(fd, buffer + total_read, len - total_read);
        if (n == 0) {
            return "2";
        } else if (n < 0) {
            if (errno == EINTR)
                continue; //interrupted, try again
            else
                return ""; //error
        }
        total_read += n;
    }
    //std::cout << "char buffer[]: ";
    for (int i = 0; i < len; i++) {
        std::cout << buffer[i];
    }
    //std::cout << "\nstring message: ";
    std::string message(buffer, total_read);
    //std::cout << message << "\n" << std::flush;

    if (len == 1 && message[0] == '2') { //if termination message
        //std::cout << "connection closed, terminating program\n" << std::flush;
        debug_msg(get_node_num(fd), false, "connection closed, terminating program");
        this->destroy = true;
        return "2";
    }

    debug_msg(get_node_num(fd), false, "successful read: " + message);
    return message;
}

std::vector<int> Node::extract_clock(std::string msg) {
    std::vector<std::string> tokens = split(msg, " ");
    std::vector<int> clock_vals((this->clock).size());
    // std::cout << "message to extract: " << msg << ". is the last char a space: " << msg.back() << " == " << "\" \"" << (msg.back() == ' ') << "\n";
    // std::cout << "tokens size: " << tokens.size() << "\ntokens: " << std::flush;
    if (tokens[0] == "0") {
        for (int i = 1; i < (this->clock).size() + 1; i++) { //first token is msg_type, not part of vector clock
            //std::cout << tokens[i] << ", ";
            clock_vals[i - 1] = std::stoi(tokens[i]);
        }
    }
    else {
        std::cout << "extract clock:  ";
        for (int i = 2; i < (this->clock).size() + 1; i++) {
            std::cout << tokens[i] << ", ";
            clock_vals[i - 2] = std::stoi(tokens[i]);
        }
    }
    std::cout << "\n" << std::flush;
    
    return clock_vals;
}

bool Node::start_MAP() { //startup function, consists of ensuring all nodes are setup before beginning MAP protocol
    //broadcast start message
    for (const auto& pair : this->connections) {
        send_message(pair.first, 3, "");
    }
    std::cout << "Node " << this->node_number << " broadcast to all neighbors\n" << std::flush;

    //wait for start message ACK
    int num_started = 0;
    std::unordered_set<int> heard_back; //nodes that we have heard back from
    auto past = std::chrono::steady_clock::now(); //if process takes too long, abort
    
    std::cout << "Node " << this->node_number << " neighbors: ";
    for (const auto& pair : this->connections) {
        std::cout << pair.first << " ";
    }
    std::cout << "\n" << std::flush;
    
    while (heard_back.size() < (this->connections).size()) {
        for (const auto& pair : this->connections) {
            std::string msg = read_msg(pair.second.read_fd);
            if (msg.empty()) continue;
            else if (msg[0] == '3') {
                heard_back.emplace(pair.first);
                std::cout << "Node " << this->node_number << " heard back from neighbor " << pair.first << "\n" << std::flush;
            }
            else if (msg == "2") return false;
        }
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - past).count() > 13) {
            std::cerr << "Node " << this->node_number << " failure to hear back from all neighbors, abort\n";
            return false; 
        }
    }
    std::cout << "Node " << this->node_number << " start message from all neighbors, beginning MAP protocol\n" << std::flush;
    return true;

    // //node 0 starts by broadcasting a begin message
    // if (this->node_number == 0) {
    //     for (const auto& pair : this->connections) {
    //         send_message(pair.first, 3, "");
    //     }
    // }

    // //wait to receive begin message, record the node that it was received from
    // bool begin_rcvd = false;
    // int parent_node = -1; //node num of the first node begin message received from
    // auto past = std::chrono::steady_clock::now(); //if process takes too long, abort
    // while (!begin_rcvd) {
    //     for (const auto& pair : this->connections) {
    //         std::string msg = read_msg(pair.second.read_fd);
    //         if (msg.empty()) continue;
    //         else if (msg[0] == '3') {
    //             parent_node = pair.first;
    //         }
    //         else if (msg == "2") return false;
    //     }
    //     if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - past).count() > 8) {
    //         std::cerr << "failure to hear back from all neighbors, abort\n";
    //         return false; 
    //     }
    // }

    // //propogate begin message to all non-parent nodes
    // for (const auto& pair : this->connections) {
    //     if (pair.first == parent_node) continue;
    //     send_message(pair.first, 3, "");
    // }

    // //edge nodes responsible for initiating ACK messages
    

    // //once ACKs received from all neighbors, send ACK to parent
    // past = std::chrono::steady_clock::now();
    // int num_started = 0;
    // std::unordered_set<int> heard_back; //nodes that we have heard back from
    // int zero_or_not = this->node_number == 0 ? 0 : 1;
    // while (heard_back.size() < (this->connections).size() - zero_or_not) {
    //     for (const auto& pair : this->connections) {
    //         std::string msg = read_msg(pair.second.read_fd);
    //         if (pair.first == parent_node || msg.empty()) continue;
    //         else if (msg[0] == '3') {
    //             heard_back.emplace(pair.first);
    //         }
    //         else if (msg == "2") return;
    //     }
    //     if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - past).count() > 8) {
    //         std::cerr << "failure to hear back from all neighbors, abort\n";
    //         return; 
    //     }
    // }
    // std::cout << "Node " << this->node_number << " start message from all neighbors, beginning MAP protocol\n" << std::flush;


}

void Node::do_MAP() {
    //if (!start_MAP()) return;    
    std::cout << "Node " << this->node_number << " startup success\n" << std::flush;
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> num_messages(this->minPerActive, this->maxPerActive);
    std::uniform_int_distribution<> nodes(0, (this->connections).size() - 1);
    std::vector<int> temp_connections;
    for (const auto& pair : this->connections) temp_connections.push_back(pair.first); //in order to random access nodes to send messages to, construct vector of node_nums
    auto past = std::chrono::steady_clock::now();
    if (this->node_number == 0) snapshot[0] = this->clock;

    //snapshot file
    std::string filename = "config-" + std::to_string(this->node_number) + ".out";
    std::ofstream snapfile(filename);
    if (!snapfile) {
        std::cerr << "Error: Could not open file " << filename << " for writing.\n";
        return;
    }

    //wait for start message response to begin
    // int num_started = 0;
    // std::unordered_set<int> heard_back; //nodes that we have heard back from
    // while (heard_back.size() < (this->connections).size()) {
    //     for (const auto& pair : this->connections) {
    //         std::string msg = read_msg(pair.second.read_fd);
    //         if (msg.empty()) continue;
    //         else if (msg[0] == '3') {
    //             heard_back.emplace(pair.first);
    //         }
    //         else if (msg == "2") return;
    //     }
    //     if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - past).count() > 8) {
    //         std::cerr << "failure to hear back from all neighbors, abort\n";
    //         return; 
    //     }
    // }
    // std::cout << "Node " << this->node_number << " start message from all neighbors, beginning MAP protocol\n" << std::flush;
    // past = std::chrono::steady_clock::now();

    int messages_sent = 0;
    while (!(this->terminateProtocol) && !(this->destroy)) {
        std::cout << "this in loop " << this << "\n" << std::flush; 
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - past);
        if (elapsed.count() > 17) {
            snapfile.close();
            return; //if doing nothing for long time, stop executing program
        }
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
            if (msg.size() > 0) { //if successful read of message
                if (msg[0] == '0') {
                    std::vector<int> temp_clock = this->extract_clock(msg);
                    for (int i = 0; i < this->clock.size(); i++) {
                        (this->clock)[i] = std::max((this->clock)[i], temp_clock[i]);
                    }
                    (this->clock)[this->node_number]++;
                    debug_msg(pair.first, false, "");
                    std::cout << "vector clock: [";
                    for (int i = 0; i < (this->clock).size(); i++) {
                        std::cout << (this->clock)[i] << " ";
                    }
                    std::cout << "]\n" << std::flush;
                    this->become_active();
                }
                else if (msg[0] == '1') { //CL protocol
                    for (size_t i = 0; i < (this->clock).size(); ++i) {
                        snapfile << clock[i] << " ";
                        std::cout << "Node " << node_number << " wrote " << clock[i] << " to snapfile\n" << std::flush;
                    }
                    snapfile << "\n";
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
                    else if (msg.size() > 1 && this->node_number == 0) {
                        int nod_num = msg[2] - '0';
                        (this->snapshot)[nod_num] = this->extract_clock(msg);
                    }
                }
                else if (msg == "2") { //failure due to socket closed or some other fatal error
                    std::cout << "msg == 2\n" << std::flush;
                    for (const auto& p : this->connections) { //obtain nodenum
                        send_message(p.first, 2, ""); //send termination messages to neighbors
                    }
                    snapfile.close();
                    return;
                }
            }
        }
        
        this->become_passive();
    }
    snapfile.close();
}

void Node::print_snapshot() {
    std::cout << "Printing snapshot-------------------------------------\n" << std::flush;
    for (int i = 0; i < (this->snapshot).size(); i++) {
        std::cout << i << " ";
        for (int j = 0; j < (this->snapshot)[i].size(); j++) {
            std::cout << (this->snapshot)[i][j] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::flush;
}

std::ostream& operator<<(std::ostream& os, const Node& node) {
    os << "Node(hostname=" << node.hostname << ", port=" << node.port << ")\n";
    return os;
}

Node::~Node() {
    for (const auto& pair : this->connections) {
        close(pair.second.read_fd);
        close(pair.second.write_fd);
    }
}