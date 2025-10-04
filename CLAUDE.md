# 1.0 Overview

## 1.1 Document Purpose
This document defines an agent-based software development system where specialized AI agents collaborate via file-based communication to produce Linux embedded C++ software from requirements to deployment.

## 1.2 Key Changes in Version 3.0
File separation:
- /ai-comm folder: AI agent communication (INPUT, OUTPUT, HANDOVER, coordination)
- /workspace folder: Deliverables (source, docs, plantuml diagrams, tests)

Benefits:
- Clean separation between orchestration and deliverables
- Easier to archive or exclude AI communication files
- Clearer workspace containing only production assets
- Better version control (optionally .gitignore ai-comm)
- Simple extraction of deployable artifacts

Version 3.0 Improvements:
- Enhanced error handling with retry mechanisms and timeout specifications
- Advanced dependency management with conditional and typed dependencies
- Progress tracking with heartbeat mechanisms for long-running tasks
- Workspace permission model preventing write conflicts
- Human-in-the-loop approval workflow for critical decisions
- Comprehensive validation layer for all JSON schemas
- Incremental build support with file change detection
- Standardized audit trail and logging framework

## 1.3 Key Principles
- Specialization: Each agent has a singular focused responsibility (1 agent = 1 specific task)
- File-based communication: Standardized JSON contracts with validation
- Separation of concerns: Coordination vs deliverables
- Transparency: Fully auditable actions and decisions
- Parallel execution: Independent tasks can run concurrently
- Dependency management: Explicit ordering via typed dependency graph
- Error resilience: Retry mechanisms, timeouts, and failure recovery
- Resource management: Workspace permissions and conflict prevention
- Incremental builds: Skip unchanged components for efficiency

## 1.4 System Components (Conceptual)
```text
┌─────────────────────────────────────────────────────┐
│                     USER                            │
│          (Provides Requirements)                    │
└─────────────────────┬───────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────┐
│          MASTER ORCHESTRATOR AGENT                  │
│      (Central Coordination & Delegation)            │
│                                                     │
│   Manages: /ai-comm (coordination)                  │
│   Creates: /workspace (deliverables)                │
└────────────┬─────────────────┬──────────────────────┘
             ↓                 ↓
    ┌────────────────┐  ┌─────────────────┐
    │ TASK PLANNING  │  │  SPECIALIZED    │
    │     AGENT      │  │     AGENTS      │
    └────────────────┘  └─────────────────┘
```

# 2.0 Workspace Structure

## 2.1 Root Layout
```text
project_root/
  user_input/
  ai-comm/
  workspace/
```

## 2.2 AI Communication (ai-comm/) Key Subdirectories
- 00_coordination (project_manifest.json, execution_plan.json, task_status.json)
- 01_design
- 02_implementation
- 03_testing
- 04_quality
- 05_documentation
- 06_build_deploy
- 07_maintenance
- 08_optimization
- 09_security_safety
- logs/ (agent execution logs, audit trails)
- validation/ (schema validation results)

## 2.3 Workspace (workspace/) Key Subdirectories
- design
- src (include, impl, ipc, network, utils)
- tests (unit, integration, performance, fixtures)
- docs (api, architecture, user, development)
- build (CMakeLists.txt, cmake/, Makefile, toolchain/)
- ci (.gitlab-ci.yml, jenkins/, scripts/)
- config (server.conf, logging.conf)
- quality (misra, static_analysis, coverage, performance)
- release (versioned, latest)

# 3.0 File Communication Protocol
All AI communication JSON structured files reside under /ai-comm.

