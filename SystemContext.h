#pragma once

#include "StateTracker.h"
#include "SessionGate.h"
#include "UserDirectory.h"
#include "WriterFairRWLock.h"
#include "ConResServices.h"

/* SystemContext wires the shared objects together once so every thread uses the same state. */
struct SystemContext {
    StateTracker tracker;
    SessionGate gate;
    UserDirectory users;
    WriterFairRWLock fileRW;
    AuthenticationService authentication;
    SessionAdmissionControl admission;
    SessionStateManagement sessionState;
    MonitoringAndStatusTracking monitoring;
    ResourceAccessService resourceAccess;

    explicit SystemContext(int N)
        : tracker(),
          gate(N, tracker),
          users(),
          fileRW(),
          authentication(users),
          admission(gate),
          sessionState(tracker),
          monitoring(tracker),
          resourceAccess(fileRW, tracker) {}
};
