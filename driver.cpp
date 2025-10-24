#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "node.h"
#include "config_parser.h"

int main(int argc, char ** argv) {
    struct config node_values;
    std::cout << "error: " << extract_config(argv[1], node_values) << "\n" << std::flush;
    std::cout << node_values;
    Node n {node_values};
    if (node_values.node_num == 0) {
        n.print_snapshot();
    }
}