## 3.1 INPUT.json Schema (Enhanced)
```json
{
  "delegation_id": "DEL_XXX",
  "task_id": "TASK_X_X",
  "from_agent": "Agent Name",
  "to_agent": "Target Agent Name",
  "timestamp": "2025-10-02T10:00:00Z",
  "status": "READY",
  "timeout_minutes": 120,
  "max_retries": 3,
  "skip_if_unchanged": false,
  "requires_human_approval": false,
  "task": {"task_name": "Task Name", "description": "What the agent needs to do"},
  "input_documents": [
    "workspace/design/architecture/system_architecture.md",
    "ai-comm/01_design/system_architecture/HANDOVER.json"
  ],
  "workspace_paths": {
    "read_from": ["workspace/design/architecture/", "workspace/src/include/"],
    "write_to": "workspace/src/impl/",
    "permissions": {
      "exclusive_write": true,
      "allowed_extensions": [".cpp", ".h"]
    }
  },
  "requirements_ref": "ai-comm/00_coordination/requirements/REQ_001.json",
  "expected_outputs": {
    "workspace_deliverables": [
      "workspace/src/impl/tcp_server.cpp",
      "workspace/src/include/tcp_server.h"
    ],
    "ai_comm_files": [
      "ai-comm/02_implementation/tcp_socket/OUTPUT.json",
      "ai-comm/02_implementation/tcp_socket/HANDOVER.json"
    ],
    "validation_required": true
  },
  "incremental_build": {
    "enabled": true,
    "input_checksums": {
      "workspace/design/architecture/system_architecture.md": "sha256:abc123..."
    }
  }
}
```

## 3.2 OUTPUT.json Schema (Enhanced)
```json
{
  "task_id": "TASK_X_X",
  "agent": "Agent Name",
  "status": "COMPLETED",
  "started_at": "2025-10-02T10:00:00Z",
  "completed_at": "2025-10-02T14:00:00Z",
  "duration_hours": 4.0,
  "progress_percentage": 100,
  "retry_count": 0,
  "workspace_deliverables": {
    "source_files": ["workspace/src/impl/tcp_server.cpp", "workspace/src/include/tcp_server.h"],
    "documentation": ["workspace/docs/api/tcp_server_api.md"],
    "diagrams": ["workspace/design/diagrams/tcp_flow.png"],
    "tests": ["workspace/tests/unit/test_tcp_server.cpp"]
  },
  "validation_status": {
    "schema_valid": true,
    "files_exist": true,
    "compilation_passed": true,
    "tests_passed": true,
    "test_results": {
      "total": 15,
      "passed": 15,
      "failed": 0,
      "coverage_percent": 92.5
    }
  },
  "key_decisions": [
    "Used epoll for I/O multiplexing",
    "Implemented connection pool with max 1024 connections"
  ],
  "assumptions": [
    "Linux kernel >= 2.6 for epoll support",
    "Single-threaded event loop (multi-threading handled separately)"
  ],
  "metrics": {
    "lines_of_code": 250,
    "functions_created": 8,
    "classes_created": 2,
    "test_cases_written": 15,
    "memory_usage_kb": 1024,
    "cpu_time_seconds": 0.5
  },
  "error": null,
  "logs_ref": "ai-comm/logs/tcp_socket_agent_20251002.log",
  "audit_trail_ref": "ai-comm/logs/audit/TASK_X_X_audit.json"
}
```

## 3.2.1 OUTPUT.json Status Values
- `COMPLETED`: Task finished successfully with all validations passed
- `FAILED`: Task failed, see `error` field for details
- `IN_PROGRESS`: Task is running, check `progress_percentage`
- `BLOCKED`: Task blocked by dependencies or resources, see `error.blocking_reason`
- `REQUIRES_APPROVAL`: Waiting for human approval
- `SKIPPED`: Task skipped due to unchanged inputs (incremental build)

