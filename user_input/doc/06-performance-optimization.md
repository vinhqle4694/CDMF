# CDMF Performance and Optimization Guide

## Table of Contents
1. [Performance Benchmarks](#performance-benchmarks)
2. [Optimization Strategies](#optimization-strategies)
3. [Service Performance](#service-performance)
4. [Module Optimization](#module-optimization)
5. [IPC Performance](#ipc-performance)
6. [Memory Management](#memory-management)
7. [Threading and Concurrency](#threading-and-concurrency)
8. [Profiling and Analysis](#profiling-and-analysis)
9. [Performance Tuning](#performance-tuning)
10. [Best Practices](#best-practices)
11. [Performance Monitoring](#performance-monitoring)
12. [Case Studies](#case-studies)

## Performance Benchmarks

### Baseline Performance Metrics

Understanding CDMF's performance characteristics is crucial for optimization.

#### Service Call Latency

| Operation Type | In-Process | Out-of-Process | Remote |
|---------------|------------|----------------|---------|
| Simple call | 5 ns | 50 μs | 5 ms |
| With small data (1KB) | 10 ns | 60 μs | 6 ms |
| With large data (1MB) | 500 ns | 200 μs | 15 ms |
| Complex operation | 1 μs | 100 μs | 10 ms |

#### Throughput Metrics

| Metric | In-Process | Out-of-Process | Remote |
|--------|------------|----------------|---------|
| Calls/second | 10M | 20K | 200 |
| Data transfer | Unlimited | 1 GB/s | 100 MB/s |
| Concurrent operations | 10,000+ | 1,000 | 100 |

#### Module Operations

| Operation | Time | Notes |
|-----------|------|-------|
| Module load | 1-10 ms | Depends on size and dependencies |
| Module start | 1-100 ms | Depends on initialization |
| Module stop | 1-50 ms | Depends on cleanup |
| Service registration | 50 ns | O(1) operation |
| Service lookup | 50-100 ns | Hash map lookup |
| Service tracker update | 100 ns | Event dispatch |

### Memory Overhead

#### Framework Core
- Base framework: ~10 MB
- Service registry: ~10 KB + (100 bytes × services)
- Event dispatcher: ~4 KB + thread pool
- Module registry: ~5 KB + (512 bytes × modules)

#### Per Module
- Module object: ~512 bytes
- Module context: ~256 bytes
- Service registrations: ~128 bytes each
- Manifest data: 1-5 KB

#### Total Memory Formula
```
Total = 10 MB (framework)
      + (N_modules × 10 KB)
      + (N_services × 256 bytes)
      + Module data
```

## Optimization Strategies

### Architecture-Level Optimizations

#### 1. Choose the Right Deployment Model

```cpp
// For maximum performance: in-process
{
  "deployment": {
    "type": "single-process",
    "enable_ipc": false
  }
}

// For isolation with good performance: hybrid
{
  "deployment": {
    "type": "hybrid",
    "trusted_modules": "in-process",
    "untrusted_modules": "out-of-process"
  }
}
```

#### 2. Minimize Module Boundaries

```cpp
// Instead of many small modules
Module A (100 LOC) → Module B (100 LOC) → Module C (100 LOC)

// Combine related functionality
Module ABC (300 LOC) with internal components
```

#### 3. Use Service Caching

```cpp
class OptimizedConsumer {
private:
    // Cache service references
    std::shared_ptr<ILogger> cachedLogger_;
    std::shared_ptr<IDatabase> cachedDatabase_;

public:
    void initialize(IModuleContext* context) {
        // Look up once, use many times
        cachedLogger_ = context->getService<ILogger>("logger");
        cachedDatabase_ = context->getService<IDatabase>("database");
    }

    void doWork() {
        // Direct use, no lookup overhead
        cachedLogger_->log("Working...");
        cachedDatabase_->query("SELECT...");
    }
};
```

### Code-Level Optimizations

#### 1. Avoid Frequent Service Lookups

```cpp
// BAD: Lookup on every call
void processRequest(IModuleContext* ctx, const Request& req) {
    auto logger = ctx->getService<ILogger>("logger");  // 50ns each time
    logger->log("Processing request");

    auto db = ctx->getService<IDatabase>("database");  // Another 50ns
    db->query(req.query);
}

// GOOD: Cache services
class RequestProcessor {
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IDatabase> db_;

public:
    RequestProcessor(IModuleContext* ctx)
        : logger_(ctx->getService<ILogger>("logger"))
        , db_(ctx->getService<IDatabase>("database")) {}

    void processRequest(const Request& req) {
        logger_->log("Processing request");  // Direct call, ~5ns
        db_->query(req.query);
    }
};
```

#### 2. Use Move Semantics

```cpp
// BAD: Unnecessary copies
class DataProcessor : public IDataProcessor {
    std::vector<Data> process(std::vector<Data> input) override {  // Copy
        std::vector<Data> result = transform(input);  // Copy
        return result;  // Copy (maybe)
    }
};

// GOOD: Move semantics
class DataProcessor : public IDataProcessor {
    std::vector<Data> process(std::vector<Data>&& input) override {  // Move
        auto result = transform(std::move(input));  // Move
        return result;  // RVO/Move
    }
};
```

#### 3. String Optimization

```cpp
// BAD: String concatenation in loop
std::string buildMessage(const std::vector<std::string>& parts) {
    std::string result;
    for (const auto& part : parts) {
        result += part + ", ";  // Multiple allocations
    }
    return result;
}

// GOOD: Reserve and append
std::string buildMessage(const std::vector<std::string>& parts) {
    size_t totalSize = 0;
    for (const auto& part : parts) {
        totalSize += part.size() + 2;
    }

    std::string result;
    result.reserve(totalSize);  // Single allocation

    for (const auto& part : parts) {
        result.append(part);
        result.append(", ");
    }
    return result;
}
```

## Service Performance

### Service Registry Optimization

#### 1. Use Service Ranking

```cpp
// Set higher ranking for preferred implementations
cdmf::Properties props;
props.set("service.ranking", "1000");  // Higher = preferred

context->registerService<ICache>(
    "com.example.ICache",
    fastCache,
    props
);
```

#### 2. Filter Services Efficiently

```cpp
// BAD: Get all services then filter in code
auto refs = context->getServiceReferences<IDatabase>("database");
for (const auto& ref : refs) {
    auto props = ref.getProperties();
    if (props.get("type") == "postgresql") {
        // Use this one
    }
}

// GOOD: Use LDAP filter
auto refs = context->getServiceReferences<IDatabase>(
    "database",
    "(type=postgresql)"  // Filtered at registry level
);
```

#### 3. Service Pooling

```cpp
class ConnectionPool : public IConnectionFactory {
private:
    std::queue<std::shared_ptr<Connection>> pool_;
    std::mutex mutex_;
    size_t maxSize_ = 100;

public:
    std::shared_ptr<Connection> getConnection() override {
        std::lock_guard lock(mutex_);

        if (!pool_.empty()) {
            auto conn = pool_.front();
            pool_.pop();
            return conn;  // Reuse existing
        }

        return createNewConnection();  // Create if needed
    }

    void returnConnection(std::shared_ptr<Connection> conn) {
        std::lock_guard lock(mutex_);

        if (pool_.size() < maxSize_) {
            conn->reset();  // Reset state
            pool_.push(conn);  // Return to pool
        }
    }
};
```

### Service Call Optimization

#### 1. Batch Operations

```cpp
// BAD: Multiple service calls
for (const auto& item : items) {
    database->insert(item);  // N service calls
}

// GOOD: Batch operation
database->batchInsert(items);  // 1 service call
```

#### 2. Async Service Calls

```cpp
class AsyncService : public IAsyncService {
    std::future<Result> processAsync(const Request& req) override {
        return std::async(std::launch::async, [this, req] {
            return process(req);
        });
    }
};

// Usage
auto future = service->processAsync(request);
// Do other work...
auto result = future.get();  // Wait for result
```

## Module Optimization

### Module Loading

#### 1. Lazy Loading

```cpp
class LazyModule : public IModuleActivator {
private:
    std::unique_ptr<ExpensiveResource> resource_;
    std::once_flag initFlag_;

    void ensureInitialized() {
        std::call_once(initFlag_, [this] {
            resource_ = std::make_unique<ExpensiveResource>();
        });
    }

public:
    void start(IModuleContext* context) override {
        // Don't initialize expensive resources here
        // Wait until actually needed
    }

    void doWork() {
        ensureInitialized();  // Initialize on first use
        resource_->process();
    }
};
```

#### 2. Module Preloading

```json
{
  "modules": {
    "preload": [
      "com.example.core",
      "com.example.cache",
      "com.example.database"
    ],
    "lazy_load": [
      "com.example.reporting",
      "com.example.analytics"
    ]
  }
}
```

### Module Communication

#### 1. Shared Memory for Large Data

```cpp
class SharedMemoryTransport : public ITransport {
private:
    struct SharedBuffer {
        std::atomic<bool> ready;
        size_t size;
        char data[1024 * 1024];  // 1MB buffer
    };

    SharedBuffer* buffer_;

public:
    void send(const void* data, size_t size) override {
        memcpy(buffer_->data, data, size);
        buffer_->size = size;
        buffer_->ready = true;
    }

    size_t receive(void* data, size_t maxSize) override {
        while (!buffer_->ready) {
            std::this_thread::yield();
        }

        size_t size = std::min(buffer_->size, maxSize);
        memcpy(data, buffer_->data, size);
        buffer_->ready = false;
        return size;
    }
};
```

#### 2. Zero-Copy Message Passing

```cpp
class ZeroCopyMessage {
private:
    std::shared_ptr<const std::vector<uint8_t>> data_;

public:
    // Move data ownership instead of copying
    explicit ZeroCopyMessage(std::vector<uint8_t>&& data)
        : data_(std::make_shared<const std::vector<uint8_t>>(std::move(data))) {}

    // Multiple readers can share the same data
    const uint8_t* getData() const {
        return data_->data();
    }

    size_t getSize() const {
        return data_->size();
    }
};
```

## IPC Performance

### Transport Selection

#### Performance Comparison

| Transport | Latency | Throughput | CPU Usage | Use Case |
|-----------|---------|------------|-----------|----------|
| Unix Socket | 50 μs | 1 GB/s | Medium | Default, balanced |
| Shared Memory | 10 μs | 10 GB/s | Low | Large data transfer |
| Named Pipes | 100 μs | 500 MB/s | Medium | Windows compatibility |
| gRPC | 5 ms | 100 MB/s | High | Remote communication |

#### Configuration

```json
{
  "ipc": {
    "transports": {
      "default": "unix-socket",
      "large_data": "shared-memory",
      "remote": "grpc"
    },
    "shared_memory": {
      "size_mb": 100,
      "num_buffers": 10
    },
    "unix_socket": {
      "buffer_size": 65536,
      "backlog": 100
    }
  }
}
```

### Message Serialization

#### 1. Choose Efficient Formats

```cpp
// JSON: Human readable but slow
{
  "type": "request",
  "id": 12345,
  "data": "..."
}

// Protocol Buffers: Fast and compact
message Request {
  int32 id = 1;
  bytes data = 2;
}

// FlatBuffers: Zero-copy deserialization
namespace Message;

table Request {
  id: int32;
  data: [ubyte];
}
```

#### 2. Batch Small Messages

```cpp
class MessageBatcher {
private:
    std::vector<Message> batch_;
    std::chrono::milliseconds flushInterval_{10};
    size_t maxBatchSize_ = 100;

public:
    void send(const Message& msg) {
        batch_.push_back(msg);

        if (batch_.size() >= maxBatchSize_) {
            flush();
        }
    }

private:
    void flush() {
        if (batch_.empty()) return;

        // Send all messages in one IPC call
        transport_->sendBatch(batch_);
        batch_.clear();
    }
};
```

## Memory Management

### Memory Pool Allocation

```cpp
template<typename T>
class MemoryPool {
private:
    struct Block {
        alignas(T) char data[sizeof(T)];
        Block* next;
    };

    Block* freeList_;
    std::vector<std::unique_ptr<Block[]>> chunks_;
    size_t chunkSize_ = 1024;

public:
    T* allocate() {
        if (!freeList_) {
            expandPool();
        }

        Block* block = freeList_;
        freeList_ = freeList_->next;
        return reinterpret_cast<T*>(block);
    }

    void deallocate(T* ptr) {
        Block* block = reinterpret_cast<Block*>(ptr);
        block->next = freeList_;
        freeList_ = block;
    }

private:
    void expandPool() {
        auto chunk = std::make_unique<Block[]>(chunkSize_);

        for (size_t i = 0; i < chunkSize_ - 1; ++i) {
            chunk[i].next = &chunk[i + 1];
        }
        chunk[chunkSize_ - 1].next = freeList_;

        freeList_ = &chunk[0];
        chunks_.push_back(std::move(chunk));
    }
};

// Usage
MemoryPool<Request> requestPool;
Request* req = requestPool.allocate();
// Use request...
requestPool.deallocate(req);
```

### Reduce Memory Fragmentation

```cpp
class FragmentationReducer {
    // Allocate objects of similar sizes together
    MemoryPool<SmallObject> smallPool_;   // < 64 bytes
    MemoryPool<MediumObject> mediumPool_; // < 1KB
    MemoryPool<LargeObject> largePool_;   // < 64KB

    template<typename T>
    T* allocate() {
        if constexpr (sizeof(T) <= 64) {
            return smallPool_.allocate<T>();
        } else if constexpr (sizeof(T) <= 1024) {
            return mediumPool_.allocate<T>();
        } else {
            return largePool_.allocate<T>();
        }
    }
};
```

### Smart Pointer Optimization

```cpp
// Avoid shared_ptr when unique_ptr suffices
class Service {
    // BAD: Unnecessary reference counting
    std::shared_ptr<Resource> resource_;

    // GOOD: Single ownership
    std::unique_ptr<Resource> resource_;
};

// Use weak_ptr to break cycles
class Node {
    std::shared_ptr<Node> child_;
    std::weak_ptr<Node> parent_;  // Breaks cycle
};
```

## Threading and Concurrency

### Thread Pool Configuration

```cpp
class OptimizedThreadPool {
private:
    size_t optimalThreadCount() {
        // CPU-bound: num_cores
        // I/O-bound: num_cores * 2
        unsigned int cores = std::thread::hardware_concurrency();
        return cores * (isIOBound_ ? 2 : 1);
    }

public:
    OptimizedThreadPool(bool isIOBound = false)
        : isIOBound_(isIOBound)
        , numThreads_(optimalThreadCount()) {

        for (size_t i = 0; i < numThreads_; ++i) {
            workers_.emplace_back([this] { workerThread(); });
        }
    }
};
```

### Lock-Free Data Structures

```cpp
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    void enqueue(T item) {
        Node* newNode = new Node;
        newNode->data = new T(std::move(item));
        newNode->next = nullptr;

        Node* prevTail = tail_.exchange(newNode);
        prevTail->next = newNode;
    }

    std::optional<T> dequeue() {
        Node* head = head_.load();
        Node* next = head->next.load();

        if (next == nullptr) {
            return std::nullopt;
        }

        T* data = next->data.exchange(nullptr);
        head_.store(next);
        delete head;

        return *data;
    }
};
```

### Minimize Lock Contention

```cpp
class ShardedCache {
private:
    static constexpr size_t NUM_SHARDS = 16;

    struct Shard {
        std::shared_mutex mutex;
        std::unordered_map<std::string, std::string> data;
    };

    std::array<Shard, NUM_SHARDS> shards_;

    size_t getShardIndex(const std::string& key) {
        return std::hash<std::string>{}(key) % NUM_SHARDS;
    }

public:
    void put(const std::string& key, const std::string& value) {
        auto& shard = shards_[getShardIndex(key)];
        std::unique_lock lock(shard.mutex);
        shard.data[key] = value;
    }

    std::string get(const std::string& key) {
        auto& shard = shards_[getShardIndex(key)];
        std::shared_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        return it != shard.data.end() ? it->second : "";
    }
};
```

## Profiling and Analysis

### CPU Profiling

#### Using perf (Linux)

```bash
# Record CPU profile
perf record -g ./cdmf-app

# Analyze results
perf report

# Generate flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

#### Using Instruments (macOS)

```bash
# Time Profiler
instruments -t "Time Profiler" -D trace.trace ./cdmf-app

# Analyze in Instruments GUI
open trace.trace
```

### Memory Profiling

#### Valgrind Massif

```bash
# Profile heap usage
valgrind --tool=massif ./cdmf-app

# Visualize results
ms_print massif.out.12345
```

#### AddressSanitizer

```cpp
// Compile with ASan
g++ -fsanitize=address -g -O2 module.cpp

// Run with options
ASAN_OPTIONS=print_stats=1:check_leaks=1 ./app
```

### Custom Profiling

```cpp
class Profiler {
private:
    struct ProfileData {
        size_t count = 0;
        std::chrono::nanoseconds totalTime{0};
        std::chrono::nanoseconds minTime{std::chrono::nanoseconds::max()};
        std::chrono::nanoseconds maxTime{0};
    };

    std::unordered_map<std::string, ProfileData> data_;
    std::mutex mutex_;

public:
    class Timer {
        Profiler* profiler_;
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;

    public:
        Timer(Profiler* p, const std::string& name)
            : profiler_(p), name_(name)
            , start_(std::chrono::high_resolution_clock::now()) {}

        ~Timer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = end - start_;
            profiler_->record(name_, duration);
        }
    };

    void record(const std::string& name,
                std::chrono::nanoseconds duration) {
        std::lock_guard lock(mutex_);
        auto& prof = data_[name];
        prof.count++;
        prof.totalTime += duration;
        prof.minTime = std::min(prof.minTime, duration);
        prof.maxTime = std::max(prof.maxTime, duration);
    }

    void report() {
        std::lock_guard lock(mutex_);
        for (const auto& [name, prof] : data_) {
            auto avg = prof.totalTime / prof.count;
            std::cout << name << ":\n"
                     << "  Count: " << prof.count << "\n"
                     << "  Avg: " << avg.count() << "ns\n"
                     << "  Min: " << prof.minTime.count() << "ns\n"
                     << "  Max: " << prof.maxTime.count() << "ns\n";
        }
    }
};

// Usage
Profiler profiler;

void someFunction() {
    Profiler::Timer timer(&profiler, "someFunction");
    // Function body...
}
```

## Performance Tuning

### Framework Configuration

```json
{
  "performance": {
    "service_cache_size": 1000,
    "service_cache_ttl_ms": 60000,
    "event_queue_size": 10000,
    "event_thread_pool_size": 8,
    "lazy_loading": true,
    "preload_modules": [
      "com.example.core",
      "com.example.cache"
    ]
  }
}
```

### Compiler Optimizations

```cmake
# CMake optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")  # Link-time optimization
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-exceptions")  # If not using exceptions

# Profile-guided optimization
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-generate")
# Run typical workload...
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-use")
```

### System Tuning

#### Linux Kernel Parameters

```bash
# /etc/sysctl.conf

# Increase network buffers
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728

# TCP tuning
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728

# File descriptors
fs.file-max = 2097152

# Shared memory
kernel.shmmax = 68719476736
kernel.shmall = 4294967296
```

#### Process Limits

```bash
# /etc/security/limits.conf
cdmf soft nofile 65536
cdmf hard nofile 65536
cdmf soft nproc 32768
cdmf hard nproc 32768
```

## Best Practices

### 1. Measure Before Optimizing

```cpp
// Always profile first
void optimizeFunction() {
    auto start = std::chrono::high_resolution_clock::now();

    // Original implementation
    doWork();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;

    if (duration > threshold) {
        // Only optimize if actually slow
    }
}
```

### 2. Cache Aggressively

```cpp
class CachingService : public IService {
private:
    mutable std::unordered_map<Key, Value> cache_;
    mutable std::shared_mutex mutex_;

public:
    Value compute(const Key& key) const override {
        {
            std::shared_lock lock(mutex_);
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                return it->second;  // Cache hit
            }
        }

        // Cache miss
        Value value = expensiveComputation(key);

        {
            std::unique_lock lock(mutex_);
            cache_[key] = value;
        }

        return value;
    }
};
```

### 3. Batch Operations

```cpp
class BatchProcessor {
    void processBatch(const std::vector<Item>& items) {
        // Process all at once
        database_->beginTransaction();

        for (const auto& item : items) {
            database_->insert(item);
        }

        database_->commit();
    }
};
```

### 4. Use Appropriate Data Structures

```cpp
// Choose based on usage pattern

// Frequent lookups: unordered_map
std::unordered_map<Key, Value> lookup;

// Ordered iteration: map
std::map<Key, Value> ordered;

// Small size (<100): vector + linear search
std::vector<std::pair<Key, Value>> small;

// Cache-friendly iteration: vector
std::vector<Value> sequential;

// Concurrent access: concurrent_unordered_map
tbb::concurrent_unordered_map<Key, Value> concurrent;
```

## Performance Monitoring

### Real-Time Metrics

```cpp
class PerformanceMonitor {
private:
    struct Metrics {
        std::atomic<uint64_t> callCount{0};
        std::atomic<uint64_t> totalLatencyNs{0};
        std::atomic<uint64_t> errorCount{0};
    };

    std::unordered_map<std::string, Metrics> metrics_;

public:
    void recordCall(const std::string& service,
                   std::chrono::nanoseconds latency,
                   bool success) {
        auto& m = metrics_[service];
        m.callCount++;
        m.totalLatencyNs += latency.count();
        if (!success) m.errorCount++;
    }

    void exportMetrics() {
        for (const auto& [service, m] : metrics_) {
            uint64_t calls = m.callCount.load();
            uint64_t totalNs = m.totalLatencyNs.load();
            uint64_t errors = m.errorCount.load();

            double avgLatencyMs = (totalNs / calls) / 1000000.0;
            double errorRate = (errors * 100.0) / calls;

            // Export to monitoring system
            prometheus::Gauge()
                .Name("service_latency_ms")
                .Label("service", service)
                .Set(avgLatencyMs);

            prometheus::Gauge()
                .Name("service_error_rate")
                .Label("service", service)
                .Set(errorRate);
        }
    }
};
```

### Performance Alerts

```yaml
# Prometheus alert rules
groups:
  - name: cdmf_performance
    rules:
      - alert: HighServiceLatency
        expr: service_latency_ms > 100
        for: 5m
        annotations:
          summary: "High service latency detected"

      - alert: LowThroughput
        expr: rate(service_calls_total[5m]) < 100
        for: 10m
        annotations:
          summary: "Low service throughput"

      - alert: HighMemoryUsage
        expr: process_resident_memory_bytes > 2e9
        for: 5m
        annotations:
          summary: "High memory usage (>2GB)"
```

## Case Studies

### Case Study 1: Financial Trading System

**Challenge**: Reduce order processing latency from 500μs to <50μs

**Solution**:
1. Moved all critical modules to in-process
2. Implemented lock-free queues for order flow
3. Used memory pools for order objects
4. CPU affinity for critical threads

**Result**: 45μs average latency (91% improvement)

```cpp
class OptimizedTradingEngine {
private:
    // Memory pool for orders
    MemoryPool<Order> orderPool_;

    // Lock-free queue for incoming orders
    LockFreeQueue<Order*> orderQueue_;

    // Pre-allocated response buffers
    std::array<Response, 1000> responseBuffers_;

    // CPU affinity
    void pinToCore(int core) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    }

public:
    void processOrder(Order order) {
        // Allocate from pool (no heap allocation)
        Order* pooledOrder = orderPool_.allocate();
        *pooledOrder = std::move(order);

        // Enqueue (lock-free)
        orderQueue_.enqueue(pooledOrder);
    }
};
```

### Case Study 2: IoT Data Processing

**Challenge**: Handle 1M messages/second from IoT devices

**Solution**:
1. Implemented message batching (100 messages per batch)
2. Used shared memory for inter-module communication
3. Parallel processing with sharded data structures
4. Protocol Buffers for serialization

**Result**: 1.2M messages/second sustained throughput

```cpp
class IoTDataProcessor {
private:
    // Sharded processing
    static constexpr size_t NUM_SHARDS = 16;
    std::array<MessageQueue, NUM_SHARDS> shards_;

    // Batch processing
    void processBatch(const MessageBatch& batch) {
        // Process all messages in parallel
        std::for_each(std::execution::par,
                     batch.begin(), batch.end(),
                     [this](const Message& msg) {
            processMessage(msg);
        });
    }
};
```

### Case Study 3: Video Streaming Service

**Challenge**: Reduce startup time for video modules from 5s to <1s

**Solution**:
1. Lazy initialization of codecs
2. Preloading of critical modules
3. Parallel module startup
4. Caching of configuration data

**Result**: 800ms startup time (84% improvement)

```cpp
class VideoStreamingModule {
private:
    // Lazy codec initialization
    mutable std::once_flag codecInitFlag_;
    mutable std::unique_ptr<Codec> codec_;

    void initCodec() const {
        std::call_once(codecInitFlag_, [this] {
            codec_ = std::make_unique<Codec>();
        });
    }

public:
    void start(IModuleContext* context) override {
        // Fast startup - don't init codec yet
        registerService(context);
    }

    void processFrame(const Frame& frame) {
        initCodec();  // Initialize on first use
        codec_->process(frame);
    }
};
```

---

*Previous: [Deployment and Operations Guide](05-deployment-operations.md)*
*Next: [Migration and Integration Guide →](07-migration-integration.md)*