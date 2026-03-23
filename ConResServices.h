#pragma once

#include <mutex>
#include <optional>

#include "UserDirectory.h"
#include "SessionGate.h"
#include "StateTracker.h"
#include "WriterFairRWLock.h"
#include "Guards.h"
#include "FileOps.h"

/* These lightweight service wrappers make the implementation read closer to the design diagram. */
class AuthenticationService {
public:
    explicit AuthenticationService(UserDirectory& users)
        : users_(users) {}

    /* Forward credential checks to the user directory. */
    std::optional<UserRecord> authenticate(int uid, const std::string& username) const
    {
        return users_.authenticate(uid, username);
    }

    /* The session launcher iterates over this list to create the worker threads. */
    const std::vector<UserRecord>& records() const
    {
        return users_.records();
    }

private:
    UserDirectory& users_;
};

/* The admission service exposes the semaphore-backed login gate. */
class SessionAdmissionControl {
public:
    explicit SessionAdmissionControl(SessionGate& gate)
        : gate_(gate) {}

    SessionGate& gate()
    {
        return gate_;
    }

    const SessionGate& gate() const
    {
        return gate_;
    }

private:
    SessionGate& gate_;
};

/* This wrapper keeps state tracking separate from the higher-level workflow code. */
class SessionStateManagement {
public:
    explicit SessionStateManagement(StateTracker& tracker)
        : tracker_(tracker) {}

    StateTracker& tracker()
    {
        return tracker_;
    }

    const StateTracker& tracker() const
    {
        return tracker_;
    }

private:
    StateTracker& tracker_;
};

/* Monitoring methods are grouped here so main.cpp can stay focused on the session flow. */
class MonitoringAndStatusTracking {
public:
    explicit MonitoringAndStatusTracking(StateTracker& tracker)
        : tracker_(tracker) {}

    /* Print the run configuration before the worker threads begin. */
    void printBanner(int users, int opsPerUser, int maxSessions)
    {
        tracker_.printBanner(users, opsPerUser, maxSessions);
    }

    /* Every significant state transition is funneled through the tracker. */
    void printStatus(const std::string& reason)
    {
        tracker_.printStatus(reason);
    }

    /* Summaries give the report and demo a compact end-of-run view. */
    void printStatistics()
    {
        tracker_.printStatistics();
    }

    /* Export the collected state history so the frontend can replay the run. */
    void exportFrontendData(
        const std::vector<UserRecord>& users,
        int maxSessions,
        int opsPerUser
    )
    {
        tracker_.exportFrontendData(users, maxSessions, opsPerUser);
    }

    std::mutex& coutMutex()
    {
        return tracker_.coutMutex;
    }

private:
    StateTracker& tracker_;
};

/* ResourceAccessService wraps the lock guards around the file operations themselves. */
class ResourceAccessService {
public:
    ResourceAccessService(WriterFairRWLock& fileRW, StateTracker& tracker)
        : fileRW_(fileRW), tracker_(tracker) {}

    /* Reads take shared ownership, simulate file access, then release automatically. */
    void performRead(int uid)
    {
        ReadGuard guard(fileRW_, tracker_, uid);
        doReadFileSimulated(uid);
    }

    /* Writes take exclusive ownership before appending to the shared file. */
    void performWrite(int uid, int opIndex)
    {
        WriteGuard guard(fileRW_, tracker_, uid);
        doWriteFileSimulated(uid, opIndex);
    }

private:
    WriterFairRWLock& fileRW_;
    StateTracker& tracker_;
};
