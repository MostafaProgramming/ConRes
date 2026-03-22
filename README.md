# ConRes

ConRes is a C++20 concurrent resource access and synchronisation engine built for coursework on threads, semaphores, and readers-writer coordination. It simulates authorised engineers accessing a shared file, `ProductSpecification.txt`, while enforcing:

- a maximum number of active sessions
- a waiting queue for extra users
- multiple concurrent readers
- a single exclusive writer
- thread-safe state tracking and replayable evidence output

## What The System Demonstrates

- **Thread-based user sessions**: each authorised user runs as a separate thread.
- **Session admission control**: a counting semaphore limits concurrent active users to `N = 4`.
- **Waiting queue behaviour**: extra users are blocked until a slot becomes available.
- **Readers-writer synchronisation**: many readers can access the shared file together, while writers update it one at a time.
- **State visibility**: active users, waiting users, readers, writer, and statistics are tracked and displayed.
- **Manual shared-file demo**: the frontend can read and update the real `ProductSpecification.txt` through a small local API.

## Project Structure

- [main.cpp](./main.cpp): starts the system, spawns user threads, and exports replay data
- [SystemContext.h](./SystemContext.h): bundles shared runtime objects and service wrappers
- [ConResServices.h](./ConResServices.h): light service layer used to align the code structure with the design
- [SessionGate.h](./SessionGate.h): counting semaphore + waiting queue admission control
- [WriterFairRWLock.h](./WriterFairRWLock.h): writer-priority readers-writer lock
- [Guards.h](./Guards.h): RAII guards for login, reading, and writing
- [StateTracker.h](./StateTracker.h): thread-safe tracking, logging, and replay export
- [UserDirectory.h](./UserDirectory.h): predefined authorised users
- [FileOps.h](./FileOps.h): shared file helpers and simulated file activity
- [frontend/](./frontend): browser visualiser
- [frontend_server.py](./frontend_server.py): local server for frontend assets and manual shared-file API

## Requirements

- Windows PowerShell
- `g++` with C++20 support
- Python 3

## How To Run

### Option 1: Full Frontend Visualiser

From the project root:

```powershell
powershell -ExecutionPolicy Bypass -File .\launch_frontend.ps1
```

Then open:

- [http://localhost:8000](http://localhost:8000)

This command:

1. builds the C++ program
2. runs the concurrency simulation
3. generates replay data for the frontend
4. starts the local frontend/API server

### Option 2: Terminal Simulation Only

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
.\main.exe
```

This runs the simulation directly in the terminal and writes updates to the shared file.

## How To Use The Frontend

### Replay Mode

Use replay mode to demonstrate the recorded concurrent run:

- press `Play` to animate the event timeline
- inspect the active session panel and waiting queue
- observe concurrent readers and the current writer
- use the event feed, spotlight, and statistics to explain the run

### Manual Mode

Use manual mode to demonstrate the concurrency rules live:

1. click `Attempt Login` for up to four users
2. attempt a fifth login to show queueing
3. start multiple reads to show safe concurrent reading
4. request a write to show exclusive writer behaviour
5. stop the reads so the writer can proceed
6. type a manual update and save it to the shared file
7. show the live contents of `ProductSpecification.txt`

## Key Concurrency Rules

- at most **4 users** may be active at once
- extra users are placed in a **waiting queue**
- **multiple readers** may access the file concurrently
- **one writer** may update the file at a time
- queued writers block new readers to reduce writer starvation
- shared tracking state is protected with mutexes and atomic counters

## Output And Evidence

During execution, ConRes produces:

- console status output
- live frontend state and replay views
- updates to `ProductSpecification.txt`
- replay data for the browser visualiser

## Demo Tips

For coursework demonstration, the best flow is:

1. show replay mode for automatic concurrency evidence
2. show manual mode for live login, queueing, reading, writing, and file updates
3. briefly show `main.cpp`, `SessionGate.h`, and `WriterFairRWLock.h` as the critical code sections
