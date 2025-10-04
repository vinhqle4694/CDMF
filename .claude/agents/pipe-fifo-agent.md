---
name: pipe-fifo-agent
description: Pipes (named/anonymous) for IPC
tools: Read, Write
model: sonnet
color: yellow
---

# Pipe FIFO Agent

Implement pipe and FIFO-based inter-process communication.

## Deliverables
- `workspace/src/ipc/pipe_ipc.cpp`
- Anonymous and named pipes
- Non-blocking I/O support

## Focus
- pipe() for anonymous pipes
- mkfifo() for named pipes
- O_NONBLOCK handling
- EOF detection

## Quality
- Proper error handling
- Resource cleanup
- Tests ≥80% coverage
