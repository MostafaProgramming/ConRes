#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
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

#include "UserDirectory.h"

struct TrackerEvent {
    long long id = 0;
    long long elapsedMs = 0;
    std::string timestamp;
    std::string reason;
    std::string category;
    std::vector<int> activeUsers;
    std::vector<int> waitingUsers;
    std::vector<int> readingUsers;
    std::optional<int> writingUser;
};

class StateTracker {
public:
    // Shared state snapshots for the live concurrency view.
    std::unordered_set<int> activeUsers;
    std::deque<int> waitingUsers;
    std::unordered_set<int> readingUsers;
    std::optional<int> writingUser;

    // Mutexes protect shared state and serialized console/log output.
    std::mutex stateMutex;
    std::mutex coutMutex;

    std::ofstream logFile;

    // Atomic counters support lock-safe summary statistics.
    std::atomic<int> totalReads{0};
    std::atomic<int> totalWrites{0};
    std::atomic<int> peakActive{0};
    std::atomic<int> sessionsCompleted{0};

    std::atomic<long long> eventCounter{0};

    // Per-user activity counts are exported to the frontend dataset.
    std::unordered_map<int, int> readsByUser;
    std::unordered_map<int, int> writesByUser;

    // Event history is stored separately for replay and evidence output.
    std::mutex historyMutex;
    std::vector<TrackerEvent> eventHistory;

    std::chrono::steady_clock::time_point startedAt;


    StateTracker() {
        logFile.open("ConRes_Log.txt", std::ios::out | std::ios::trunc);
        startedAt = std::chrono::steady_clock::now();
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
        readsByUser[uid]++;

        totalReads++;
    }

    void stopReading(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        readingUsers.erase(uid);
    }

    void startWriting(int uid) {
        std::lock_guard<std::mutex> lk(stateMutex);

        writingUser = uid;
        writesByUser[uid]++;

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

    static std::vector<int> sortedSnapshot(const std::unordered_set<int>& users)
    {
        std::vector<int> result(users.begin(), users.end());
        std::sort(result.begin(), result.end());
        return result;
    }

    static std::string classifyReason(const std::string& reason)
    {
        if (reason == "System starting")
            return "system_start";
        if (reason == "System finished")
            return "system_finish";
        if (reason.find("requested login") != std::string::npos)
            return "login_requested";
        if (reason.find("logged in") != std::string::npos)
            return "login_success";
        if (reason.find("logged out") != std::string::npos)
            return "logout";
        if (reason.find("Reading started") != std::string::npos)
            return "read_start";
        if (reason.find("Reading finished") != std::string::npos)
            return "read_finish";
        if (reason.find("Updating started") != std::string::npos)
            return "write_start";
        if (reason.find("Updating finished") != std::string::npos)
            return "write_finish";
        return "info";
    }

    static std::string escapeJson(const std::string& value)
    {
        std::ostringstream escaped;

        for (unsigned char ch : value) {
            switch (ch) {
            case '\\':
                escaped << "\\\\";
                break;
            case '"':
                escaped << "\\\"";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped << "\\u"
                            << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(ch)
                            << std::dec << std::setfill(' ');
                } else {
                    escaped << static_cast<char>(ch);
                }
            }
        }

        return escaped.str();
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
        long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt
        ).count();
        std::string timestamp = nowTime();

        std::ostringstream output;

        output << "#" << eventId << " ";

        output << "[" << timestamp << "] ";

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

        TrackerEvent event;
        event.id = eventId;
        event.elapsedMs = elapsedMs;
        event.timestamp = timestamp;
        event.reason = reason;
        event.category = classifyReason(reason);
        event.activeUsers = sortedSnapshot(activeSnap);
        event.waitingUsers.assign(waitingSnap.begin(), waitingSnap.end());
        event.readingUsers = sortedSnapshot(readingSnap);
        event.writingUser = writingSnap;

