---
name: multi-process-agent
description: Fork/exec, process management, and daemon creation
tools: Read, Write
model: sonnet
color: yellow
---

# Multi-Process Agent

Implement process management with fork/exec, daemons, and process monitoring.

## Deliverables
- `workspace/src/impl/process_manager.cpp`
- `workspace/src/include/process_manager.h`
- `workspace/tests/unit/test_process_manager.cpp`

## Implementation Focus
- fork/exec patterns
- Daemon creation (double fork)
- Process monitoring
- Graceful shutdown
- Resource cleanup

## Quality Gates
- Zombie processes prevented
- Signal handlers registered
- Resource leaks checked
- Unit tests ≥80% coverage

## Coordinates With
- signal-handling-agent
- system-call-agent
