#pragma once

#include <mutex>
#include <condition_variable>
#include <semaphore>
#include "StateTracker.h"

/* SessionGate limits how many authenticated users may be active at the same time. */
class SessionGate {
public:
    explicit SessionGate(int maxActive, StateTracker& tracker)
        : slots_(maxActive), tracker_(tracker) {}

    void login(int uid) {
        tracker_.addWaiting(uid);
        tracker_.printStatus("User " + std::to_string(uid) + " requested login");

        uint64_t myTicket;
        {
            /* Ticketing keeps login attempts moving through the gate in arrival order. */
            std::lock_guard<std::mutex> lk(gateMtx_);
            myTicket = nextTicket_++;
        }

        {
            std::unique_lock<std::mutex> lk(gateMtx_);
            gateCv_.wait(lk, [&] { return myTicket == nowServing_; });
        }

        /* The semaphore enforces the hard upper bound on active sessions. */
        slots_.acquire();

        tracker_.moveWaitingToActive(uid);
        tracker_.printStatus("User " + std::to_string(uid) + " logged in");

        {
            std::lock_guard<std::mutex> lk(gateMtx_);
            ++nowServing_;
        }

        /* Wake the next queued login once this ticket has moved through the gate. */
        gateCv_.notify_all();
    }

    void logout(int uid) {
        tracker_.removeActive(uid);
        slots_.release();
        tracker_.printStatus("User " + std::to_string(uid) + " logged out");
        gateCv_.notify_all();
    }

private:
    std::counting_semaphore<4> slots_;

    StateTracker& tracker_;

    /* The ticket gate gives waiting users a predictable FIFO admission order. */
    std::mutex gateMtx_;
    std::condition_variable gateCv_;
    uint64_t nextTicket_ = 0;
    uint64_t nowServing_ = 0;
};
