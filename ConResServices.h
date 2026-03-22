#pragma once

#include <mutex>
#include <optional>

#include "UserDirectory.h"
#include "SessionGate.h"
#include "StateTracker.h"
#include "WriterFairRWLock.h"
#include "Guards.h"
#include "FileOps.h"

class AuthenticationService {
public:
    explicit AuthenticationService(UserDirectory& users)
        : users_(users) {}

    std::optional<UserRecord> authenticate(int uid, const std::string& username) const
    {
        return users_.authenticate(uid, username);
    }

    const std::vector<UserRecord>& records() const
    {
        return users_.records();
    }

private:
    UserDirectory& users_;
};

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

class MonitoringAndStatusTracking {
public:
    explicit MonitoringAndStatusTracking(StateTracker& tracker)
        : tracker_(tracker) {}

    void printBanner(int users, int opsPerUser, int maxSessions)
    {
        tracker_.printBanner(users, opsPerUser, maxSessions);
    }

    void printStatus(const std::string& reason)
    {
        tracker_.printStatus(reason);
    }

    void printStatistics()
    {
        tracker_.printStatistics();
    }

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

class ResourceAccessService {
public:
    ResourceAccessService(WriterFairRWLock& fileRW, StateTracker& tracker)
        : fileRW_(fileRW), tracker_(tracker) {}

    void performRead(int uid)
    {
        ReadGuard guard(fileRW_, tracker_, uid);
        doReadFileSimulated(uid);
    }

    void performWrite(int uid, int opIndex)
    {
        WriteGuard guard(fileRW_, tracker_, uid);
        doWriteFileSimulated(uid, opIndex);
    }

private:
    WriterFairRWLock& fileRW_;
    StateTracker& tracker_;
};
