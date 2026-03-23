#pragma once

#include <mutex>
#include <condition_variable>

/* This readers-writer lock prefers waiting writers so writes are not starved by constant reads. */
class WriterFairRWLock {
public:
    void acquireRead() {
        std::unique_lock<std::mutex> lk(m_);
        /* New readers pause while a writer is active or already queued. */
        cv_.wait(lk, [&] { return !writerActive_ && waitingWriters_ == 0; });
        ++activeReaders_;
    }

    void releaseRead() {
        std::lock_guard<std::mutex> lk(m_);
        --activeReaders_;
        if (activeReaders_ == 0)
            cv_.notify_all();
    }

    void acquireWrite() {
        std::unique_lock<std::mutex> lk(m_);
        ++waitingWriters_;
        /* A writer enters only when the file is completely idle. */
        cv_.wait(lk, [&] { return !writerActive_ && activeReaders_ == 0; });
        --waitingWriters_;
        writerActive_ = true;
    }

    void releaseWrite() {
        std::lock_guard<std::mutex> lk(m_);
        writerActive_ = false;
        cv_.notify_all();
    }

private:
    /* One mutex and one condition variable are enough for this lock implementation. */
    std::mutex m_;
    std::condition_variable cv_;
    int activeReaders_ = 0;
    int waitingWriters_ = 0;
    bool writerActive_ = false;
};
