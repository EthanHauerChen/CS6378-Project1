#include <string>
#include <vector>

struct neighbor {
    std::string hostname;
    int port;
};

struct config {
    int nodes, minPerActive, maxPerActive;
    long minSendDelay, snapshotDelay; 
    int maxNumber;
    
    std::string hostname;
    int port;
    std::vector<neighbor> neighbors;
};

int extract_config(std::string filename, config& values);