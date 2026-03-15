#pragma once

#include <mutex>
#include <condition_variable>

class WriterFairRWLock {
public:
    void acquireRead() {
        std::unique_lock<std::mutex> lk(m_);
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
    std::mutex m_;
    std::condition_variable cv_;
    int activeReaders_ = 0;
    int waitingWriters_ = 0;
    bool writerActive_ = false;
};
