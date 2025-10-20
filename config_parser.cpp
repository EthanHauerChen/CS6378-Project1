#include "config_parser.h"
#include <string>
#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <ostream>

/* taken from https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c */
std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    std::string s = str;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    tokens.push_back(s);

    return tokens;
}

/** TODO: make sure 'values' parameter is only altered at the very end of the function
 * when it isn't possible to "error out" and return early, resulting in a partially completed
 * config struct
 */
int extract_config(std::string filename, config& values) {
    std::ifstream configFile(filename);
    /****** UNCOMMENT WHEN TESTING ON DC SERVERS */
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    values.hostname = hostname;
    //char* hostname = "dc02.utdallas.edu";

    if (!configFile.is_open()) {
        fprintf(stderr, "unable to open file: %s\n", filename.c_str());
        return -1;
    }

    std::string line;
    /* read first line */
    while (getline(configFile, line)) {
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) { //ensure current line is a valid line
            //std::cout << line << "\n";
            std::vector<std::string> tokens = split(line, " ");
            //std::cout << tokens.size() << "\n";
            if (tokens.size() != 6) {
                fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line\n");
                return -2;
            }
            values.nodes = std::stoi(tokens[0]);
            values.minPerActive = std::stoi(tokens[1]);
            values.maxPerActive = std::stoi(tokens[2]);
            values.minSendDelay = std::stol(tokens[3]);
            values.snapshotDelay = std::stol(tokens[4]);
            values.maxNumber = std::stoi(tokens[5]);
            //std::cout << "before break" << "\n";
            break;
        }
    }

    /* scan next n (values.nodes) lines to obtain port number */
    int valid_lines_read = 0;
    std::string hosts_ports[values.nodes];
    int node_num = -1; //which node num (not hostname) the program is
    while (valid_lines_read < values.nodes) {
        getline(configFile, line);
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) { //ensure current line is a valid line
            //std::cout << line << "\n";
            std::vector<std::string> tokens = split(line, " ");
            // std::cout << tokens[0][0] << " " << tokens[1][0] << "\n";
            // std::cout << tokens.size() << "\n";
            // std::cout << "hi";

            //remove all tokens starting from '#'
            int i = 0;
            while (i < tokens.size() && tokens[i][0] != '#') {
                //std::cout << i << ", ";
                i++;
            }
            //std::cout << "\n";
            tokens.resize(i);
            //std::cout << "new tokens size: " << tokens.size() << "\n";
            if (tokens.size() != 3) {
                fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line\n");
                return -3;
            }
            hosts_ports[valid_lines_read] = line;

            if (strstr(hostname, tokens[1].c_str())) { //if current line contains the machine's hostname
                std::cout << "hostname: " << hostname << ", tokens[1] = " << tokens[1] << ", valid_lines_read = " << valid_lines_read << "\n";
                values.port = std::stoi(tokens[2]);
                node_num = valid_lines_read;
                values.node_num = node_num;
            }
            valid_lines_read++;
        }
    }

    std::cout << "node_num = " << node_num << "\n";
    /* scan final n lines */
    valid_lines_read = 0;
    while (valid_lines_read < values.nodes) {
        getline(configFile, line);
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) { //ensure current line is a valid line
            std::cout << line << "\n";
            if (!(valid_lines_read == node_num)) {
                valid_lines_read++;
                continue;
            }
            std::vector<std::string> tokens = split(line, " ");
            //std::cout << tokens.size() << "\n";
            // if (tokens.size() > values.nodes - 1) {
            //     fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line\n");
            //     return -4;
            // }
            for (int i = 0; i < tokens.size(); i++) {
                if (!(isdigit(tokens[i][0]) && (num = tokens[i][0] - '0') >= 0)) break; //if token on the line is not a number, that means it must be invalid or a comment
                int neighbor_num = std::stoi(tokens[i]);
                std::cout << "neighbor_num: " << neighbor_num << "\n";
                std::vector<std::string> node_tokens = split(hosts_ports[neighbor_num], " ");

                std::string hostn = node_tokens[1] + ".utdallas.edu"; //hostname, use localhost when testing locally
                int port = std::stoi(node_tokens[2]); //port
                values.neighbors.push_back({hostn, port, neighbor_num});
                //values.neighbors.emplace_back(hostn, port); Doesn't work, emplace uses constructor which neighbor struct does not have
                // values.neighbors[i].hostname = node_tokens[1] + ".utdallas.edu"; //use localhost when testing locally
                // values.neighbors[i].port = std::stoi(node_tokens[2]);
            }
            break; //successfully read the line containing this node's neighbors
        }
    }

    return 0;
}

std::ostream& operator<<(std::ostream &strm, const config &c) {
    //I FUCKING HATE C++
    std::string neighbors = "[";
    for (neighbor n : c.neighbors) {
        neighbors += std::string("(hostname:") + n.hostname +
        std::string(", port:") + std::to_string(n.port) +
        std::string(", node #: ") + std::to_string(n.nodenum) + 
        std::string("), ");
    }
    neighbors += "]";
    return strm << "config {\n\t" << 
        "nodes: " << c.nodes << "\n\t" <<
        "minPerActive: " << c.minPerActive << "\n\t" <<
        "maxPerActive: " << c.maxPerActive << "\n\t" <<
        "minSendDelay: " << c.minSendDelay << "\n\t" <<
        "snapshotDelay: " << c.snapshotDelay << "\n\t" <<
        "maxNumber: " << c.maxNumber << "\n\t" <<
        "hostname: " << c.hostname << "\n\t" <<
        "port: " << c.port << "\n\t" <<
        "neighbors: " << neighbors << "\n}\n";
}