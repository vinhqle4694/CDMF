/**
 * @file message.h
 * @brief Message class interface for IPC infrastructure
 *
 * Defines the Message class which encapsulates a complete IPC message
 * including header, metadata, payload, and error information.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Message Types Agent
 */

#ifndef CDMF_IPC_MESSAGE_H
#define CDMF_IPC_MESSAGE_H

#include "message_types.h"
#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <chrono>

namespace cdmf {
namespace ipc {

/**
 * @brief Message class for IPC communication
 *
 * Thread-safe message container that manages header, metadata, and payload.
 * Supports validation, serialization metadata, and lifecycle management.
 *
 * Thread Safety: This class is thread-safe for all operations. Internal
 * state is protected by a mutex.
 */
class Message {
public:
    /**
     * @brief Default constructor
     * Creates an empty message with default header values
     */
    Message();

    /**
     * @brief Constructor with message type
     * @param type The message type
     */
    explicit Message(MessageType type);

    /**
     * @brief Constructor with type and payload
     * @param type The message type
     * @param payload Payload data
     * @param size Payload size in bytes
     */
    Message(MessageType type, const void* payload, uint32_t size);

    /**
     * @brief Copy constructor
     * @param other Message to copy
     */
    Message(const Message& other);

    /**
     * @brief Move constructor
     * @param other Message to move
     */
    Message(Message&& other) noexcept;

    /**
     * @brief Copy assignment operator
     * @param other Message to copy
     * @return Reference to this message
     */
    Message& operator=(const Message& other);

