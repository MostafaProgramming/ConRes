#include <thread>
#include <vector>
#include <random>
#include <chrono>

#include "SystemContext.h"
#include "Guards.h"
#include "FileOps.h"

void userSession(SystemContext& ctx, UserRecord attemptedUser, int operations, unsigned seed)
{
    {
        std::lock_guard<std::mutex> out(ctx.monitoring.coutMutex());
        std::cout << "[" << StateTracker::nowTime()
                  << "] User " << attemptedUser.username
                  << " (" << attemptedUser.id << ") authenticating...\n";
    }

    sleep_ms(50 + (attemptedUser.id % 7) * 20);

    auto authenticatedUser = ctx.authentication.authenticate(
        attemptedUser.id, attemptedUser.username
    );

    if (!authenticatedUser) {
        std::lock_guard<std::mutex> out(ctx.monitoring.coutMutex());
        std::cout << "[" << StateTracker::nowTime()
                  << "] Authentication failed for "
                  << attemptedUser.username << " (" << attemptedUser.id << ")\n";
        return;
    }

    const UserRecord user = *authenticatedUser;

    SessionGuard session(ctx.admission.gate(), user.id);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> coin(0, 99);

    for (int i = 1; i <= operations; ++i) {

        if (coin(rng) < 70) {
            ctx.resourceAccess.performRead(user.id);
        } else {
            ctx.resourceAccess.performWrite(user.id, i);
        }

        sleep_ms(30 + (user.id % 5) * 15);
    }
}

int main()
{
    ensureFileExists();

    constexpr int N = 4;
    constexpr int opsPerUser = 6;

    SystemContext ctx(N);
    const int totalUsers = static_cast<int>(ctx.authentication.records().size());
    ctx.monitoring.printBanner(totalUsers, opsPerUser, N);
    ctx.monitoring.printStatus("System starting");

    std::vector<std::thread> threads;

    for (const auto& user : ctx.authentication.records()) {
        unsigned seed = static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
            + user.id * 1337u
        );

        threads.emplace_back([&ctx, user, seed] {
            userSession(ctx, user, opsPerUser, seed);
        });

        sleep_ms(35);
    }

    for (auto& t : threads)
        t.join();
    ctx.monitoring.printStatus("System finished");
    ctx.monitoring.printStatistics();
    ctx.monitoring.exportFrontendData(ctx.authentication.records(), N, opsPerUser);
}
