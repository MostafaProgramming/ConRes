#pragma once

#include "SessionGate.h"
#include "WriterFairRWLock.h"
#include "StateTracker.h"

/* SessionGuard ties login and logout to object lifetime so a thread cannot forget to release its session slot. */
class SessionGuard {
public:
    SessionGuard(SessionGate& gate, int uid)
        : gate_(gate), uid_(uid), active_(true) {
        gate_.login(uid_);
    }

    SessionGuard(const SessionGuard&) = delete;
    SessionGuard& operator=(const SessionGuard&) = delete;
    SessionGuard(SessionGuard&&) = delete;
    SessionGuard& operator=(SessionGuard&&) = delete;

    ~SessionGuard() {
        if (active_) gate_.logout(uid_);
    }

private:
    SessionGate& gate_;
    int uid_;
    bool active_;
};

/* ReadGuard and WriteGuard apply the same RAII pattern to file ownership and tracker updates. */
class ReadGuard {
public:
    ReadGuard(WriterFairRWLock& rw, StateTracker& tracker, int uid)
        : rw_(rw), tracker_(tracker), uid_(uid) {
        rw_.acquireRead();
        tracker_.startReading(uid_);
        tracker_.printStatus("File: Reading started by " + std::to_string(uid_));
    }

    ReadGuard(const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;
    ReadGuard(ReadGuard&&) = delete;
    ReadGuard& operator=(ReadGuard&&) = delete;

    ~ReadGuard() {
        tracker_.stopReading(uid_);
        tracker_.printStatus("File: Reading finished by " + std::to_string(uid_));
        rw_.releaseRead();
    }

private:
    WriterFairRWLock& rw_;
    StateTracker& tracker_;
    int uid_;
};

/* WriteGuard keeps the writer lock, tracker state, and file update lifetime in sync. */
class WriteGuard {
public:
    WriteGuard(WriterFairRWLock& rw, StateTracker& tracker, int uid)
        : rw_(rw), tracker_(tracker), uid_(uid) {
        rw_.acquireWrite();
        tracker_.startWriting(uid_);
        tracker_.printStatus(
            "File: Updating started by " + std::to_string(uid_));
    }

    WriteGuard(const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;
    WriteGuard(WriteGuard&&) = delete;
    WriteGuard& operator=(WriteGuard&&) = delete;

    ~WriteGuard() {
        tracker_.stopWriting(uid_);
        tracker_.printStatus(
            "File: Updating finished by " + std::to_string(uid_));
        rw_.releaseWrite();
    }

private:
    WriterFairRWLock& rw_;
    StateTracker& tracker_;
    int uid_;
};
