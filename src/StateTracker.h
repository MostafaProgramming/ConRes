#pragma once

#include <fstream>
#include <unordered_set>
#include <deque>
#include <optional>
#include <mutex>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <atomic>

class StateTracker {
public:

    std::unordered_set<int> activeUsers;
    std::deque<int> waitingUsers;
    std::unordered_set<int> readingUsers;
    std::optional<int> writingUser;

    std::mutex stateMutex;
    std::mutex coutMutex;

    std::ofstream logFile;


    std::atomic<int> totalReads{0};
    std::atomic<int> totalWrites{0};
    std::atomic<int> peakActive{0};
    std::atomic<int> sessionsCompleted{0};

    std::atomic<long long> eventCounter{0};



    StateTracker() {
        logFile.open("ConRes_Log.txt", std::ios::out | std::ios::trunc);
    }


    static std::string nowTime() {
        using clock = std::chrono::system_clock;

        auto t = clock::to_time_t(clock::now());

        std::tm tm{};

    #ifdef _WIN32
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif

        std::ostringstream oss;

        oss << std::put_time(&tm, "%H:%M:%S");

        return oss.str();
    }


    void addWaiting(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);
        waitingUsers.push_back(uid);
    }

    void moveWaitingToActive(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        auto it = std::find(waitingUsers.begin(), waitingUsers.end(), uid);

        if (it != waitingUsers.end())
            waitingUsers.erase(it);

        activeUsers.insert(uid);

        // update peak active users
        int current = activeUsers.size();
        peakActive = std::max(peakActive.load(), current);
    }

    void removeActive(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        activeUsers.erase(uid);

        sessionsCompleted++;
    }

  
    void startReading(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        readingUsers.insert(uid);

        totalReads++;
    }

    void stopReading(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        readingUsers.erase(uid);
    }

    void startWriting(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        writingUser = uid;

        totalWrites++;
    }

    void stopWriting(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        if (writingUser && *writingUser == uid)
            writingUser.reset();
    }


    void printBanner(int users, int opsPerUser, int maxSessions){
        std::lock_guard<std::mutex> lk(coutMutex);

        std::ostringstream banner;

        banner << "==============================\n";
        banner << "   ConRes Concurrency System\n";
        banner << "==============================\n\n";

        banner << "Simulation Configuration\n";
        banner << "------------------------\n";
        banner << "Users: " << users << "\n";
        banner << "Operations per user: " << opsPerUser << "\n";
        banner << "Max concurrent sessions: " << maxSessions << "\n";
        banner << "Synchronization model: Semaphore + Readers/Writers Lock\n";
        banner << "\n==============================\n";
        banner << "Simulation starting\n";
        banner << "==============================\n\n";

        std::cout << banner.str();

        if (logFile.is_open()){
            logFile << banner.str();
        }
    }
    void printStatus(const std::string& reason = ""){
        std::unordered_set<int> activeSnap;
        std::deque<int> waitingSnap;
        std::unordered_set<int> readingSnap;
        std::optional<int> writingSnap;

        {
            std::lock_guard<std::mutex> lk(stateMutex);

            activeSnap = activeUsers;
            waitingSnap = waitingUsers;
            readingSnap = readingUsers;
            writingSnap = writingUser;
        }

        long long eventId = ++eventCounter;

        std::ostringstream output;

        output << "#" << eventId << " ";

        output << "[" << nowTime() << "] ";

        if (!reason.empty())
            output << reason << " | ";

        auto printSet = [&](const std::unordered_set<int>& s)
        {
            std::vector<int> v(s.begin(), s.end());

            std::sort(v.begin(), v.end());

            output << "[";

            for (size_t i = 0; i < v.size(); ++i)
                output << v[i] << (i + 1 < v.size() ? ", " : "");

            output << "]";
        };

        output << "Active=";
        printSet(activeSnap);

        output << " Waiting=[";

        for (size_t i = 0; i < waitingSnap.size(); ++i)
            output << waitingSnap[i] << (i + 1 < waitingSnap.size() ? ", " : "");

        output << "]";

        output << " Reading=";
        printSet(readingSnap);

        output << " Writing=";

        if (writingSnap)
            output << *writingSnap;
        else
            output << "None";

        output << "\n";

        std::lock_guard<std::mutex> out(coutMutex);

        std::cout << output.str();

        if (logFile.is_open())
            logFile << output.str();
    }


    void printStatistics()
    {
        std::lock_guard<std::mutex> lk(coutMutex);

        std::cout << "\n==============================\n";
        std::cout << "        ConRes Statistics\n";
        std::cout << "==============================\n";

        std::cout << "Total Reads: " << totalReads << "\n";
        std::cout << "Total Writes: " << totalWrites << "\n";
        std::cout << "Peak Active Users: " << peakActive << "\n";
        std::cout << "Sessions Completed: " << sessionsCompleted << "\n";

        std::cout << "==============================\n";

        if (logFile.is_open()) {
            logFile << "\n==============================\n";
            logFile << "        ConRes Statistics\n";
            logFile << "==============================\n";
            logFile << "Total Reads: " << totalReads << "\n";
            logFile << "Total Writes: " << totalWrites << "\n";
            logFile << "Peak Active Users: " << peakActive << "\n";
            logFile << "Sessions Completed: " << sessionsCompleted << "\n";
            logFile << "==============================\n";
        }
    }
};
