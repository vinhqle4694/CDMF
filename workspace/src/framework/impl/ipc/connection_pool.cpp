/**
 * @file connection_pool.cpp
 * @brief Implementation of connection pooling infrastructure
 */

#include "ipc/connection_pool.h"
#include "utils/log.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace cdmf {
namespace ipc {

/**
 * @brief Connection metadata
 */
struct ConnectionMetadata {
    TransportPtr transport;
    std::chrono::steady_clock::time_point created_time;
    std::chrono::steady_clock::time_point last_used_time;
    uint64_t use_count = 0;
    bool in_use = false;
};

/**
 * @brief Per-endpoint pool state
 */
struct EndpointPoolState {
    std::string endpoint;
    std::deque<std::unique_ptr<ConnectionMetadata>> connections;
    ConnectionPoolStats stats;
    uint32_t next_index = 0;  // For round-robin
    mutable std::mutex mutex;
    std::condition_variable cv;
};

/**
 * @brief PooledConnection implementation
 */
class PooledConnection::Impl {
public:
    Impl(TransportPtr transport, std::function<void(TransportPtr)> releaser)
        : transport_(transport)
        , releaser_(std::move(releaser))
        , released_(false) {
        LOGD_FMT("PooledConnection::Impl constructed");
    }

    ~Impl() {
        LOGD_FMT("PooledConnection::Impl destructor called");
        release();
    }

    Impl(Impl&& other) noexcept
        : transport_(std::move(other.transport_))
        , releaser_(std::move(other.releaser_))
        , released_(other.released_) {
        LOGD_FMT("PooledConnection::Impl moved");
        other.released_ = true;
    }

    Impl& operator=(Impl&& other) noexcept {
        if (this != &other) {
            LOGD_FMT("PooledConnection::Impl move assigned");
            release();
            transport_ = std::move(other.transport_);
            releaser_ = std::move(other.releaser_);
            released_ = other.released_;
            other.released_ = true;
        }
        return *this;
    }

    TransportPtr get() const {
        return transport_;
    }

    void release() {
        if (!released_ && transport_ && releaser_) {
            LOGD_FMT("PooledConnection::Impl releasing connection");
            releaser_(transport_);
            released_ = true;
        }
    }

    TransportPtr transport_;
    std::function<void(TransportPtr)> releaser_;
    bool released_;
};

// PooledConnection implementation

PooledConnection::PooledConnection(TransportPtr transport,
                                   std::function<void(TransportPtr)> releaser)
    : pImpl_(std::make_unique<Impl>(transport, std::move(releaser))) {
}

PooledConnection::~PooledConnection() = default;

PooledConnection::PooledConnection(PooledConnection&&) noexcept = default;
PooledConnection& PooledConnection::operator=(PooledConnection&&) noexcept = default;

TransportPtr PooledConnection::get() const {
    return pImpl_ ? pImpl_->get() : nullptr;
}

TransportPtr PooledConnection::operator->() const {
    return get();
}

PooledConnection::operator bool() const {
    return pImpl_ && pImpl_->transport_ && !pImpl_->released_;
}

void PooledConnection::release() {
    if (pImpl_) {
        pImpl_->release();
    }
}

/**
 * @brief ConnectionPool implementation
 */
class ConnectionPool::Impl {
public:
    Impl(const ConnectionPoolConfig& config, ConnectionFactory factory)
        : config_(config)
        , factory_(std::move(factory))
        , running_(false)
        , should_stop_(false) {
        LOGD_FMT("ConnectionPool::Impl constructed with max_pool_size=" << config.max_pool_size);
    }

    ~Impl() {
        LOGD_FMT("ConnectionPool::Impl destructor called");
        stop();
    }

    bool start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            LOGW_FMT("ConnectionPool already running");
            return false;
        }

        LOGI_FMT("Starting ConnectionPool");
        should_stop_ = false;
        running_ = true;

        // Start eviction thread
        eviction_thread_ = std::thread(&Impl::evictionLoop, this);