## 3.3 HANDOVER.json Schema (Enhanced)
```json
{
  "handover_id": "HO_XXX",
  "from_agent": "Current Agent Name",
  "to_agents": ["Next Agent 1", "Next Agent 2"],
  "timestamp": "2025-10-02T14:00:00Z",
  "summary": "TCP server implementation completed with epoll support",
  "context": {
    "key_information": "Server uses non-blocking sockets",
    "decisions_made": ["Max connections: 1024", "Connection timeout: 30 seconds"],
    "workspace_files_to_reference": [
      "workspace/src/impl/tcp_server.cpp",
      "workspace/src/include/tcp_server.h"
    ]
  },
  "assumptions": [
    "Linux kernel >= 2.6 for epoll support",
    "Single-threaded event loop (multi-threading handled separately)"
  ],
  "blocking_issues": [],
  "suggested_next_steps": [
    "Integrate with multi-threading agent for connection handling",
    "Add SSL/TLS support via network-security-agent"
  ],
  "for_multi_threading_agent": {
    "task": "Integrate thread pool with TCP server",
    "reference_files": ["workspace/src/impl/tcp_server.cpp"],
    "integration_points": [
      "TCPServer::handleConnection() - dispatch to thread pool"
    ],
    "priority": "HIGH"
  }
}
```

## 3.4 Error Schema
```json
{
  "error": {
    "code": "ERR_COMPILATION_FAILED",
    "message": "Compilation failed for tcp_server.cpp",
    "details": "Missing header file: tcp_common.h",
    "blocking_reason": "Required dependency not available",
    "retry_possible": true,
    "suggested_action": "Wait for header generation from component-design-agent"
  }
}
```

## 3.5 Coordination Files

### project_manifest.json
```json
{
  "project_id": "TCP_SERVER_20251002",
  "project_name": "Multi-threaded TCP Server",
  "version": "1.0.0",
  "status": "IN_PROGRESS",
  "current_phase": "IMPLEMENTATION",
  "workspace_root": "workspace/",
  "ai_comm_root": "ai-comm/",
  "workspace_structure": {
    "design": "workspace/design/",
    "source_code": "workspace/src/",
    "tests": "workspace/tests/",
    "documentation": "workspace/docs/",
    "build": "workspace/build/"
  },
  "active_tasks": [
    {
      "task_id": "TASK_2_1",
      "agent": "TCP Socket Agent",
      "status": "IN_PROGRESS",
      "ai_comm_path": "ai-comm/02_implementation/tcp_socket/",
      "workspace_output": "workspace/src/impl/"
    }
  ],
  "metrics": {"total_tasks": 43, "completed": 15, "in_progress": 3, "pending": 25}
}
```

### execution_plan.json
```json
{
  "plan_id": "PLAN_001",
  "project_id": "TCP_SERVER_20251002",
  "total_duration_weeks": 6,
  "total_tasks": 43,
  "max_parallel_agents": 10,
  "resource_limits": {
    "max_cpu_percent": 80,
    "max_memory_gb": 16
  },
  "phases": [
    {
      "phase_id": "PHASE_1",
      "phase_name": "Design Phase",
      "ai_comm_path": "ai-comm/01_design/",
      "workspace_output_path": "workspace/design/",
      "tasks": [
        {
          "task_id": "TASK_1_1",
          "task_name": "System Architecture Design",
          "assigned_agent": "System Architecture Agent",
          "ai_comm_workspace": "ai-comm/01_design/system_architecture/",
          "workspace_output": "workspace/design/architecture/",
          "dependencies": [],
          "priority": "CRITICAL",
          "timeout_minutes": 180,
          "requires_human_approval": true
        }
      ]
    }
  ],
  "dependency_graph": {
    "TASK_1_2": {
      "depends_on": ["TASK_1_1"],
      "dependency_type": "REQUIRED",
      "blocking": true,
      "conditions": {
        "if_task_fails": "ABORT_PHASE",
        "if_task_skipped": "CONTINUE"
      }
    },
    "TASK_2_1": {
      "depends_on": ["TASK_1_2"],
      "dependency_type": "REQUIRED",
      "blocking": true,
      "conditions": {
        "if_task_fails": "RETRY_THEN_ABORT"
      }
    }
  }
}
```

# 4.0 Complete Workflow

