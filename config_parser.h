#include <string>
#include <vector>

struct neighbor {
    std::string hostname;
    int port;
    int nodenum;
};

struct config {
    int nodes, minPerActive, maxPerActive;
    long minSendDelay, snapshotDelay; 
    int maxNumber;
    
    std::string hostname;
    int port;
    std::vector<neighbor> neighbors;
};

std::ostream& operator<<(std::ostream &strm, const config &c);

int extract_config(std::string filename, config& values);