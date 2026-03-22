#pragma once

#include <fstream>
#include <thread>
#include <chrono>

static const char* kFileName = "ProductSpecification.txt";

inline void ensureFileExists() {
    std::ifstream in(kFileName);
    if (!in.good()) {
        std::ofstream out(kFileName);
        out << "Initial Product Specification\n";
    }
}

inline void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void doReadFileSimulated(int uid) {
    std::ifstream in(kFileName);
    std::string line;
    while (std::getline(in, line)) {}
    sleep_ms(80 + (uid % 5) * 30);
}

inline void doWriteFileSimulated(int uid, int opIndex) {
    std::ofstream out(kFileName, std::ios::app);
    out << "Update by user " << uid << " (op " << opIndex << ")\n";
    sleep_ms(120 + (uid % 5) * 40);
}