## 4.1 Phase 0: Initialization
```text
1. User provides: user_input/software_requirements.md
2. Master Orchestrator: create project_manifest.json and task_planning/INPUT.json
3. Task Planning Agent: produces execution_plan.json, OUTPUT.json, HANDOVER.json
4. Master transitions to Phase 1
```

## 4.2 Phase 1: Design
```text
TASK_1_1 System Architecture → produces architecture docs
TASK_1_2 Component Design → consumes TASK_1_1 outputs
```

## 4.3 Phase 2: Implementation
```text
TASK_2_1 TCP Socket Implementation
Reads: design architecture, component design handover
Writes: tcp_server.cpp / tcp_server.h / unit test
```

## 4.4 Agent Interaction Flow

### Specialized Agent Workflow
1. Read INPUT.json from ai-comm/{phase}/{agent}/INPUT.json
2. Read workspace files specified in INPUT.json workspace_paths.read_from
3. Perform the assigned task
4. Write deliverables to workspace paths specified in INPUT.json workspace_paths.write_to
5. Write OUTPUT.json to ai-comm/{phase}/{agent}/OUTPUT.json
6. Write HANDOVER.json to ai-comm/{phase}/{agent}/HANDOVER.json for next agents

### Master Orchestrator Workflow

#### Task Delegation
1. Validate task schema before delegation
2. Check workspace permissions and acquire locks
3. Create INPUT.json with:
   - Task details and timeout specifications
   - Workspace paths for reading and writing
   - Expected outputs and validation requirements
   - Incremental build checksums
4. Write INPUT.json to agent's ai-comm directory
5. Log delegation to audit trail

#### Task Monitoring
1. Check for task timeout
2. Read OUTPUT.json when available
3. Validate OUTPUT.json schema
4. Verify workspace deliverables exist
5. Validate tests passed and compilation succeeded
6. Handle different statuses:
   - **COMPLETED**: Release workspace lock, process HANDOVER.json, delegate next tasks
   - **FAILED**: Execute retry logic or failure recovery
   - **BLOCKED**: Wait for blocking conditions to resolve
   - **REQUIRES_APPROVAL**: Request human input
   - **IN_PROGRESS**: Update progress dashboard

#### Error Handling
1. Check if retry is possible and retry count < max_retries
2. If retry possible: Re-delegate task with incremented retry count
3. If retry exhausted: Check dependency conditions
   - ABORT_PHASE: Stop current phase
   - ABORT_PROJECT: Terminate project
   - SKIP: Skip dependent tasks, continue others
4. Log all failures to audit trail

#### Workspace Management
1. Acquire exclusive write locks before task delegation
2. Verify no conflicts with other active agents
3. Check file permissions and allowed extensions
4. Release locks after task completion or failure

# 5.0 Workflow Examples

## 5.1 TCP Socket Agent Flow

### Step 1: INPUT.json
```json
{
  "task_id": "TASK_2_1",
  "workspace_paths": {
    "read_from": ["workspace/design/architecture/", "workspace/design/components/"],
    "write_to": "workspace/src/"
  }
}
```

### Step 2: Execution
- Reads: system_architecture.md, component_specifications.json, component_design HANDOVER
- Performs: TCP server implementation with epoll support
- Writes: tcp_server.cpp, tcp_server.h, test_tcp_server.cpp

### Step 3: Output
- Writes OUTPUT.json with validation status
- Writes HANDOVER.json with context for next agents

## 5.2 Parallel Execution Example
```text
TCP Socket Agent → COMPLETED
Multi-Threading Agent → COMPLETED (runs in parallel)
Shared Memory Agent → COMPLETED (runs in parallel)

Next phase begins when all dependencies are satisfied
```

## 5.3 Post-Completion Structure
```text
ai-comm/ (archivable coordination files)
workspace/ (production deliverables)
```

# 6.0 Best Practices

## 6.1 File Organization
- Strict separation: AI coordination (ai-comm) vs deliverables (workspace)
- Source: workspace/src
- Tests: workspace/tests
- Docs: workspace/docs
- Build: workspace/build
- Quality: workspace/quality

