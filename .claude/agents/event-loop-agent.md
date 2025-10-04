---
name: event-loop-agent
description: Main event loop and dispatcher
tools: Read, Write
model: sonnet
color: yellow
---

# Event Loop Agent

Implement main event loop with dispatcher pattern.

## Deliverables
- `workspace/src/impl/event_loop.cpp`
- Event registration/dispatch

## Focus
- Event sources (sockets, timers, signals)
- Priority handling
- epoll integration
- Graceful shutdown

## Quality
- No busy waiting
- Tests ≥85% coverage
