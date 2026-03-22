# ConRes Frontend

This folder contains the local visualization frontend for the ConRes simulation.

## Quick Start

From the repository root run:

```powershell
powershell -ExecutionPolicy Bypass -File .\launch_frontend.ps1
```

Then open:

`http://localhost:8000`

## What it shows

- active session slots
- waiting queue pressure
- concurrent readers
- exclusive writer ownership
- a replayable event timeline
- per-user read and write totals
- correctness checks for key synchronization rules
