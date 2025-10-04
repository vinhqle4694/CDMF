/**
 * @file protobuf_serializer.h
 * @brief Protocol Buffers serializer implementation for IPC messages
 *
 * Implements the ISerializer interface using Google Protocol Buffers.
 * Provides efficient, versioned binary serialization with schema evolution support.
 *
 * Features:
 * - Compact binary encoding
 * - Forward and backward compatibility
 * - Schema versioning
 * - Cross-language support
 * - Efficient varint encoding
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Serialization Agent
 */

#ifndef CDMF_IPC_PROTOBUF_SERIALIZER_H
#define CDMF_IPC_PROTOBUF_SERIALIZER_H

#include "serializer.h"
#include <mutex>

namespace cdmf {
namespace ipc {

/**
 * @brief Protocol Buffers serializer implementation
 *
 * Serializes/deserializes messages using Google Protocol Buffers format.
 * Thread-safe implementation with internal synchronization.
 *
 * Advantages:
 * - Compact binary format (smaller than JSON, comparable to Binary)
 * - Schema evolution (add/remove fields without breaking compatibility)
 * - Cross-platform and cross-language support
 * - Extensive tooling and documentation
 *
 * Performance:
 * - Serialization: ~300-500 MB/s (varint encoding overhead)
 * - Deserialization: ~250-400 MB/s (parsing overhead)
 * - Size: Typically 10-30% smaller than binary due to varint encoding
 *
 * Thread Safety: All methods are thread-safe.
 */
class ProtoBufSerializer : public ISerializer {
public:
    /**
     * @brief Constructor
     */
    ProtoBufSerializer();

    /**
     * @brief Destructor
     */
    ~ProtoBufSerializer() override;

    /**
     * @brief Serializes a message to Protocol Buffers format
     *
     * Converts a Message object into Protocol Buffers wire format.
     * The format is self-describing and supports schema evolution.
     *
     * @param message The message to serialize
     * @return Serialization result containing protobuf data or error
     */
    SerializationResult serialize(const Message& message) override;

    /**
     * @brief Deserializes Protocol Buffers data to a message
     *
     * Parses Protocol Buffers wire format back into a Message object.
     * Handles missing fields gracefully (schema evolution).
     *
     * @param data Serialized protobuf data
     * @param size Size of data in bytes
     * @return Deserialization result containing message or error
     */
    DeserializationResult deserialize(const void* data, uint32_t size) override;

    /**
     * @brief Gets the serialization format
     * @return SerializationFormat::PROTOBUF
     */
    SerializationFormat getFormat() const override {
        return SerializationFormat::PROTOBUF;
    }

    /**
     * @brief Gets the serializer name
     * @return "ProtoBuf"
     */
    std::string getName() const override {
        return "ProtoBuf";
    }

    /**
     * @brief Gets the serializer version
     * @return Version string
     */
    std::string getVersion() const override {
        return "1.0.0";
    }

    /**
     * @brief Validates Protocol Buffers data
     *
     * Performs quick validation without full deserialization.
     * Checks wire format integrity and required fields.
     *
     * @param data Data to validate
     * @param size Size of data
     * @return true if data appears valid, false otherwise
     */
    bool validate(const void* data, uint32_t size) const override;

    /**
     * @brief Estimates serialized size
     *
     * Provides an estimate of the Protocol Buffers wire format size.
     * Actual size may vary due to varint encoding.
     *
     * @param message Message to estimate
     * @return Estimated size in bytes
     */
    uint32_t estimateSerializedSize(const Message& message) const override;

    /**
     * @brief Checks if compression is supported
     * @return false (protobuf handles compression separately)
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

private:
    /** Mutex for thread safety */
    mutable std::mutex mutex_;

    /**
     * @brief Converts MessageType enum to protobuf enum
     * @param type CDMF message type
     * @return Protobuf message type value
     */
    static int convertMessageType(MessageType type);

    /**
     * @brief Converts protobuf enum to MessageType
     * @param proto_type Protobuf message type value
     * @return CDMF message type
     */
    static MessageType convertFromProtoMessageType(int proto_type);

    /**
     * @brief Converts MessagePriority enum to protobuf enum
     * @param priority CDMF message priority
     * @return Protobuf priority value
     */
    static int convertMessagePriority(MessagePriority priority);

    /**
     * @brief Converts protobuf enum to MessagePriority
     * @param proto_priority Protobuf priority value
     * @return CDMF message priority
     */
    static MessagePriority convertFromProtoPriority(int proto_priority);

    /**
     * @brief Converts SerializationFormat enum to protobuf enum
     * @param format CDMF serialization format
     * @return Protobuf format value
     */
    static int convertSerializationFormat(SerializationFormat format);

    /**
     * @brief Converts protobuf enum to SerializationFormat
     * @param proto_format Protobuf format value
     * @return CDMF serialization format
     */
    static SerializationFormat convertFromProtoFormat(int proto_format);
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_PROTOBUF_SERIALIZER_H