        {
            std::lock_guard<std::mutex> historyLock(historyMutex);
            eventHistory.push_back(std::move(event));
        }

        std::lock_guard<std::mutex> out(coutMutex);

        std::cout << output.str();

        if (logFile.is_open())
            logFile << output.str();
    }

    void exportFrontendData(
        const std::vector<UserRecord>& users,
        int maxSessions,
        int opsPerUser,
        const std::string& path = "frontend/data/conres_run.json"
    )
    {
        std::vector<TrackerEvent> eventsSnapshot;
        std::unordered_map<int, int> readsSnapshot;
        std::unordered_map<int, int> writesSnapshot;

        {
            std::lock_guard<std::mutex> historyLock(historyMutex);
            eventsSnapshot = eventHistory;
        }

        {
            std::lock_guard<std::mutex> stateLock(stateMutex);
            readsSnapshot = readsByUser;
            writesSnapshot = writesByUser;
        }

        std::filesystem::path exportPath(path);

        if (exportPath.has_parent_path())
            std::filesystem::create_directories(exportPath.parent_path());

        std::ofstream out(exportPath, std::ios::out | std::ios::trunc);

        out << "{\n";
        out << "  \"metadata\": {\n";
        out << "    \"title\": \"ConRes Concurrency Visualization\",\n";
        out << "    \"generatedAt\": \"" << escapeJson(nowTime()) << "\",\n";
        out << "    \"maxSessions\": " << maxSessions << ",\n";
        out << "    \"operationsPerUser\": " << opsPerUser << ",\n";
        out << "    \"totalUsers\": " << users.size() << ",\n";
        out << "    \"totalEvents\": " << eventsSnapshot.size() << "\n";
        out << "  },\n";

        out << "  \"users\": [\n";
        for (size_t i = 0; i < users.size(); ++i) {
            const auto& user = users[i];
            out << "    {\n";
            out << "      \"id\": " << user.id << ",\n";
            out << "      \"username\": \"" << escapeJson(user.username) << "\",\n";
            out << "      \"reads\": " << readsSnapshot[user.id] << ",\n";
            out << "      \"writes\": " << writesSnapshot[user.id] << "\n";
            out << "    }" << (i + 1 < users.size() ? "," : "") << "\n";
        }
        out << "  ],\n";

        out << "  \"summary\": {\n";
        out << "    \"totalReads\": " << totalReads.load() << ",\n";
        out << "    \"totalWrites\": " << totalWrites.load() << ",\n";
        out << "    \"peakActiveUsers\": " << peakActive.load() << ",\n";
        out << "    \"sessionsCompleted\": " << sessionsCompleted.load() << "\n";
        out << "  },\n";

        out << "  \"events\": [\n";
        for (size_t i = 0; i < eventsSnapshot.size(); ++i) {
            const auto& event = eventsSnapshot[i];

            auto writeArray = [&](const char* label, const std::vector<int>& values)
            {
                out << "      \"" << label << "\": [";
                for (size_t j = 0; j < values.size(); ++j) {
                    out << values[j];
                    if (j + 1 < values.size())
                        out << ", ";
                }
                out << "]";
            };

            out << "    {\n";
            out << "      \"id\": " << event.id << ",\n";
            out << "      \"elapsedMs\": " << event.elapsedMs << ",\n";
            out << "      \"timestamp\": \"" << escapeJson(event.timestamp) << "\",\n";
            out << "      \"category\": \"" << escapeJson(event.category) << "\",\n";
            out << "      \"reason\": \"" << escapeJson(event.reason) << "\",\n";
            writeArray("activeUsers", event.activeUsers);
            out << ",\n";
            writeArray("waitingUsers", event.waitingUsers);
            out << ",\n";
            writeArray("readingUsers", event.readingUsers);
            out << ",\n";
            out << "      \"writingUser\": ";
            if (event.writingUser)
                out << *event.writingUser;
            else
                out << "null";
            out << "\n";
            out << "    }" << (i + 1 < eventsSnapshot.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
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