## 6.2 Path References

### INPUT.json excerpt
```json
{
  "workspace_paths": {
    "read_from": ["workspace/design/", "workspace/src/include/"],
    "write_to": "workspace/src/impl/"
  }
}
```

### OUTPUT.json excerpt
```json
{
  "workspace_deliverables": {
    "source_files": ["workspace/src/impl/tcp_server.cpp"],
    "tests": ["workspace/tests/unit/test_tcp_server.cpp"]
  }
}
```

### HANDOVER.json excerpt
```json
{
  "workspace_files_to_reference": [
    "workspace/src/impl/tcp_server.cpp",
    "workspace/src/include/tcp_server.h"
  ]
}
```

## 6.3 Version Control
.gitignore recommendation:
```text
ai-comm/
# Always commit workspace
```

## 6.4 Archiving
```bash
tar -czf project-ai-comm.tar.gz ai-comm/
tar -czf project-deliverables.tar.gz workspace/
tar -czf project-release.tar.gz workspace/src/ workspace/docs/ workspace/build/ workspace/release/
```

## 6.5 Agent Implementation Guidelines
- Validate INPUT.json schema before execution
- Check incremental build: skip if inputs unchanged
- Read workspace files first
- Write all deliverables into workspace
- Use ai-comm only for coordination files (OUTPUT, HANDOVER, logs)
- Document decisions and assumptions in OUTPUT.json
- Reference workspace paths in HANDOVER.json
- Report blocking issues and suggested next steps in HANDOVER.json
- Validate inputs pre-execution
- Report errors with structured error schema
- Include comprehensive metrics (LOC, tests, performance)
- Update progress_percentage for long-running tasks
- Run tests and include results in validation_status
- Generate audit trail for all actions
- Request human approval when required
- Handle timeouts gracefully

## 6.6 Orchestrator Guidelines
- Validate all INPUT/OUTPUT JSON schemas
- Acquire workspace locks before task delegation
- Verify deliverables before marking task complete (file existence, tests passed, compilation)
- Monitor ai-comm + workspace continuously
- Check task timeouts and handle gracefully
- Implement retry logic with exponential backoff
- Log all delegations to audit trail
- Track metrics globally (progress, resource usage)
- Fail on missing deliverables with clear error messages
- Handle conditional dependencies (REQUIRED, OPTIONAL, BLOCKING)
- Execute failure actions (ABORT_PHASE, ABORT_PROJECT, SKIP, RETRY)
- Manage human approval workflow for critical tasks
- Enforce resource limits (max parallel agents, CPU, memory)
- Support incremental builds (skip unchanged tasks)
- Release workspace locks after task completion/failure

# 7.0 Summary

## 7.1 Core Benefits
- Clean separation (ai-comm vs workspace)
- Organized structure with clear ownership
- Deployment-friendly deliverables
- Supports parallel, dependency-aware execution
- Full traceability across artifacts

## 7.2 Version 3.0 Enhancements
- **Error Resilience**: Retry mechanisms, timeouts, structured error handling
- **Advanced Dependencies**: Conditional execution (REQUIRED, OPTIONAL, BLOCKING)
- **Progress Tracking**: Real-time progress updates with heartbeat monitoring
- **Validation Layer**: Schema validation for all JSON contracts
- **Resource Management**: Workspace locks, permission model, conflict prevention
- **Incremental Builds**: Skip unchanged tasks based on file checksums
- **Human-in-the-Loop**: Approval workflow for critical decisions
- **Audit Trail**: Complete logging of all agent actions and decisions
- **Test Integration**: Automated test execution and validation before task completion

## 7.3 Maintained Features
- Specialized agents (1 agent = 1 specific task)
- File-based JSON protocol
- Typed dependency graph with conditional execution
- Parallelization with resource limits
- Complete auditability and transparency

