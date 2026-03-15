#include <thread>
#include <vector>
#include <random>
#include <chrono>

#include "SystemContext.h"
#include "Guards.h"
#include "FileOps.h"

void userSession(SystemContext& ctx, int uid, int operations, unsigned seed)
{
    {
        std::lock_guard<std::mutex> out(ctx.tracker.coutMutex);
        std::cout << "[" << StateTracker::nowTime()
                  << "] User " << uid << " authenticating...\n";
    }

    sleep_ms(50 + (uid % 7) * 20);

    SessionGuard session(ctx.gate, uid);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> coin(0, 99);

    for (int i = 1; i <= operations; ++i) {

        if (coin(rng) < 70) {
            ReadGuard rg(ctx.fileRW, ctx.tracker, uid);
            doReadFileSimulated(uid);
        } else {
            WriteGuard wg(ctx.fileRW, ctx.tracker, uid);
            doWriteFileSimulated(uid, i);
        }

        sleep_ms(30 + (uid % 5) * 15);
    }
}

int main()
{
    ensureFileExists();

    constexpr int N = 4;
    constexpr int totalUsers = 10;
    constexpr int opsPerUser = 6;

    SystemContext ctx(N);
    ctx.tracker.printBanner(totalUsers, opsPerUser, N);
    std::cout << "\n==============================\n";
    std::cout << " ConRes Concurrency Simulation\n";
    std::cout << "==============================\n\n";

    std::cout << "Users: " << totalUsers << "\n";
    std::cout << "Operations per user: " << opsPerUser << "\n";
    std::cout << "Max concurrent sessions: " << N << "\n";
    std::cout << "Synchronization: Semaphore + RWLock\n";

    std::cout << "\n==============================\n";
    std::cout << "Simulation starting\n";
    std::cout << "==============================\n\n";
    ctx.tracker.printStatus("System starting");

    std::vector<std::thread> threads;

    for (int uid = 1; uid <= totalUsers; ++uid) {
        unsigned seed = static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count() + uid * 1337u
        );

        threads.emplace_back([&ctx, uid, seed] {
            userSession(ctx, uid, opsPerUser, seed);
        });

        sleep_ms(35);
    }

    for (auto& t : threads)
        t.join();
    ctx.tracker.printStatus("System finished");
    ctx.tracker.printStatistics();
}