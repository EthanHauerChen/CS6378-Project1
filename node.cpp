#include "node.h"
#include <sys/socket.h>

Node::Node(std::string hostname, int port) {
    this.hostname = hostname;
    this.port = port;
}

Node::listen_for_connections() { 

}

/** obtain list of hostnames and ports, create multiple sockets for each and attempt to connecdt */
Node::initiate_connections(const std::string& hostnames, int[] ports, int size) {
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
        host = gethostbyname(hostnames[i]);
        
        /* connection already exists, no need to connect */
        if (address.sin_family = connections.contains)
        if (!host) {
            fprintf(stderr, "error: unknown host %s\n", argv[1]);
            return -2;
        }
        memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
        if (connect(sockfd, (struct sockaddr *)&address, sizeof(address))) {
            fprintf(stderr, "%s: error: cannot connect to host %s\n", argv[0], argv[1]);
            return -3;
        }

        connection_fds.insert(sockfd);
    }

    return 0; //success
}