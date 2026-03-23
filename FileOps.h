#pragma once

#include <fstream>
#include <thread>
#include <chrono>

/* The shared resource for both the console simulation and the manual frontend. */
static const char* kFileName = "ProductSpecification.txt";

/* Seed the shared file with a starter line so reads always have something to inspect. */
inline void ensureFileExists() {
    std::ifstream in(kFileName);
    if (!in.good()) {
        std::ofstream out(kFileName);
        out << "Initial Product Specification\n";
    }
}

/* Small helper used throughout the simulation to make thread interleaving visible. */
inline void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/* Simulated reads consume the file and hold the shared lock long enough to overlap. */
inline void doReadFileSimulated(int uid) {
    std::ifstream in(kFileName);
    std::string line;
    while (std::getline(in, line)) {}
    sleep_ms(80 + (uid % 5) * 30);
}

/* Simulated writes append an audit line while the writer lock is held exclusively. */
inline void doWriteFileSimulated(int uid, int opIndex) {
    std::ofstream out(kFileName, std::ios::app);
    out << "Update by user " << uid << " (op " << opIndex << ")\n";
    sleep_ms(120 + (uid % 5) * 40);
}
