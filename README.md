# ConRes
ConRes Concurrency Simulation

This project simulates a concurrent resource management system
where multiple users access a shared file.

Features

- Thread-based user sessions
- Counting semaphore to limit concurrent users
- Writer-priority readers-writer lock
- Thread-safe state tracking
- Detailed execution logging
- Performance statistics

Technologies

- C++20
- std::thread
- std::mutex
- std::condition_variable
- std::counting_semaphore
