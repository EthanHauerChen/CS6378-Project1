#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

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

int main(int argc, char ** argv) {
    int nodes, minPerActive, maxPerActive;
    long minSendDelay, snapshotDelay; 
    int maxNumber;

    std::ifstream configFile(argv[1]);

    /* check commandline args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s config_file\n", argv[0]);
        return -1;
    }

    if (!configFile.is_open()) {
        fprintf(stderr, "unable to open file: %s\n", argv[1]);
        return -2;
    }
    
    std::string line;
    /* read first line */
    while (getline(configFile, line)) {
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) {
            std::cout << line << "\n";
            std::vector<std::string> tokens = split(line, " ");
            std::cout << tokens.size() << "\n";
            if (tokens.size() != 6) {
                fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line");
                return -3;
            }
            nodes = std::stoi(tokens[0]);
            minPerActive = std::stoi(tokens[1]);
            maxPerActive = std::stoi(tokens[2]);
            minSendDelay = std::stol(tokens[3]);
            snapshotDelay = std::stol(tokens[4]);
            maxNumber = std::stoi(tokens[5]);
            std::cout << "before break" << "\n";
            break;
        }
    }

    printf("%d %d %d %ld %ld %d", nodes, minPerActive, maxPerActive, minSendDelay, snapshotDelay, maxNumber);
}