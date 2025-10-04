---
name: posix-semaphore-agent
description: Named/unnamed semaphores for resource counting
tools: Read, Write
model: sonnet
color: yellow
---

# POSIX Semaphore Agent

Implement POSIX semaphores for resource management.

## Deliverables
- `workspace/src/ipc/semaphore.cpp`
- Named and unnamed semaphores
- Timeout operations

## Focus
- sem_open (named)
- sem_init (unnamed)
- sem_timedwait
- Resource counting

## Quality
- No deadlocks
- Cleanup guaranteed
- Tests ≥80% coverage
