---
name: timer-agent
description: POSIX timers, timerfd, and high-resolution timing
tools: Read, Write
model: sonnet
color: yellow
---

# Timer Agent

Implement timers with high-resolution support.

## Deliverables
- `workspace/src/impl/timer.cpp`
- Periodic and one-shot timers

## Focus
- timer_create/timer_settime
- timerfd for event loops
- clock_gettime (MONOTONIC)
- Nanosecond precision

## Quality
- Drift measurement
- Tests ≥80% coverage
