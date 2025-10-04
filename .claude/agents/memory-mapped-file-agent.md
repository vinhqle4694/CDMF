---
name: memory-mapped-file-agent
description: mmap for file mapping
tools: Read, Write
model: sonnet
color: yellow
---

# Memory Mapped File Agent

Implement memory-mapped file I/O with mmap.

## Deliverables
- `workspace/src/impl/mmap_file.cpp`
- Shared/private mappings

## Focus
- mmap/munmap
- MAP_SHARED/MAP_PRIVATE
- msync for persistence
- Large file support

## Quality
- Sync guarantees
- Tests ≥80% coverage
