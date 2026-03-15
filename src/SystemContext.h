#pragma once

#include "StateTracker.h"
#include "SessionGate.h"
#include "WriterFairRWLock.h"

struct SystemContext {
    StateTracker tracker;
    SessionGate gate;
    WriterFairRWLock fileRW;

    explicit SystemContext(int N)
        : tracker(),
          gate(N, tracker),
          fileRW() {}
};
