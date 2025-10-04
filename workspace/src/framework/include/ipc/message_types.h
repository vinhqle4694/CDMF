/**
 * @file message_types.h
 * @brief Core message type definitions and enumerations for IPC infrastructure
 *
 * This file defines the fundamental message types, enumerations, and constants
 * used throughout the CDMF IPC layer for inter-process and inter-module communication.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Message Types Agent
 */

#ifndef CDMF_IPC_MESSAGE_TYPES_H
#define CDMF_IPC_MESSAGE_TYPES_H

#include <cstdint>
#include <string>
#include <chrono>

namespace cdmf {
namespace ipc {

/**
 * @brief Message type enumeration
 *
 * Defines the primary categories of messages that can be exchanged
 * between processes or modules in the CDMF framework.
 */
enum class MessageType : uint8_t {
    /** Request message - expects a response */
    REQUEST = 0x01,

    /** Response message - reply to a request */
    RESPONSE = 0x02,

    /** Event message - fire-and-forget notification */
    EVENT = 0x03,

    /** Error message - indicates an error condition */
    ERROR = 0x04,

    /** Heartbeat message - keep-alive signal */
    HEARTBEAT = 0x05,

    /** Control message - system/control operations */
    CONTROL = 0x06,

    /** Unknown/invalid message type */
    UNKNOWN = 0xFF
};

/**
 * @brief Message priority levels
 *
 * Defines priority levels for message processing and scheduling.
 */
enum class MessagePriority : uint8_t {
    /** Low priority - best effort delivery */
    LOW = 0,

    /** Normal priority - default */
    NORMAL = 1,

    /** High priority - expedited processing */
    HIGH = 2,

    /** Critical priority - highest priority */
    CRITICAL = 3
};

/**
 * @brief Message flags bitmask
 *
 * Flags that modify message handling behavior.
 */
enum class MessageFlags : uint32_t {
    /** No special flags */
    NONE = 0x00000000,

    /** Message requires acknowledgment */
    REQUIRE_ACK = 0x00000001,

    /** Message payload is compressed */
    COMPRESSED = 0x00000002,

    /** Message payload is encrypted */
    ENCRYPTED = 0x00000004,

    /** Message is part of a fragmented sequence */
    FRAGMENTED = 0x00000008,

    /** This is the last fragment in a sequence */
    LAST_FRAGMENT = 0x00000010,

    /** Message should be persisted */
    PERSISTENT = 0x00000020,

    /** Message requires ordered delivery */
    ORDERED = 0x00000040,

    /** Message has a timeout/expiration */
    EXPIRES = 0x00000080
};

/**
 * @brief Message status codes
 *
 * Status codes for message processing and delivery.
 */
enum class MessageStatus : uint16_t {
    /** Message created but not sent */
    CREATED = 0,

    /** Message queued for sending */
    QUEUED = 1,

    /** Message sent successfully */
    SENT = 2,

    /** Message delivered to recipient */
    DELIVERED = 3,

    /** Message processed successfully */
    PROCESSED = 4,

    /** Message failed to send */
    SEND_FAILED = 100,

    /** Message delivery failed */
    DELIVERY_FAILED = 101,

    /** Message processing failed */
    PROCESSING_FAILED = 102,

    /** Message timed out */
    TIMEOUT = 103,

    /** Message rejected by recipient */
    REJECTED = 104,

    /** Message format invalid */
    INVALID_FORMAT = 105,

    /** Message size exceeds limits */
    SIZE_EXCEEDED = 106
};

/**
 * @brief Serialization format enumeration
 *
 * Supported serialization formats for message payloads.
 */
enum class SerializationFormat : uint8_t {
    /** Raw binary data (no serialization) */
    BINARY = 0x01,

    /** JSON text format */
    JSON = 0x02,

    /** Protocol Buffers binary format */
    PROTOBUF = 0x03,

    /** MessagePack binary format */
    MESSAGEPACK = 0x04,

    /** Custom application-specific format */
    CUSTOM = 0xFF
};

/**
 * @brief Message header structure
 *
 * Fixed-size header containing message metadata. This structure
 * is always at the beginning of every serialized message.
 *
 * Layout (56 bytes total):
 * - message_id: 16 bytes (UUID-style identifier)
 * - correlation_id: 16 bytes (for request-response matching)
 * - timestamp: 8 bytes (microseconds since epoch)
 * - type: 1 byte
 * - priority: 1 byte
 * - format: 1 byte
 * - version: 1 byte
 * - flags: 4 bytes
 * - payload_size: 4 bytes
 * - checksum: 4 bytes (CRC32 of payload)
 */
struct MessageHeader {
    /** Unique message identifier (128-bit UUID) */
    uint8_t message_id[16];