    /**
     * @brief Move assignment operator
     * @param other Message to move
     * @return Reference to this message
     */
    Message& operator=(Message&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~Message();

    // Header accessors

    /**
     * @brief Gets the message header
     * @return Const reference to message header
     */
    const MessageHeader& getHeader() const;

    /**
     * @brief Gets mutable message header
     * @return Reference to message header
     */
    MessageHeader& getHeader();

    /**
     * @brief Gets the message ID
     * @param id Output buffer (must be at least 16 bytes)
     */
    void getMessageId(uint8_t* id) const;

    /**
     * @brief Sets the message ID
     * @param id Message ID (must be 16 bytes)
     */
    void setMessageId(const uint8_t* id);

    /**
     * @brief Generates a new unique message ID
     */
    void generateMessageId();

    /**
     * @brief Gets the correlation ID
     * @param id Output buffer (must be at least 16 bytes)
     */
    void getCorrelationId(uint8_t* id) const;

    /**
     * @brief Sets the correlation ID
     * @param id Correlation ID (must be 16 bytes)
     */
    void setCorrelationId(const uint8_t* id);

    /**
     * @brief Gets the message timestamp
     * @return Timestamp in microseconds since Unix epoch
     */
    uint64_t getTimestamp() const;

    /**
     * @brief Sets the message timestamp
     * @param timestamp Timestamp in microseconds since Unix epoch
     */
    void setTimestamp(uint64_t timestamp);

    /**
     * @brief Updates timestamp to current time
     */
    void updateTimestamp();

    /**
     * @brief Gets the message type
     * @return Message type
     */
    MessageType getType() const;

    /**
     * @brief Sets the message type
     * @param type Message type
     */
    void setType(MessageType type);

    /**
     * @brief Gets the message priority
     * @return Message priority
     */
    MessagePriority getPriority() const;

    /**
     * @brief Sets the message priority
     * @param priority Message priority
     */
    void setPriority(MessagePriority priority);

    /**
     * @brief Gets the serialization format
     * @return Serialization format
     */
    SerializationFormat getFormat() const;

    /**
     * @brief Sets the serialization format
     * @param format Serialization format
     */
    void setFormat(SerializationFormat format);

    /**
     * @brief Gets the protocol version
     * @return Protocol version
     */
    uint8_t getVersion() const;

    /**
     * @brief Checks if a flag is set
     * @param flag Flag to check
     * @return true if flag is set, false otherwise
     */
    bool hasFlag(MessageFlags flag) const;

    /**
     * @brief Sets a flag
     * @param flag Flag to set
     */
    void setFlag(MessageFlags flag);

    /**
     * @brief Clears a flag
     * @param flag Flag to clear
     */
    void clearFlag(MessageFlags flag);

    /**
     * @brief Gets all flags
     * @return Current flags value
     */
    uint32_t getFlags() const;

    /**
     * @brief Sets all flags
     * @param flags New flags value
     */
    void setFlags(uint32_t flags);

    // Payload management

    /**
     * @brief Gets the payload data
     * @return Const pointer to payload data (may be nullptr if empty)
     */
    const uint8_t* getPayload() const;

    /**
     * @brief Gets mutable payload data
     * @return Pointer to payload data (may be nullptr if empty)
     */
    uint8_t* getPayload();

    /**
     * @brief Gets the payload size
     * @return Payload size in bytes
     */
    uint32_t getPayloadSize() const;

    /**
     * @brief Sets the payload data (copies data)
     * @param data Payload data
     * @param size Payload size in bytes
     * @return true if successful, false if size exceeds maximum
     */
    bool setPayload(const void* data, uint32_t size);

    /**
     * @brief Sets the payload data (moves data)
     * @param data Payload data vector
     * @return true if successful, false if size exceeds maximum
     */
    bool setPayload(std::vector<uint8_t>&& data);

    /**
     * @brief Clears the payload
     */
    void clearPayload();

    /**
     * @brief Appends data to payload
     * @param data Data to append
     * @param size Size of data
     * @return true if successful, false if would exceed maximum size
     */
    bool appendPayload(const void* data, uint32_t size);

    // Metadata management

    /**
     * @brief Gets the message metadata
     * @return Const reference to metadata
     */
    const MessageMetadata& getMetadata() const;

    /**
     * @brief Gets mutable message metadata
     * @return Reference to metadata
     */
    MessageMetadata& getMetadata();

    /**
     * @brief Sets the source endpoint
     * @param endpoint Source endpoint identifier
     */
    void setSourceEndpoint(const std::string& endpoint);

    /**
     * @brief Gets the source endpoint
     * @return Source endpoint identifier
     */
    std::string getSourceEndpoint() const;

    /**
     * @brief Sets the destination endpoint
     * @param endpoint Destination endpoint identifier
     */
    void setDestinationEndpoint(const std::string& endpoint);

    /**
     * @brief Gets the destination endpoint
     * @return Destination endpoint identifier
     */
    std::string getDestinationEndpoint() const;

    /**
     * @brief Sets the message subject
     * @param subject Message subject/topic
     */
    void setSubject(const std::string& subject);

    /**
     * @brief Gets the message subject
     * @return Message subject/topic
     */
    std::string getSubject() const;

    // Error information (for ERROR message type)

    /**
     * @brief Gets the error information
     * @return Const reference to error info
     */
    const ErrorInfo& getErrorInfo() const;

    /**
     * @brief Gets mutable error information
     * @return Reference to error info
     */
    ErrorInfo& getErrorInfo();

    /**
     * @brief Sets error information
     * @param code Error code
     * @param message Error message
     */
    void setError(uint32_t code, const std::string& message);

    /**
     * @brief Checks if this is an error message
     * @return true if type is ERROR, false otherwise
     */
    bool isError() const;

    // Validation and integrity

    /**
     * @brief Validates the message
     *
     * Checks:
     * - Header validity
     * - Payload size within limits
     * - Checksum correctness
     * - Metadata consistency
     *
     * @return true if valid, false otherwise
     */
    bool validate() const;

    /**
     * @brief Computes the checksum of the payload
     * @return CRC32 checksum
     */
    uint32_t computeChecksum() const;

    /**
     * @brief Updates the checksum in the header
     */
    void updateChecksum();

    /**
     * @brief Verifies the checksum matches the payload
     * @return true if checksum is valid, false otherwise
     */
    bool verifyChecksum() const;

    // Status tracking

    /**
     * @brief Gets the message status
     * @return Current status
     */
    MessageStatus getStatus() const;

    /**
     * @brief Sets the message status
     * @param status New status
     */
    void setStatus(MessageStatus status);

    // Utility methods

    /**
     * @brief Gets the total message size (header + payload)
     * @return Total size in bytes
     */
    uint32_t getTotalSize() const;

    /**
     * @brief Checks if the message is empty (no payload)
     * @return true if empty, false otherwise
     */
    bool isEmpty() const;

    /**
     * @brief Clears all message data
     */
    void clear();

    /**
     * @brief Creates a response message for this message
     * @return New message with correlation ID set to this message's ID
     */
    Message createResponse() const;

    /**
     * @brief Creates an error response message
     * @param code Error code
     * @param message Error message
     * @return Error message with correlation ID set to this message's ID
     */
    Message createErrorResponse(uint32_t code, const std::string& message) const;

    /**
     * @brief Gets a string representation of the message (for debugging)
     * @return String representation
     */
    std::string toString() const;

private:
    /** Message header */
    MessageHeader header_;

    /** Message metadata */
    MessageMetadata metadata_;

    /** Payload data */
    std::vector<uint8_t> payload_;

    /** Error information (for ERROR messages) */
    ErrorInfo error_info_;

    /** Current message status */
    MessageStatus status_;

    /** Mutex for thread safety */
    mutable std::mutex mutex_;

    /**
     * @brief Computes CRC32 checksum
     * @param data Data to checksum
     * @param size Size of data
     * @return CRC32 checksum
     */
    static uint32_t crc32(const void* data, uint32_t size);
};

/**
 * @brief Shared pointer type for Message
 */
using MessagePtr = std::shared_ptr<Message>;

/**
 * @brief Unique pointer type for Message
 */
using MessageUniquePtr = std::unique_ptr<Message>;

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_MESSAGE_H
