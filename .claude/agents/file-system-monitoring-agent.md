---
name: file-system-monitoring-agent
description: inotify for file system change notifications
tools: Read, Write
model: sonnet
color: yellow
---

# File System Monitoring Agent

Implement file system monitoring with inotify.

## Deliverables
- `workspace/src/impl/fs_monitor.cpp`
- Event handling

## Focus
- inotify_init/inotify_add_watch
- Event types (CREATE, MODIFY, DELETE)
- Recursive monitoring
- Event coalescing

## Quality
- Event ordering
- Tests ≥75% coverage