        LOGI_FMT("ConnectionPool started successfully");
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                LOGD_FMT("ConnectionPool not running, nothing to stop");
                return;
            }

            LOGI_FMT("Stopping ConnectionPool");
            should_stop_ = true;
            running_ = false;
        }

        // Wake up all waiting threads
        for (auto& [endpoint, state] : pools_) {
            state->cv.notify_all();
        }

        if (eviction_thread_.joinable()) {
            LOGD_FMT("Joining eviction thread");
            eviction_thread_.join();
        }

        // Close all connections
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [endpoint, state] : pools_) {
            std::lock_guard<std::mutex> state_lock(state->mutex);
            LOGD_FMT("Closing " << state->connections.size() << " connections for endpoint: " << endpoint);
            for (auto& conn : state->connections) {
                if (conn->transport) {
                    conn->transport->disconnect();
                    conn->transport->cleanup();
                }
            }
            state->connections.clear();
        }
        LOGI_FMT("ConnectionPool stopped successfully");
    }

    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    PooledConnection acquire(const std::string& endpoint,
                            std::chrono::milliseconds timeout) {
        auto acquire_start = std::chrono::steady_clock::now();

        LOGD_FMT("Acquiring connection for endpoint: " << endpoint << ", timeout: " << timeout.count() << "ms");

        // Get or create pool state
        EndpointPoolState* state = getOrCreatePoolState(endpoint);
        if (!state) {
            LOGE_FMT("Failed to get or create pool state for endpoint: " << endpoint);
            return PooledConnection(nullptr, {});
        }

        TransportPtr transport = nullptr;

        {
            std::unique_lock<std::mutex> lock(state->mutex);

            // Wait for available connection
            auto deadline = acquire_start + timeout;
            while (running_ && !transport) {
                // Try to get available connection
                transport = getAvailableConnection(*state);

                if (!transport) {
                    // Try to create new connection if under max
                    if (getTotalConnections(*state) < config_.max_pool_size) {
                        transport = createNewConnection(endpoint, *state);
                    }
                }

                if (!transport) {
                    if (!config_.wait_if_exhausted) {
                        state->stats.acquire_timeouts++;
                        break;
                    }

                    // Wait for connection to become available
                    auto now = std::chrono::steady_clock::now();
                    if (now >= deadline) {
                        state->stats.acquire_timeouts++;
                        break;
                    }

                    // Wait with predicate to exit early if shutting down
                    state->cv.wait_until(lock, deadline, [this] { return should_stop_.load(); });

                    // Check if we need to exit due to shutdown
                    if (should_stop_) {
                        break;
                    }
                }
            }

            if (transport) {
                state->stats.total_acquisitions++;
                state->stats.active_connections++;

                // Update average acquire time
                auto acquire_end = std::chrono::steady_clock::now();
                auto acquire_time = std::chrono::duration_cast<std::chrono::microseconds>(
                    acquire_end - acquire_start);

                state->stats.avg_acquire_time = std::chrono::microseconds(
                    (state->stats.avg_acquire_time.count() *
                     (state->stats.total_acquisitions - 1) +
                     acquire_time.count()) / state->stats.total_acquisitions);

                LOGD_FMT("Connection acquired successfully for endpoint: " << endpoint
                         << ", acquire time: " << acquire_time.count() << "us");
            }
        }

        if (!transport) {
            LOGE_FMT("Failed to acquire connection for endpoint: " << endpoint);
            return PooledConnection(nullptr, {});
        }

        // Create releaser callback
        auto releaser = [this, endpoint](TransportPtr t) {
            this->releaseConnection(endpoint, t);
        };

        return PooledConnection(transport, releaser);
    }

    PooledConnection tryAcquire(const std::string& endpoint) {
        LOGD_FMT("Trying to acquire connection (non-blocking) for endpoint: " << endpoint);
        return acquire(endpoint, std::chrono::milliseconds(0));
    }

    ConnectionPoolStats getStats(const std::string& endpoint) const {
        LOGD_FMT("Getting stats for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pools_.find(endpoint);
        if (it == pools_.end()) {
            LOGW_FMT("Endpoint not found for stats retrieval: " << endpoint);
            return ConnectionPoolStats{};
        }

        std::lock_guard<std::mutex> state_lock(it->second->mutex);
        return it->second->stats;
    }

    ConnectionPoolStats getAggregateStats() const {
        LOGD_FMT("Getting aggregate stats for all endpoints");
        std::lock_guard<std::mutex> lock(mutex_);

        ConnectionPoolStats aggregate;

        for (const auto& [endpoint, state] : pools_) {
            std::lock_guard<std::mutex> state_lock(state->mutex);
            aggregate.total_connections += state->stats.total_connections;
            aggregate.active_connections += state->stats.active_connections;
            aggregate.idle_connections += state->stats.idle_connections;
            aggregate.total_acquisitions += state->stats.total_acquisitions;
            aggregate.total_releases += state->stats.total_releases;
            aggregate.acquire_timeouts += state->stats.acquire_timeouts;
            aggregate.connections_created += state->stats.connections_created;
            aggregate.connections_destroyed += state->stats.connections_destroyed;
            aggregate.evictions_idle += state->stats.evictions_idle;
            aggregate.evictions_lifetime += state->stats.evictions_lifetime;
            aggregate.validation_failures += state->stats.validation_failures;
            aggregate.peak_connections = std::max(aggregate.peak_connections,
                                                 state->stats.peak_connections);
        }

        LOGD_FMT("Aggregate stats - total: " << aggregate.total_connections
                 << ", active: " << aggregate.active_connections << ", idle: " << aggregate.idle_connections);
        return aggregate;
    }

    void resetStats(const std::string& endpoint) {
        LOGI_FMT("Resetting stats for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pools_.find(endpoint);
        if (it != pools_.end()) {
            std::lock_guard<std::mutex> state_lock(it->second->mutex);
            it->second->stats = ConnectionPoolStats{};
            LOGI_FMT("Stats reset completed for endpoint: " << endpoint);
        } else {
            LOGW_FMT("Endpoint not found for stats reset: " << endpoint);
        }
    }

    void closeAll(const std::string& endpoint) {
        LOGI_FMT("Closing all connections for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pools_.find(endpoint);
        if (it == pools_.end()) {
            LOGW_FMT("Endpoint not found for closeAll: " << endpoint);
            return;
        }

        std::lock_guard<std::mutex> state_lock(it->second->mutex);

        uint32_t closed_count = 0;
        for (auto& conn : it->second->connections) {
            if (conn->transport) {
                conn->transport->disconnect();
                conn->transport->cleanup();
                it->second->stats.connections_destroyed++;
                closed_count++;
            }
        }

        it->second->connections.clear();
        it->second->stats.total_connections = 0;
        it->second->stats.idle_connections = 0;
        it->second->stats.active_connections = 0;
        LOGI_FMT("Closed " << closed_count << " connections for endpoint: " << endpoint);
    }

    uint32_t closeIdle(const std::string& endpoint) {
        LOGD_FMT("Closing idle connections for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pools_.find(endpoint);
        if (it == pools_.end()) {
            LOGW_FMT("Endpoint not found for closeIdle: " << endpoint);
            return 0;
        }

        uint32_t evicted = evictIdleConnections(*it->second);
        LOGD_FMT("Closed " << evicted << " idle connections for endpoint: " << endpoint);
        return evicted;
    }

    uint32_t prepopulate(const std::string& endpoint, uint32_t count) {
        LOGI_FMT("Prepopulating " << count << " connections for endpoint: " << endpoint);
        EndpointPoolState* state = getOrCreatePoolState(endpoint);
        if (!state) {
            LOGE_FMT("Failed to get or create pool state for prepopulation: " << endpoint);
            return 0;
        }

        std::lock_guard<std::mutex> lock(state->mutex);

        uint32_t created = 0;
        for (uint32_t i = 0; i < count; ++i) {
            if (getTotalConnections(*state) >= config_.max_pool_size) {
                LOGD_FMT("Max pool size reached during prepopulation at " << created << " connections");
                break;
            }

            auto transport = createNewConnection(endpoint, *state);
            if (transport) {
                // Mark the connection as idle (not in_use) for prepopulated connections
                auto& last_conn = state->connections.back();
                last_conn->in_use = false;
                state->stats.idle_connections++;
                created++;
            }
        }

        LOGI_FMT("Prepopulated " << created << " connections for endpoint: " << endpoint);
        return created;
    }

    void setValidator(ConnectionValidator validator) {
        LOGD_FMT("Setting connection validator");
        std::lock_guard<std::mutex> lock(mutex_);
        validator_ = std::move(validator);
    }

    void updateConfig(const ConnectionPoolConfig& config) {
        LOGI_FMT("Updating connection pool config - max_pool_size: " << config.max_pool_size);
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }

    ConnectionPoolConfig getConfig() const {
        LOGD_FMT("Getting connection pool config");
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    void releaseConnection(const std::string& endpoint, TransportPtr transport) {
        LOGD_FMT("Releasing connection for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pools_.find(endpoint);
        if (it == pools_.end() || !transport) {
            LOGW_FMT("Cannot release connection - endpoint not found or transport null: " << endpoint);
            return;
        }

        EndpointPoolState& state = *it->second;
        std::lock_guard<std::mutex> state_lock(state.mutex);

        // Validate connection before returning to pool
        bool valid = true;
        if (config_.validate_on_release && validator_) {
            valid = validator_(transport);
            if (!valid) {
                LOGW_FMT("Connection validation failed on release for endpoint: " << endpoint);
                state.stats.validation_failures++;
            }
        }

        // Find connection in pool
        auto conn_it = std::find_if(state.connections.begin(),
                                    state.connections.end(),
                                    [&](const auto& c) {
                                        return c->transport == transport;
                                    });

        if (conn_it != state.connections.end()) {
            if (valid && transport->isConnected()) {
                (*conn_it)->in_use = false;
                (*conn_it)->last_used_time = std::chrono::steady_clock::now();
                state.stats.active_connections--;
                state.stats.idle_connections++;
                LOGD_FMT("Connection returned to pool for endpoint: " << endpoint);
            } else {
                // Close invalid connection
                LOGW_FMT("Closing invalid connection for endpoint: " << endpoint);
                transport->disconnect();
                transport->cleanup();
                state.connections.erase(conn_it);
                state.stats.total_connections--;
                state.stats.active_connections--;
                state.stats.connections_destroyed++;
            }

            state.stats.total_releases++;
        } else {
            LOGW_FMT("Connection not found in pool during release for endpoint: " << endpoint);
        }

        // Notify waiting threads
        state.cv.notify_one();
    }

private:
    EndpointPoolState* getOrCreatePoolState(const std::string& endpoint) {
        LOGD_FMT("Getting or creating pool state for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pools_.find(endpoint);
        if (it == pools_.end()) {
            LOGD_FMT("Creating new pool state for endpoint: " << endpoint);
            auto state = std::make_unique<EndpointPoolState>();
            state->endpoint = endpoint;
            auto* ptr = state.get();
            pools_[endpoint] = std::move(state);
            return ptr;
        }

        return it->second.get();
    }

    TransportPtr getAvailableConnection(EndpointPoolState& state) {
        LOGD_FMT("Getting available connection with strategy: " << static_cast<int>(config_.load_balancing));
        // Select based on load balancing strategy
        switch (config_.load_balancing) {
            case LoadBalancingStrategy::ROUND_ROBIN:
                return getConnectionRoundRobin(state);

            case LoadBalancingStrategy::LEAST_LOADED:
                return getConnectionLeastLoaded(state);

            case LoadBalancingStrategy::RANDOM:
                return getConnectionRandom(state);

            case LoadBalancingStrategy::LEAST_RECENTLY_USED:
                return getConnectionLRU(state);
        }

        return nullptr;
    }

    TransportPtr getConnectionRoundRobin(EndpointPoolState& state) {
        for (size_t i = 0; i < state.connections.size(); ++i) {
            uint32_t idx = (state.next_index + i) % state.connections.size();
            auto& conn = state.connections[idx];

            if (!conn->in_use && validateConnection(conn->transport, state)) {
                conn->in_use = true;
                conn->use_count++;
                conn->last_used_time = std::chrono::steady_clock::now();
                state.next_index = (idx + 1) % state.connections.size();
                state.stats.idle_connections--;
                return conn->transport;
            }
        }

        return nullptr;
    }

    TransportPtr getConnectionLeastLoaded(EndpointPoolState& state) {
        ConnectionMetadata* best = nullptr;

        for (auto& conn : state.connections) {
            if (!conn->in_use && validateConnection(conn->transport, state)) {
                if (!best || conn->use_count < best->use_count) {
                    best = conn.get();
                }
            }
        }

        if (best) {
            best->in_use = true;
            best->use_count++;
            best->last_used_time = std::chrono::steady_clock::now();
            state.stats.idle_connections--;
            return best->transport;
        }

        return nullptr;
    }

    TransportPtr getConnectionRandom(EndpointPoolState& state) {
        std::vector<ConnectionMetadata*> available;

        for (auto& conn : state.connections) {
            if (!conn->in_use && validateConnection(conn->transport, state)) {
                available.push_back(conn.get());
            }
        }

        if (available.empty()) {
            return nullptr;
        }

        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, available.size() - 1);

        auto* conn = available[dist(gen)];
        conn->in_use = true;
        conn->use_count++;
        conn->last_used_time = std::chrono::steady_clock::now();
        state.stats.idle_connections--;
        return conn->transport;
    }

    TransportPtr getConnectionLRU(EndpointPoolState& state) {
        ConnectionMetadata* lru = nullptr;

        for (auto& conn : state.connections) {
            if (!conn->in_use && validateConnection(conn->transport, state)) {
                if (!lru || conn->last_used_time < lru->last_used_time) {
                    lru = conn.get();
                }
            }
        }

        if (lru) {
            lru->in_use = true;
            lru->use_count++;
            lru->last_used_time = std::chrono::steady_clock::now();
            state.stats.idle_connections--;
            return lru->transport;
        }

        return nullptr;
    }

    bool validateConnection(TransportPtr transport, EndpointPoolState& state) {
        if (!transport || !transport->isConnected()) {
            LOGD_FMT("Connection validation failed - transport null or not connected");
            return false;
        }

        if (config_.validate_on_acquire && validator_) {
            bool valid = validator_(transport);
            if (!valid) {
                LOGW_FMT("Connection validation failed on acquire");
                state.stats.validation_failures++;
                return false;
            }
        }

        return true;
    }

    TransportPtr createNewConnection(const std::string& endpoint,
                                    EndpointPoolState& state) {
        LOGD_FMT("Creating new connection for endpoint: " << endpoint);
        if (!factory_) {
            LOGE_FMT("No connection factory configured");
            return nullptr;
        }

        auto transport = factory_(endpoint);
        if (!transport) {
            LOGE_FMT("Factory failed to create transport for endpoint: " << endpoint);
            return nullptr;
        }

        auto metadata = std::make_unique<ConnectionMetadata>();
        metadata->transport = transport;
        metadata->created_time = std::chrono::steady_clock::now();
        metadata->last_used_time = metadata->created_time;
        metadata->use_count = 0;
        metadata->in_use = true;

        state.connections.push_back(std::move(metadata));
        state.stats.total_connections++;
        state.stats.connections_created++;

        if (state.stats.total_connections > state.stats.peak_connections) {
            state.stats.peak_connections = state.stats.total_connections;
        }

        LOGD_FMT("New connection created for endpoint: " << endpoint
                 << ", total connections: " << state.stats.total_connections);
        return transport;
    }

    uint32_t getTotalConnections(const EndpointPoolState& state) const {
        return static_cast<uint32_t>(state.connections.size());
    }

    void evictionLoop() {
        LOGD_FMT("Eviction loop started");
        while (!should_stop_) {
            // Evict idle and expired connections
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [endpoint, state] : pools_) {
                    evictIdleConnections(*state);
                    evictExpiredConnections(*state);
                }
            }

            // Wait for next eviction interval with frequent checks to allow quick exit
            auto sleep_end = std::chrono::steady_clock::now() + config_.eviction_interval;
            while (!should_stop_ && std::chrono::steady_clock::now() < sleep_end) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        LOGD_FMT("Eviction loop terminated");
    }

    uint32_t evictIdleConnections(EndpointPoolState& state) {
        std::lock_guard<std::mutex> lock(state.mutex);

        auto now = std::chrono::steady_clock::now();
        uint32_t evicted = 0;

        auto it = state.connections.begin();
        while (it != state.connections.end()) {
            auto& conn = *it;

            if (!conn->in_use) {
                auto idle_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - conn->last_used_time);

                if (idle_time >= config_.max_idle_time &&
                    getTotalConnections(state) > config_.min_pool_size) {

                    conn->transport->disconnect();
                    conn->transport->cleanup();

                    it = state.connections.erase(it);
                    state.stats.total_connections--;
                    state.stats.idle_connections--;
                    state.stats.connections_destroyed++;
                    state.stats.evictions_idle++;
                    evicted++;
                    continue;
                }
            }

            ++it;
        }

        return evicted;
    }

    uint32_t evictExpiredConnections(EndpointPoolState& state) {
        std::lock_guard<std::mutex> lock(state.mutex);

        auto now = std::chrono::steady_clock::now();
        uint32_t evicted = 0;

        auto it = state.connections.begin();
        while (it != state.connections.end()) {
            auto& conn = *it;

            auto lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - conn->created_time);

            if (lifetime >= config_.max_connection_lifetime && !conn->in_use) {
                conn->transport->disconnect();
                conn->transport->cleanup();

                it = state.connections.erase(it);
                state.stats.total_connections--;
                state.stats.idle_connections--;
                state.stats.connections_destroyed++;
                state.stats.evictions_lifetime++;
                evicted++;
                continue;
            }

            ++it;
        }

        return evicted;
    }

    ConnectionPoolConfig config_;
    ConnectionFactory factory_;
    ConnectionValidator validator_;

    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<EndpointPoolState>> pools_;

    std::thread eviction_thread_;
};

// ConnectionPool implementation

ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config,
                               ConnectionFactory factory)
    : pImpl_(std::make_unique<Impl>(config, std::move(factory))) {
}

ConnectionPool::~ConnectionPool() = default;

ConnectionPool::ConnectionPool(ConnectionPool&&) noexcept = default;
ConnectionPool& ConnectionPool::operator=(ConnectionPool&&) noexcept = default;

bool ConnectionPool::start() {
    return pImpl_->start();
}

void ConnectionPool::stop() {
    pImpl_->stop();
}

bool ConnectionPool::isRunning() const {
    return pImpl_->isRunning();
}

PooledConnection ConnectionPool::acquire(const std::string& endpoint) {
    return pImpl_->acquire(endpoint, pImpl_->getConfig().acquire_timeout);
}

PooledConnection ConnectionPool::acquire(const std::string& endpoint,
                                        std::chrono::milliseconds timeout) {
    return pImpl_->acquire(endpoint, timeout);
}

PooledConnection ConnectionPool::tryAcquire(const std::string& endpoint) {
    return pImpl_->tryAcquire(endpoint);
}

ConnectionPoolStats ConnectionPool::getStats(const std::string& endpoint) const {
    return pImpl_->getStats(endpoint);
}

ConnectionPoolStats ConnectionPool::getAggregateStats() const {
    return pImpl_->getAggregateStats();
}

void ConnectionPool::resetStats(const std::string& endpoint) {
    pImpl_->resetStats(endpoint);
}

void ConnectionPool::closeAll(const std::string& endpoint) {
    pImpl_->closeAll(endpoint);
}

uint32_t ConnectionPool::closeIdle(const std::string& endpoint) {
    return pImpl_->closeIdle(endpoint);
}

uint32_t ConnectionPool::prepopulate(const std::string& endpoint, uint32_t count) {
    return pImpl_->prepopulate(endpoint, count);
}

void ConnectionPool::setValidator(ConnectionValidator validator) {
    pImpl_->setValidator(std::move(validator));
}

void ConnectionPool::updateConfig(const ConnectionPoolConfig& config) {
    pImpl_->updateConfig(config);
}

ConnectionPoolConfig ConnectionPool::getConfig() const {
    return pImpl_->getConfig();
}

void ConnectionPool::releaseConnection(const std::string& endpoint,
                                      TransportPtr transport) {
    pImpl_->releaseConnection(endpoint, transport);
}

// ConnectionPoolBuilder implementation

ConnectionPoolBuilder::ConnectionPoolBuilder() {
    config_ = ConnectionPoolConfig{};
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withFactory(
    ConnectionPool::ConnectionFactory factory) {
    factory_ = std::move(factory);
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMinPoolSize(uint32_t size) {
    config_.min_pool_size = size;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMaxPoolSize(uint32_t size) {
    config_.max_pool_size = size;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withAcquireTimeout(
    std::chrono::milliseconds timeout) {
    config_.acquire_timeout = timeout;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMaxIdleTime(
    std::chrono::milliseconds time) {
    config_.max_idle_time = time;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withEvictionInterval(
    std::chrono::milliseconds interval) {
    config_.eviction_interval = interval;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withMaxLifetime(
    std::chrono::milliseconds lifetime) {
    config_.max_connection_lifetime = lifetime;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::validateOnAcquire(bool enable) {
    config_.validate_on_acquire = enable;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::validateOnRelease(bool enable) {
    config_.validate_on_release = enable;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withLoadBalancing(
    LoadBalancingStrategy strategy) {
    config_.load_balancing = strategy;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::enableHealthCheck(bool enable) {
    config_.enable_health_check = enable;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::waitIfExhausted(bool wait) {
    config_.wait_if_exhausted = wait;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::createOnDemand(bool enable) {
    config_.create_on_demand = enable;
    return *this;
}

ConnectionPoolBuilder& ConnectionPoolBuilder::withValidator(
    ConnectionPool::ConnectionValidator validator) {
    validator_ = std::move(validator);
    return *this;
}

std::unique_ptr<ConnectionPool> ConnectionPoolBuilder::build() {
    if (!factory_) {
        return nullptr;
    }

    auto pool = std::make_unique<ConnectionPool>(config_, std::move(factory_));

    if (validator_) {
        pool->setValidator(std::move(validator_));
    }

    return pool;
}

} // namespace ipc
} // namespace cdmf
