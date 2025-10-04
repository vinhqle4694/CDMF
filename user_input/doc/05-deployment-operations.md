# CDMF Deployment and Operations Guide

## Table of Contents
1. [Deployment Strategies](#deployment-strategies)
2. [Installation and Setup](#installation-and-setup)
3. [Configuration Management](#configuration-management)
4. [Module Deployment](#module-deployment)
5. [Process Management](#process-management)
6. [Monitoring and Observability](#monitoring-and-observability)
7. [Security Hardening](#security-hardening)
8. [High Availability](#high-availability)
9. [Backup and Recovery](#backup-and-recovery)
10. [Operational Procedures](#operational-procedures)
11. [Troubleshooting](#troubleshooting)
12. [Production Checklist](#production-checklist)

## Deployment Strategies

### Single-Process Deployment

All modules run within a single process for maximum performance.

#### Architecture
```
┌─────────────────────────────────────┐
│         Application Process         │
│                                     │
│  ┌──────────┐  ┌──────────┐         │
│  │  Core    │  │  Module  │         │
│  │ Framework│  │    A     │         │
│  └────┬─────┘  └────┬─────┘         │
│       │             │               │
│  ┌────▼─────────────▼────┐           │
│  │   Service Registry    │           │
│  └───────────────────────┘           │
└─────────────────────────────────────┘
```

#### When to Use
- All modules are trusted
- Maximum performance required
- Simple deployment model preferred
- Limited operational complexity acceptable

#### Configuration
```json
{
  "deployment": {
    "type": "single-process",
    "enable_ipc": false,
    "module_isolation": "none"
  }
}
```

### Multi-Process Deployment

Modules run in separate processes for isolation and fault tolerance.

#### Architecture
```
┌─────────────────────────────────────┐
│         Main Process                │
│  ┌──────────┐  ┌──────────┐         │
│  │  Core    │  │ Trusted  │         │
│  │ Framework│  │ Modules  │         │
│  └────┬─────┘  └────┬─────┘         │
│       └─────────────┘               │
│             │ IPC                   │
└─────────────┼───────────────────────┘
              │
┌─────────────▼───────────────────────┐
│       Module Process 1              │
│  ┌──────────┐                       │
│  │ Untrusted│                       │
│  │  Module  │                       │
│  └──────────┘                       │
└─────────────────────────────────────┘
```

#### When to Use
- Third-party modules present
- Fault isolation required
- Security sandboxing needed
- Resource limits per module

#### Configuration
```json
{
  "deployment": {
    "type": "multi-process",
    "enable_ipc": true,
    "ipc_transport": "unix-socket",
    "process_manager": {
      "enabled": true,
      "max_restarts": 3,
      "restart_delay_ms": 1000
    }
  }
}
```

### Hybrid Deployment

Combination of in-process and out-of-process modules.

#### Architecture
```
Main Process:
├── Core Framework
├── Trusted Modules (in-process)
│   ├── Logger
│   ├── Configuration
│   └── Core Business Logic
│
└── IPC Proxies
    ├── Untrusted Plugin (separate process)
    ├── Resource-Intensive Module (separate process)
    └── Remote Service (network)
```

#### Configuration
```json
{
  "deployment": {
    "type": "hybrid",
    "enable_ipc": true,
    "module_policies": [
      {
        "pattern": "com.company.*",
        "location": "in-process"
      },
      {
        "pattern": "com.thirdparty.*",
        "location": "out-of-process",
        "sandbox": true
      }
    ]
  }
}
```

### Containerized Deployment

Deploy CDMF in Docker/Kubernetes environments.

#### Dockerfile
```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -m -s /bin/bash cdmf

# Copy framework
COPY --chown=cdmf:cdmf bin/cdmf-framework /app/bin/
COPY --chown=cdmf:cdmf lib/*.so /app/lib/
COPY --chown=cdmf:cdmf modules/*.so /app/modules/

# Set up environment
ENV LD_LIBRARY_PATH=/app/lib
ENV CDMF_HOME=/app
ENV CDMF_MODULES=/app/modules

# Create data directories
RUN mkdir -p /data/storage /data/logs && \
    chown -R cdmf:cdmf /data

USER cdmf
WORKDIR /app

# Health check
HEALTHCHECK --interval=30s --timeout=3s \
  CMD /app/bin/cdmf-cli status || exit 1

# Start framework
CMD ["/app/bin/cdmf-framework", "--config", "/app/config/production.json"]
```

#### Kubernetes Deployment
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: cdmf-application
spec:
  replicas: 3
  selector:
    matchLabels:
      app: cdmf
  template:
    metadata:
      labels:
        app: cdmf
    spec:
      containers:
      - name: cdmf
        image: company/cdmf:1.0.0
        ports:
        - containerPort: 8080
        env:
        - name: CDMF_ENV
          value: "production"
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
          limits:
            memory: "2Gi"
            cpu: "2000m"
        volumeMounts:
        - name: config
          mountPath: /app/config
        - name: modules
          mountPath: /app/modules
        - name: data
          mountPath: /data
        livenessProbe:
          exec:
            command:
            - /app/bin/cdmf-cli
            - status
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 5
      volumes:
      - name: config
        configMap:
          name: cdmf-config
      - name: modules
        persistentVolumeClaim:
          claimName: cdmf-modules-pvc
      - name: data
        persistentVolumeClaim:
          claimName: cdmf-data-pvc
```

## Installation and Setup

### System Requirements

#### Hardware
- **CPU**: 2+ cores recommended
- **RAM**: 4GB minimum, 8GB+ recommended
- **Disk**: 10GB for framework and modules
- **Network**: 1Gbps for distributed deployments

#### Software
- **OS**: Linux (Ubuntu 20.04+, RHEL 8+), Windows 10+, macOS 10.15+
- **Compiler**: GCC 8+, Clang 7+, MSVC 2019+
- **Runtime**: glibc 2.27+, libstdc++ 6.0.25+

### Installation Methods

#### Package Manager Installation

**Ubuntu/Debian:**
```bash
# Add repository
sudo add-apt-repository ppa:cdmf/stable
sudo apt-get update

# Install framework
sudo apt-get install cdmf-framework cdmf-dev

# Verify installation
cdmf-cli --version
```

**RHEL/CentOS:**
```bash
# Add repository
sudo yum-config-manager --add-repo https://cdmf.org/rpm/cdmf.repo

# Install framework
sudo yum install cdmf-framework cdmf-devel

# Verify installation
cdmf-cli --version
```

**macOS:**
```bash
# Using Homebrew
brew tap cdmf/cdmf
brew install cdmf

# Verify installation
cdmf-cli --version
```

#### Source Installation

```bash
# Clone repository
git clone https://github.com/cdmf/cdmf.git
cd cdmf

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Install
sudo make install
sudo ldconfig

# Verify
cdmf-cli --version
```

### Directory Structure

Standard installation layout:

```
/opt/cdmf/                     # Installation root
├── bin/                       # Executables
│   ├── cdmf-framework        # Main framework executable
│   ├── cdmf-cli              # Command-line interface
│   └── cdmf-monitor          # Monitoring daemon
├── lib/                       # Core libraries
│   ├── libcdmf-core.so
│   ├── libcdmf-ipc.so
│   └── libcdmf-security.so
├── modules/                   # Installed modules
│   ├── core/                 # Core modules
│   └── user/                 # User modules
├── config/                    # Configuration files
│   ├── framework.json
│   └── modules/
├── data/                      # Runtime data
│   ├── storage/              # Persistent storage
│   ├── cache/                # Cache directory
│   └── logs/                 # Log files
└── doc/                       # Documentation
```

### Initial Configuration

Create initial configuration file:

```json
// /opt/cdmf/config/framework.json
{
  "framework": {
    "name": "production-cdmf",
    "version": "1.0.0",
    "environment": "production"
  },
  "paths": {
    "modules": "/opt/cdmf/modules",
    "storage": "/opt/cdmf/data/storage",
    "cache": "/opt/cdmf/data/cache",
    "logs": "/opt/cdmf/data/logs"
  },
  "modules": {
    "auto_start": true,
    "start_timeout_ms": 10000,
    "scan_interval_s": 60,
    "hot_deploy": true
  },
  "services": {
    "registry_cache_size": 1000,
    "service_timeout_ms": 5000
  },
  "security": {
    "enabled": true,
    "verify_signatures": true,
    "trust_store": "/opt/cdmf/config/trust"
  },
  "logging": {
    "level": "INFO",
    "file": "/opt/cdmf/data/logs/framework.log",
    "max_size_mb": 100,
    "max_files": 10,
    "format": "json"
  },
  "monitoring": {
    "enabled": true,
    "metrics_port": 9090,
    "health_port": 8080
  }
}
```

## Configuration Management

### Configuration Hierarchy

Configuration is loaded in order of precedence:

1. Command-line arguments
2. Environment variables
3. Configuration files
4. Default values

### Environment Variables

```bash
# Framework configuration
export CDMF_HOME=/opt/cdmf
export CDMF_CONFIG=/opt/cdmf/config/production.json
export CDMF_MODULES=/opt/cdmf/modules
export CDMF_LOG_LEVEL=INFO
export CDMF_ENV=production

# Module-specific
export CDMF_MODULE_com_example_logger_LEVEL=DEBUG
export CDMF_MODULE_com_example_database_URL=postgres://localhost
```

### Dynamic Configuration

Update configuration without restart:

```bash
# Update module configuration
cdmf-cli config set com.example.logger level=DEBUG

# Update framework configuration
cdmf-cli config set framework.monitoring.enabled=true

# Reload configuration
cdmf-cli config reload
```

### Configuration Templates

Use templates for different environments:

```json
// config/templates/base.json
{
  "framework": {
    "name": "${CDMF_APP_NAME}",
    "version": "${CDMF_VERSION}"
  },
  "paths": {
    "modules": "${CDMF_MODULES}",
    "storage": "${CDMF_STORAGE}"
  }
}

// config/templates/production.json
{
  "@extends": "base.json",
  "security": {
    "enabled": true,
    "verify_signatures": true
  },
  "logging": {
    "level": "WARN"
  }
}

// config/templates/development.json
{
  "@extends": "base.json",
  "security": {
    "enabled": false
  },
  "logging": {
    "level": "DEBUG"
  }
}
```

## Module Deployment

### Module Installation

#### Manual Installation
```bash
# Install single module
cdmf-cli module install /path/to/module.so

# Install with manifest
cdmf-cli module install /path/to/module-package.zip

# Install from repository
cdmf-cli module install com.example.logger:1.2.3
```

#### Hot Deployment
```bash
# Enable hot deployment
cdmf-cli config set modules.hot_deploy=true

# Copy module to hot deploy directory
cp new-module.so /opt/cdmf/modules/hot-deploy/

# Module automatically detected and installed
```

#### Batch Installation
```bash
# Install from manifest file
cat modules.txt
com.example.logger:1.2.3
com.example.database:2.0.1
com.example.cache:1.5.0

cdmf-cli module install-batch modules.txt
```

### Module Updates

#### Zero-Downtime Updates
```bash
# Update running module
cdmf-cli module update com.example.trading-engine --version 1.2.1

# Output:
# Preparing update...
# Installing new version 1.2.1...
# Stopping version 1.2.0...
# Starting version 1.2.1...
# Update completed successfully (750ms)
# Active connections preserved: 147
```

#### Rollback Procedure
```bash
# Rollback to previous version
cdmf-cli module rollback com.example.trading-engine

# Rollback to specific version
cdmf-cli module rollback com.example.trading-engine --version 1.1.9
```

### Module Dependencies

#### Dependency Resolution
```bash
# Check dependencies before installation
cdmf-cli module check-deps new-module.so

# Output:
# Required modules:
#   ✓ com.example.logger [1.0.0, 2.0.0) - satisfied (1.2.3 installed)
#   ✗ com.example.cache [2.0.0, 3.0.0) - NOT satisfied
#
# Missing dependencies must be installed first
```

#### Dependency Graph
```bash
# Visualize dependencies
cdmf-cli module deps --graph

# Output:
# com.example.app (1.0.0)
# ├── com.example.logger (1.2.3)
# │   └── com.example.common (0.5.1)
# ├── com.example.database (2.0.1)
# │   ├── com.example.pool (1.0.0)
# │   └── com.example.common (0.5.1)
# └── com.example.cache (1.5.0)
```

## Process Management

### Process Isolation

Configure process isolation for modules:

```json
// module-policy.json
{
  "policies": [
    {
      "module": "com.thirdparty.*",
      "process": {
        "isolation": "separate",
        "user": "cdmf-sandbox",
        "limits": {
          "memory_mb": 512,
          "cpu_percent": 25,
          "open_files": 100
        },
        "sandbox": {
          "type": "seccomp",
          "profile": "strict"
        }
      }
    }
  ]
}
```

### Process Monitoring

```bash
# List all processes
cdmf-cli process list

# Output:
# PID    Module                        Status    CPU%  Memory
# 12345  framework                     running   2.1   245MB
# 12346  com.example.logger           running   0.5   32MB
# 12347  com.thirdparty.plugin        running   15.2  128MB

# Monitor specific process
cdmf-cli process monitor 12347
```

### Process Control

```bash
# Restart process
cdmf-cli process restart 12347

# Kill and restart
cdmf-cli process kill 12347 --restart

# Adjust resource limits
cdmf-cli process limit 12347 --memory 1024 --cpu 50
```

## Monitoring and Observability

### Metrics Collection

#### Prometheus Integration
```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'cdmf'
    static_configs:
      - targets: ['localhost:9090']
    metrics_path: '/metrics'
```

#### Available Metrics
```
# Framework metrics
cdmf_framework_uptime_seconds
cdmf_framework_modules_total
cdmf_framework_services_total

# Module metrics
cdmf_module_state{module="com.example.logger"}
cdmf_module_start_time_seconds{module="com.example.logger"}
cdmf_module_restarts_total{module="com.example.logger"}

# Service metrics
cdmf_service_calls_total{service="ILogger"}
cdmf_service_errors_total{service="ILogger"}
cdmf_service_latency_seconds{service="ILogger"}

# IPC metrics
cdmf_ipc_messages_total{channel="unix-socket"}
cdmf_ipc_bytes_transferred{channel="unix-socket"}
cdmf_ipc_errors_total{channel="unix-socket"}
```

### Logging

#### Log Configuration
```json
{
  "logging": {
    "outputs": [
      {
        "type": "file",
        "path": "/var/log/cdmf/framework.log",
        "level": "INFO",
        "format": "json",
        "rotation": {
          "max_size_mb": 100,
          "max_files": 10,
          "compress": true
        }
      },
      {
        "type": "syslog",
        "facility": "local0",
        "level": "WARN"
      },
      {
        "type": "console",
        "level": "ERROR",
        "format": "text"
      }
    ]
  }
}
```

#### Structured Logging
```json
{
  "timestamp": "2025-01-15T10:30:45.123Z",
  "level": "INFO",
  "module": "com.example.trading",
  "service": "TradingEngine",
  "message": "Order processed",
  "context": {
    "order_id": "12345",
    "symbol": "AAPL",
    "quantity": 100,
    "price": 150.25,
    "latency_ms": 23
  }
}
```

### Health Checks

#### HTTP Health Endpoint
```bash
# Check health
curl http://localhost:8080/health

# Response:
{
  "status": "healthy",
  "timestamp": "2025-01-15T10:30:45.123Z",
  "framework": {
    "version": "1.0.0",
    "uptime_seconds": 3600,
    "state": "ACTIVE"
  },
  "modules": {
    "total": 10,
    "active": 10,
    "failed": 0
  },
  "services": {
    "registered": 25,
    "available": 25
  }
}
```

#### Custom Health Checks
```cpp
class DatabaseHealthCheck : public IHealthCheck {
public:
    HealthStatus check() override {
        try {
            db_->ping();
            return HealthStatus::HEALTHY;
        } catch (const std::exception& e) {
            return HealthStatus::UNHEALTHY;
        }
    }
};
```

### Distributed Tracing

#### OpenTelemetry Integration
```cpp
// In module code
auto tracer = opentelemetry::trace::Provider::GetTracer("cdmf");

auto span = tracer->StartSpan("process-order");
span->SetAttribute("order.id", orderId);
span->SetAttribute("order.value", orderValue);

// Process order...

span->End();
```

## Security Hardening

### Module Signing

#### Generate Signing Key
```bash
# Generate key pair
openssl genrsa -out cdmf-private.key 4096
openssl rsa -in cdmf-private.key -pubout -out cdmf-public.key

# Sign module
cdmf-sign --key cdmf-private.key --module my-module.so

# Creates my-module.so.sig
```

#### Verify Signatures
```json
{
  "security": {
    "verify_signatures": true,
    "trust_store": "/opt/cdmf/config/trust",
    "required_signers": [
      "CN=Example Corp,O=Example,C=US"
    ]
  }
}
```

### Sandboxing

#### Seccomp Profile
```json
{
  "defaultAction": "SCMP_ACT_ERRNO",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [
    {
      "names": ["read", "write", "open", "close"],
      "action": "SCMP_ACT_ALLOW"
    },
    {
      "names": ["socket", "connect"],
      "action": "SCMP_ACT_ERRNO"
    }
  ]
}
```

#### AppArmor Profile
```
#include <tunables/global>

/opt/cdmf/modules/untrusted/** {
  #include <abstractions/base>

  # Allow read access to module files
  /opt/cdmf/modules/** r,

  # Allow write to specific directories
  /tmp/cdmf/** rw,
  /var/log/cdmf/** w,

  # Deny network access
  deny network,

  # Deny capability changes
  deny capability,
}
```

### Access Control

#### Permission Configuration
```json
{
  "permissions": [
    {
      "module": "com.thirdparty.plugin",
      "allow": [
        "service:com.example.ILogger:*",
        "file:/tmp/plugin/**:rw"
      ],
      "deny": [
        "service:com.example.IDatabase:*",
        "network:*"
      ]
    }
  ]
}
```

## High Availability

### Active-Passive Setup

```
┌─────────────────┐     ┌─────────────────┐
│   Active Node   │────│  Passive Node   │
│                 │     │                 │
│  CDMF Framework │     │  CDMF Framework │
│    (running)    │     │   (standby)     │
└────────┬────────┘     └────────┬────────┘
         │                       │
         └───────────┬───────────┘
                     │
              Shared Storage
            (modules, config)
```

Configuration:
```json
{
  "ha": {
    "enabled": true,
    "mode": "active-passive",
    "heartbeat_interval_ms": 1000,
    "takeover_timeout_ms": 5000,
    "shared_storage": "/mnt/cdmf-shared"
  }
}
```

### Active-Active Setup

```
       Load Balancer
            │
     ┌──────┴──────┐
     │             │
┌─────▼───┐   ┌────▼────┐
│  Node 1 │   │  Node 2 │
│  Active │   │  Active │
└─────┬───┘   └────┬────┘
      │            │
      └────┬───────┘
           │
    Shared Services
   (Database, Cache)
```

Configuration:
```json
{
  "ha": {
    "enabled": true,
    "mode": "active-active",
    "cluster": {
      "nodes": [
        "node1.example.com",
        "node2.example.com"
      ],
      "coordination": "etcd://cluster.example.com:2379"
    }
  }
}
```

## Backup and Recovery

### Backup Strategy

#### Automated Backups
```bash
#!/bin/bash
# backup-cdmf.sh

BACKUP_DIR="/backup/cdmf/$(date +%Y%m%d)"
mkdir -p $BACKUP_DIR

# Backup configuration
cp -r /opt/cdmf/config $BACKUP_DIR/

# Backup modules
cp -r /opt/cdmf/modules $BACKUP_DIR/

# Backup data
rsync -av /opt/cdmf/data/storage $BACKUP_DIR/

# Create manifest
cat > $BACKUP_DIR/manifest.json << EOF
{
  "timestamp": "$(date -Iseconds)",
  "version": "$(cdmf-cli version)",
  "modules": $(cdmf-cli module list --json)
}
EOF

# Compress
tar -czf $BACKUP_DIR.tar.gz $BACKUP_DIR
rm -rf $BACKUP_DIR
```

#### Backup Schedule
```cron
# Crontab entry
0 2 * * * /opt/cdmf/scripts/backup-cdmf.sh
```

### Recovery Procedures

#### Full Recovery
```bash
# Stop framework
systemctl stop cdmf

# Extract backup
tar -xzf /backup/cdmf/20250115.tar.gz -C /

# Restore configuration
cp -r /backup/cdmf/20250115/config/* /opt/cdmf/config/

# Restore modules
cp -r /backup/cdmf/20250115/modules/* /opt/cdmf/modules/

# Restore data
rsync -av /backup/cdmf/20250115/storage/* /opt/cdmf/data/storage/

# Start framework
systemctl start cdmf

# Verify
cdmf-cli status
```

#### Module Recovery
```bash
# Recover specific module
cdmf-cli module recover com.example.logger --backup /backup/modules/logger-1.2.3.so

# Verify module
cdmf-cli module status com.example.logger
```

## Operational Procedures

### Startup Procedure

```bash
#!/bin/bash
# startup-cdmf.sh

echo "Starting CDMF Framework..."

# Pre-checks
if ! cdmf-cli check-config; then
    echo "Configuration check failed"
    exit 1
fi

# Clean temporary files
rm -rf /tmp/cdmf/*

# Start framework
systemctl start cdmf

# Wait for framework
timeout 30 bash -c 'until cdmf-cli status > /dev/null 2>&1; do sleep 1; done'

# Verify core modules
REQUIRED_MODULES="logger database cache"
for module in $REQUIRED_MODULES; do
    if ! cdmf-cli module status com.example.$module | grep -q ACTIVE; then
        echo "Required module $module not active"
        exit 1
    fi
done

echo "CDMF Framework started successfully"
```

### Shutdown Procedure

```bash
#!/bin/bash
# shutdown-cdmf.sh

echo "Shutting down CDMF Framework..."

# Notify users
cdmf-cli broadcast "System shutdown in 5 minutes"

# Wait for active operations
sleep 300

# Stop accepting new requests
cdmf-cli set-mode maintenance

# Graceful module shutdown
cdmf-cli module stop-all --timeout 30

# Stop framework
systemctl stop cdmf

echo "CDMF Framework stopped"
```

### Maintenance Mode

```bash
# Enter maintenance mode
cdmf-cli set-mode maintenance

# Perform maintenance
cdmf-cli module update com.example.database
cdmf-cli config reload
cdmf-cli cache clear

# Exit maintenance mode
cdmf-cli set-mode normal
```

## Troubleshooting

### Common Issues

#### Framework Won't Start

```bash
# Check logs
tail -f /opt/cdmf/data/logs/framework.log

# Common causes:
# 1. Port already in use
lsof -i :8080

# 2. Permission issues
ls -la /opt/cdmf/data/

# 3. Configuration errors
cdmf-cli check-config --verbose
```

#### Module Won't Load

```bash
# Check module dependencies
ldd /opt/cdmf/modules/my-module.so

# Check module manifest
cdmf-cli module validate my-module.so

# Check logs for errors
grep "my-module" /opt/cdmf/data/logs/framework.log
```

#### Service Not Available

```bash
# List all services
cdmf-cli service list

# Check specific service
cdmf-cli service info com.example.ILogger

# Check module providing service
cdmf-cli module status com.example.logger
```

### Diagnostic Commands

```bash
# System status
cdmf-cli diag system

# Module diagnostics
cdmf-cli diag module com.example.trading

# Service diagnostics
cdmf-cli diag service com.example.IDatabase

# IPC diagnostics
cdmf-cli diag ipc

# Generate diagnostic report
cdmf-cli diag report --output /tmp/cdmf-diag.tar.gz
```

### Performance Issues

```bash
# Profile CPU usage
cdmf-cli profile cpu --duration 60

# Profile memory usage
cdmf-cli profile memory

# Trace slow operations
cdmf-cli trace --threshold 100ms

# Analyze service calls
cdmf-cli analyze services --top 10
```

## Production Checklist

### Pre-Deployment

- [ ] System requirements verified
- [ ] Framework installed and configured
- [ ] All required modules available
- [ ] Configuration validated
- [ ] Security policies defined
- [ ] Backup strategy implemented
- [ ] Monitoring configured
- [ ] Load testing completed
- [ ] Disaster recovery plan documented
- [ ] Operations runbook created

### Deployment

- [ ] Configuration files deployed
- [ ] Modules installed and verified
- [ ] Services registered and available
- [ ] Health checks passing
- [ ] Metrics collection working
- [ ] Logging configured
- [ ] Security measures active
- [ ] Backup scheduled
- [ ] Monitoring alerts configured
- [ ] Documentation updated

### Post-Deployment

- [ ] All services operational
- [ ] Performance metrics acceptable
- [ ] No critical errors in logs
- [ ] Backup verification completed
- [ ] Team trained on procedures
- [ ] Incident response tested
- [ ] Change management process defined
- [ ] Capacity planning reviewed
- [ ] Security audit completed
- [ ] Compliance requirements met

---

*Previous: [Developer Guide](04-developer-guide.md)*
*Next: [Performance and Optimization Guide →](06-performance-optimization.md)*