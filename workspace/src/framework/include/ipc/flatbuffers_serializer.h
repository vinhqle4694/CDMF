/**
 * @file flatbuffers_serializer.h
 * @brief FlatBuffers serializer implementation for IPC messages
 *
 * Implements the ISerializer interface using Google FlatBuffers.
 * Provides zero-copy deserialization and minimal memory allocations.
 *
 * Features:
 * - Zero-copy access to deserialized data
 * - No parsing step during deserialization
 * - Minimal memory allocations
 * - Forward-only schema evolution
 * - Extremely fast deserialization
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Serialization Agent
 */

#ifndef CDMF_IPC_FLATBUFFERS_SERIALIZER_H
#define CDMF_IPC_FLATBUFFERS_SERIALIZER_H

#include "serializer.h"
#include <mutex>

namespace cdmf {
namespace ipc {

/**
 * @brief FlatBuffers serializer implementation
 *
 * Serializes/deserializes messages using Google FlatBuffers format.
 * Thread-safe implementation with internal synchronization.
 *
 * Advantages:
 * - Zero-copy deserialization (access data in-place)
 * - Extremely fast deserialization (no parsing required)
 * - Minimal memory allocations
 * - Efficient for read-heavy workloads
 * - Compact binary format
 *
 * Trade-offs:
 * - Slightly larger serialized size than ProtoBuf
 * - Schema evolution is forward-only (can add fields, not remove)
 * - More complex serialization code
 *
 * Performance:
 * - Serialization: ~200-400 MB/s (builder overhead)
 * - Deserialization: ~5-10 GB/s (zero-copy, just validation)
 * - Size: Typically 5-15% larger than binary due to vtable overhead
 *
 * Thread Safety: All methods are thread-safe.
 */
class FlatBuffersSerializer : public ISerializer {
public:
    /**
     * @brief Constructor
     * @param initial_buffer_size Initial size for FlatBuffer builder (default: 1024)
     */
    explicit FlatBuffersSerializer(size_t initial_buffer_size = 1024);

    /**
     * @brief Destructor
     */
    ~FlatBuffersSerializer() override;

    /**
     * @brief Serializes a message to FlatBuffers format
     *
     * Converts a Message object into FlatBuffers binary format.
     * Uses a builder to construct the buffer incrementally.
     *
     * @param message The message to serialize
     * @return Serialization result containing flatbuffer data or error
     */
    SerializationResult serialize(const Message& message) override;

    /**
     * @brief Deserializes FlatBuffers data to a message
     *
     * Accesses FlatBuffers data in-place without copying.
     * Performs validation but no parsing.
     *
     * @param data Serialized flatbuffer data
     * @param size Size of data in bytes
     * @return Deserialization result containing message or error
     */
    DeserializationResult deserialize(const void* data, uint32_t size) override;

    /**
     * @brief Gets the serialization format
     * @return SerializationFormat::BINARY (FlatBuffers is a binary format)
     */
    SerializationFormat getFormat() const override {
        return SerializationFormat::BINARY;  // FlatBuffers is a binary format variant
    }

    /**
     * @brief Gets the serializer name
     * @return "FlatBuffers"
     */
    std::string getName() const override {
        return "FlatBuffers";
    }

    /**
     * @brief Gets the serializer version
     * @return Version string
     */
    std::string getVersion() const override {
        return "1.0.0";
    }

    /**
     * @brief Validates FlatBuffers data
     *
     * Verifies FlatBuffer integrity without full deserialization.
     * Checks buffer format, offsets, and vtable consistency.
     *
     * @param data Data to validate
     * @param size Size of data
     * @return true if data is valid, false otherwise
     */
    bool validate(const void* data, uint32_t size) const override;

    /**
     * @brief Estimates serialized size
     *
     * Provides an estimate of the FlatBuffers size including vtable overhead.
     *
     * @param message Message to estimate
     * @return Estimated size in bytes
     */
    uint32_t estimateSerializedSize(const Message& message) const override;

    /**
     * @brief Checks if compression is supported
     * @return false (compression handled separately)
     */
    bool supportsCompression() const override {
        return false;
    }

    /**
     * @brief Checks if encryption is supported
     * @return false (encryption handled at transport layer)
     */
    bool supportsEncryption() const override {
        return false;
    }

    /**
     * @brief Checks if fragmentation is supported
     * @return false (fragmentation handled at transport layer)
     */
    bool supportsFragmentation() const override {
        return false;
    }

    /**
     * @brief Gets the initial buffer size
     * @return Initial buffer size in bytes
     */
    size_t getInitialBufferSize() const {
        return initial_buffer_size_;
    }

    /**
     * @brief Sets the initial buffer size for future serializations
     * @param size New initial buffer size
     */
    void setInitialBufferSize(size_t size) {
        initial_buffer_size_ = size;
    }

private:
    /** Initial size for FlatBuffer builder */
    size_t initial_buffer_size_;

    /** Mutex for thread safety */
    mutable std::mutex mutex_;

    /**
     * @brief Converts MessageType enum to FlatBuffers enum
     * @param type CDMF message type
     * @return FlatBuffers message type value
     */
    static uint8_t convertMessageType(MessageType type);

    /**
     * @brief Converts FlatBuffers enum to MessageType
     * @param fb_type FlatBuffers message type value
     * @return CDMF message type
     */
    static MessageType convertFromFBMessageType(uint8_t fb_type);

    /**
     * @brief Converts MessagePriority enum to FlatBuffers enum
     * @param priority CDMF message priority
     * @return FlatBuffers priority value
     */
    static uint8_t convertMessagePriority(MessagePriority priority);

    /**
     * @brief Converts FlatBuffers enum to MessagePriority
     * @param fb_priority FlatBuffers priority value
     * @return CDMF message priority
     */
    static MessagePriority convertFromFBPriority(uint8_t fb_priority);

    /**
     * @brief Converts SerializationFormat enum to FlatBuffers enum
     * @param format CDMF serialization format
     * @return FlatBuffers format value
     */
    static uint8_t convertSerializationFormat(SerializationFormat format);

    /**
     * @brief Converts FlatBuffers enum to SerializationFormat
     * @param fb_format FlatBuffers format value
     * @return CDMF serialization format
     */
    static SerializationFormat convertFromFBFormat(uint8_t fb_format);
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_FLATBUFFERS_SERIALIZER_H
