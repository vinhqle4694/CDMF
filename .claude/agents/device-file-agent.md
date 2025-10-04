---
name: device-file-agent
description: /dev access and ioctl operations
tools: Read, Write
model: sonnet
color: yellow
---

# Device File Agent

Implement device file access with ioctl operations.

## Deliverables
- `workspace/src/impl/device_io.cpp`
- ioctl wrappers

## Focus
- open /dev/* devices
- ioctl commands
- Device capability queries
- Non-blocking I/O

## Quality
- Permission checks
- Tests ≥70% coverage
