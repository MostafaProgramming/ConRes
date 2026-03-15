#pragma once

#include "SessionGate.h"
#include "WriterFairRWLock.h"
#include "StateTracker.h"

class SessionGuard {
public:
    SessionGuard(SessionGate& gate, int uid)
        : gate_(gate), uid_(uid), active_(true) {
        gate_.login(uid_);
    }

    ~SessionGuard() {
        if (active_) gate_.logout(uid_);
    }

private:
    SessionGate& gate_;
    int uid_;
    bool active_;
};

class ReadGuard {
public:
    ReadGuard(WriterFairRWLock& rw, StateTracker& tracker, int uid)
        : rw_(rw), tracker_(tracker), uid_(uid) {
        rw_.acquireRead();
        tracker_.startReading(uid_);
        tracker_.printStatus("File: Reading started by " + std::to_string(uid_));
    }

    ~ReadGuard() {
        tracker_.stopReading(uid_);
        rw_.releaseRead();
        tracker_.printStatus("File: Reading finished by " + std::to_string(uid_));
    }

private:
    WriterFairRWLock& rw_;
    StateTracker& tracker_;
    int uid_;
};

class WriteGuard {
public:
    WriteGuard(WriterFairRWLock& rw, StateTracker& tracker, int uid)
        : rw_(rw), tracker_(tracker), uid_(uid) {

        rw_.acquireWrite();

        tracker_.startWriting(uid_);

        tracker_.printStatus(
            "File: Updating started by " + std::to_string(uid_));
    }

    ~WriteGuard() {

        tracker_.printStatus(
            "File: Updating finished by " + std::to_string(uid_));

        tracker_.stopWriting(uid_);

        rw_.releaseWrite();
    }

private:
    WriterFairRWLock& rw_;
    StateTracker& tracker_;
    int uid_;
};
