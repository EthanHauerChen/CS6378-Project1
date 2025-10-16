#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "node.h"
#include "config_parser.h"

int main(int argc, char ** argv) {
    struct config node_values;
    std::cout << "error: " << extract_config(argv[1], node_values) << "\n";
    std::cout << node_values;
}