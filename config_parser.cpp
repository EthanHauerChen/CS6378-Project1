#include "config_parser.h"
#include <string>
#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>

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

config extract_config(std::string filename) {
    std::ifstream configFile(filename);
    struct config values;
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    values.hostname = hostname;

    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        fprintf(stderr, "unable to open file: %s\n", filename);
        return;
    }

    std::string line;
    /* read first line */
    while (getline(configFile, line)) {
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) { //ensure current line is a valid line
            std::cout << line << "\n";
            std::vector<std::string> tokens = split(line, " ");
            std::cout << tokens.size() << "\n";
            if (tokens.size() != 6) {
                fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line");
                return;
            }
            values.nodes = std::stoi(tokens[0]);
            values.minPerActive = std::stoi(tokens[1]);
            values.maxPerActive = std::stoi(tokens[2]);
            values.minSendDelay = std::stol(tokens[3]);
            values.snapshotDelay = std::stol(tokens[4]);
            values.maxNumber = std::stoi(tokens[5]);
            std::cout << "before break" << "\n";
            break;
        }
    }

    /* scan next n (values.nodes) lines to obtain port number, TODO: scan all n lines so that last n lines able to properly obtain neighbors*/
    int valid_lines_read = 0;
    while (valid_lines_read < values.nodes) {
        getline(configFile, line);
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) { //ensure current line is a valid line
            std::cout << line << "\n";
            std::vector<std::string> tokens = split(line, " ");
            std::cout << tokens.size() << "\n";
            if (tokens.size() != 3) {
                fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line");
                return;
            }
            valid_lines_read++;

            if (strstr(hostname), tokens[1]) { //if current line contains the machine's hostname
                std::cout << hostname;
                values.port = tokens[2];
            }
            else break;
        }
    }

    /* scan final n lines */
    valid_lines_read = 0;
    while (valid_lines_read < values.nodes) {
        getline(configFile, line);
        int num;
        if (isdigit(line[0]) && (num = line[0] - '0') >= 0) { //ensure current line is a valid line
            std::cout << line << "\n";
            std::vector<std::string> tokens = split(line, " ");
            std::cout << tokens.size() << "\n";
            if (tokens.size() != 3) {
                fprintf(stderr, "config file does not contain the correct number of tokens on the first valid line");
                return;
            }
            valid_lines_read++;
        }
    }
}