# 8.0 Appendix A: Full Directory Tree
```text
project_root/
  user_input/
    software_requirements.md
  ai-comm/
    00_coordination/
      project_manifest.json
      execution_plan.json
      task_status.json
      requirements/
        REQ_001.json
    01_design/
    02_implementation/
    03_testing/
    04_quality/
    05_documentation/
    06_build_deploy/
    07_maintenance/
    08_optimization/
    09_security_safety/
    logs/
      agent_execution/
        tcp_socket_agent_20251002.log
      audit/
        TASK_X_X_audit.json
    validation/
      schema_validation_results.json
  workspace/
    design/
      architecture/
      diagrams/
    src/
      include/
      impl/
      ipc/
      network/
      utils/
    tests/
      unit/
      integration/
      performance/
      fixtures/
    docs/
      api/
      architecture/
      user/
      development/
    build/
      CMakeLists.txt
      cmake/
      Makefile
      toolchain/
    ci/
      .gitlab-ci.yml
      jenkins/
      scripts/
    config/
      server.conf
      logging.conf
    quality/
      misra/
      static_analysis/
      coverage/
      performance/
    release/
      versioned/
      latest/
```

# 9.0 Appendix B: Task Lifecycle State Machine

```text
┌─────────┐
│ CREATED │
└────┬────┘
     ↓
┌─────────┐     timeout     ┌─────────┐
│  READY  │────────────────→│ TIMEOUT │
└────┬────┘                 └─────────┘
     ↓
┌──────────────┐
│ IN_PROGRESS  │←──────────┐
└────┬─────────┘           │
     ↓                     │
     ├─→ REQUIRES_APPROVAL │
     │         ↓           │
     │    [approved]       │
     │         ↓           │
     ├─→ BLOCKED           │
     │         ↓           │
     │    [unblocked]──────┘
     ↓
┌──────────┐
│ COMPLETED│
│  FAILED  │
│ SKIPPED  │
└──────────┘
```

## Task State Transitions
- **CREATED** → **READY**: Task created and waiting for dependencies
- **READY** → **IN_PROGRESS**: Agent starts execution
- **IN_PROGRESS** → **REQUIRES_APPROVAL**: Critical decision needs human input
- **REQUIRES_APPROVAL** → **IN_PROGRESS**: Human approves, task continues
- **IN_PROGRESS** → **BLOCKED**: Missing dependency or resource
- **BLOCKED** → **IN_PROGRESS**: Blocker resolved, task resumes
- **IN_PROGRESS** → **COMPLETED**: Successful completion with all validations passed
- **IN_PROGRESS** → **FAILED**: Unrecoverable error (after retries exhausted)
- **READY/IN_PROGRESS** → **TIMEOUT**: Task exceeded timeout limit
- **IN_PROGRESS** → **SKIPPED**: Incremental build detected no changes

# 10.0 Appendix C: Dependency Types and Actions

## Dependency Types
- **REQUIRED**: Task must complete successfully for dependent tasks to run
- **OPTIONAL**: Task completion preferred but not mandatory
- **BLOCKING**: Task must start/complete before dependent tasks can start
- **NON_BLOCKING**: Dependent tasks can run in parallel

## Failure Actions
- **ABORT_PHASE**: Stop current phase, skip all remaining tasks in phase
- **ABORT_PROJECT**: Terminate entire project execution
- **SKIP**: Skip dependent tasks only, continue other tasks
- **RETRY**: Retry failed task up to max_retries, then execute failure action
- **RETRY_THEN_ABORT**: Retry failed task, abort phase/project if retries exhausted
- **CONTINUE**: Ignore failure, continue with all tasks

## Conditional Execution Example
```json
{
  "TASK_3_5": {
    "depends_on": ["TASK_3_1", "TASK_3_2"],
    "dependency_type": "REQUIRED",
    "blocking": true,
    "conditions": {
      "if_task_fails": "RETRY_THEN_ABORT",
      "if_task_skipped": "CONTINUE",
      "if_timeout": "ABORT_PHASE"
    }
  }
}
```
