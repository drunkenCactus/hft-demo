#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

const std::string logfile_path = "/var/log/hft/md_feeder.log";

int main(int argc, char *argv[]) {
    std::ofstream logfile;
    logfile.open(logfile_path, std::ios::app);
    if (!logfile.is_open()) {
        std::cerr << "Failed to open logfile: " << logfile_path << std::endl;
    }

    logfile << "MD Feeder started!" << std::endl;
    uint64_t seconds = 0;
    while (true) {
        logfile << "Alive for " << ++seconds << " seconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    logfile.close();
    return 0;
}