    /** Correlation ID for request-response matching */
    uint8_t correlation_id[16];

    /** Message timestamp (microseconds since Unix epoch) */
    uint64_t timestamp;

    /** Message type */
    MessageType type;

    /** Message priority */
    MessagePriority priority;

    /** Serialization format of payload */
    SerializationFormat format;

    /** Protocol version (currently 0x01) */
    uint8_t version;

    /** Message flags (bitwise OR of MessageFlags) */
    uint32_t flags;

    /** Size of payload data in bytes */
    uint32_t payload_size;

    /** CRC32 checksum of payload */
    uint32_t checksum;

    /**
     * @brief Default constructor
     * Initializes header with default values
     */
    MessageHeader();

    /**
     * @brief Validates header consistency
     * @return true if header is valid, false otherwise
     */
    bool validate() const;

    /**
     * @brief Checks if a specific flag is set
     * @param flag The flag to check
     * @return true if flag is set, false otherwise
     */
    bool hasFlag(MessageFlags flag) const;

    /**
     * @brief Sets a specific flag
     * @param flag The flag to set
     */
    void setFlag(MessageFlags flag);

    /**
     * @brief Clears a specific flag
     * @param flag The flag to clear
     */
    void clearFlag(MessageFlags flag);
} __attribute__((packed));

// Ensure header is exactly 56 bytes
static_assert(sizeof(MessageHeader) == 56, "MessageHeader must be 56 bytes");

/**
 * @brief Message metadata structure
 *
 * Extended metadata that doesn't need to be in the fixed header.
 */
struct MessageMetadata {
    /** Source endpoint identifier */
    std::string source_endpoint;

    /** Destination endpoint identifier */
    std::string destination_endpoint;

    /** Message subject/topic */
    std::string subject;

    /** Content type (MIME-like) */
    std::string content_type;

    /** Message expiration time (absolute timestamp) */
    std::chrono::system_clock::time_point expiration;

    /** Number of delivery attempts */
    uint32_t retry_count;

    /** Maximum allowed retries */
    uint32_t max_retries;

    /**
     * @brief Default constructor
     */
    MessageMetadata();

    /**
     * @brief Checks if message has expired
     * @return true if expired, false otherwise
     */
    bool isExpired() const;
};

/**
 * @brief Error information structure
 *
 * Detailed error information for ERROR message type.
 */
struct ErrorInfo {
    /** Error code (application-specific) */
    uint32_t error_code;

    /** Error message */
    std::string error_message;

    /** Error category/domain */
    std::string error_category;

    /** Stack trace or additional context */
    std::string error_context;

    /**
     * @brief Default constructor
     */
    ErrorInfo();

    /**
     * @brief Constructor with error details
     * @param code Error code
     * @param message Error message
     */
    ErrorInfo(uint32_t code, const std::string& message);
};

// Common constants
namespace constants {
    /** Maximum message size (16 MB) */
    constexpr uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

    /** Maximum payload size (16 MB - header size) */
    constexpr uint32_t MAX_PAYLOAD_SIZE = MAX_MESSAGE_SIZE - sizeof(MessageHeader);

    /** Protocol version */
    constexpr uint8_t PROTOCOL_VERSION = 0x01;

    /** Default message timeout (30 seconds) */
    constexpr uint32_t DEFAULT_TIMEOUT_MS = 30000;

    /** Maximum number of message fragments */
    constexpr uint32_t MAX_FRAGMENTS = 1024;
}

/**
 * @brief Converts MessageType to string representation
 * @param type The message type
 * @return String representation
 */
const char* messageTypeToString(MessageType type);

/**
 * @brief Converts MessageStatus to string representation
 * @param status The message status
 * @return String representation
 */
const char* messageStatusToString(MessageStatus status);

/**
 * @brief Converts SerializationFormat to string representation
 * @param format The serialization format
 * @return String representation
 */
const char* serializationFormatToString(SerializationFormat format);

/**
 * @brief Bitwise OR operator for MessageFlags
 */
inline MessageFlags operator|(MessageFlags a, MessageFlags b) {
    return static_cast<MessageFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

/**
 * @brief Bitwise AND operator for MessageFlags
 */
inline MessageFlags operator&(MessageFlags a, MessageFlags b) {
    return static_cast<MessageFlags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

/**
 * @brief Bitwise OR-assignment operator for MessageFlags
 */
inline MessageFlags& operator|=(MessageFlags& a, MessageFlags b) {
    a = a | b;
    return a;
}

/**
 * @brief Bitwise AND-assignment operator for MessageFlags
 */
inline MessageFlags& operator&=(MessageFlags& a, MessageFlags b) {
    a = a & b;
    return a;
}

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_MESSAGE_TYPES_H
