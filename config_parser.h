#include <string>

struct config {
    int nodes, minPerActive, maxPerActive;
    long minSendDelay, snapshotDelay; 
    int maxNumber;
    
    std::string hostname;
    int port;
};

config extract_config(std::string filename);