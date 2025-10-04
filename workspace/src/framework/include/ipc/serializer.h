/**
 * @file serializer.h
 * @brief Abstract serializer interface for IPC message serialization
 *
 * Defines the abstract interface for message serialization and deserialization.
 * Supports multiple serialization formats (Binary, JSON, Protocol Buffers, MessagePack).
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Message Types Agent
 */

#ifndef CDMF_IPC_SERIALIZER_H
#define CDMF_IPC_SERIALIZER_H

#include "message.h"
#include <memory>
#include <vector>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Serialization result structure
 *
 * Contains the result of a serialization operation including
 * status, data, and any error information.
 */
struct SerializationResult {
    /** Success flag */
    bool success;

    /** Serialized data */
    std::vector<uint8_t> data;

    /** Error message (if success is false) */
    std::string error_message;

    /** Error code (0 if success) */
    uint32_t error_code;

    /**
     * @brief Default constructor (failed result)
     */
    SerializationResult()
        : success(false), error_code(0) {}

    /**
     * @brief Constructor for successful result
     * @param serialized_data The serialized data
     */
    explicit SerializationResult(std::vector<uint8_t>&& serialized_data)
        : success(true), data(std::move(serialized_data)), error_code(0) {}

    /**
     * @brief Constructor for failed result
     * @param code Error code
     * @param message Error message
     */
    SerializationResult(uint32_t code, const std::string& message)
        : success(false), error_message(message), error_code(code) {}
};

/**
 * @brief Deserialization result structure
 *
 * Contains the result of a deserialization operation including
 * status, message, and any error information.
 */
struct DeserializationResult {
    /** Success flag */
    bool success;

    /** Deserialized message */
    MessagePtr message;

    /** Error message (if success is false) */
    std::string error_message;

    /** Error code (0 if success) */
    uint32_t error_code;

    /**
     * @brief Default constructor (failed result)
     */
    DeserializationResult()
        : success(false), error_code(0) {}

    /**
     * @brief Constructor for successful result
     * @param msg The deserialized message
     */
    explicit DeserializationResult(MessagePtr msg)
        : success(true), message(std::move(msg)), error_code(0) {}

    /**
     * @brief Constructor for failed result
     * @param code Error code
     * @param message Error message
     */
    DeserializationResult(uint32_t code, const std::string& message)
        : success(false), error_message(message), error_code(code) {}
};

/**
 * @brief Abstract serializer interface
 *
 * Defines the contract for message serialization and deserialization.
 * Implementations provide format-specific serialization logic.
 *
 * Thread Safety: Implementations should be thread-safe.
 */
class ISerializer {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ISerializer() = default;

    /**
     * @brief Serializes a message to binary data
     *
     * Converts a Message object into a serialized byte stream that can
     * be transmitted or stored. The format depends on the implementation.
     *
     * @param message The message to serialize
     * @return Serialization result containing data or error
     */
    virtual SerializationResult serialize(const Message& message) = 0;

    /**
     * @brief Deserializes binary data to a message
     *
     * Converts a serialized byte stream back into a Message object.
     *
     * @param data Serialized data
     * @param size Size of data in bytes
     * @return Deserialization result containing message or error
     */
    virtual DeserializationResult deserialize(const void* data, uint32_t size) = 0;

    /**
     * @brief Deserializes binary data to a message (vector variant)
     *
     * @param data Serialized data vector
     * @return Deserialization result containing message or error
     */
    virtual DeserializationResult deserialize(const std::vector<uint8_t>& data) {
        return deserialize(data.data(), static_cast<uint32_t>(data.size()));
    }

    /**
     * @brief Gets the serialization format supported by this serializer
     * @return Serialization format
     */
    virtual SerializationFormat getFormat() const = 0;

    /**
     * @brief Gets the name of the serializer
     * @return Serializer name (e.g., "JSON", "ProtoBuf")
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Gets the version of the serializer implementation
     * @return Version string
     */
    virtual std::string getVersion() const = 0;

    /**
     * @brief Validates that data can be deserialized
     *
     * Performs a quick validation without full deserialization.
     *
     * @param data Data to validate
     * @param size Size of data
     * @return true if data appears valid, false otherwise
     */
    virtual bool validate(const void* data, uint32_t size) const = 0;

    /**
     * @brief Estimates the serialized size of a message
     *
     * Provides an estimate of how large the serialized data will be.
     * Useful for buffer pre-allocation.
     *
     * @param message The message to estimate
     * @return Estimated size in bytes
     */
    virtual uint32_t estimateSerializedSize(const Message& message) const = 0;

    /**
     * @brief Checks if this serializer supports compression
     * @return true if compression is supported, false otherwise
     */
    virtual bool supportsCompression() const {
        return false;
    }

    /**
     * @brief Checks if this serializer supports encryption
     * @return true if encryption is supported, false otherwise
     */
    virtual bool supportsEncryption() const {
        return false;
    }

    /**
     * @brief Checks if this serializer supports fragmentation
     * @return true if fragmentation is supported, false otherwise
     */
    virtual bool supportsFragmentation() const {
        return false;
    }

protected:
    /**
     * @brief Protected constructor (interface cannot be instantiated directly)
     */
    ISerializer() = default;

    /**
     * @brief Protected copy constructor
     */
    ISerializer(const ISerializer&) = default;

    /**
     * @brief Protected move constructor
     */
    ISerializer(ISerializer&&) = default;

    /**
     * @brief Protected copy assignment
     */
    ISerializer& operator=(const ISerializer&) = default;

    /**
     * @brief Protected move assignment
     */
    ISerializer& operator=(ISerializer&&) = default;
};

/**
 * @brief Shared pointer type for ISerializer
 */
using SerializerPtr = std::shared_ptr<ISerializer>;

/**
 * @brief Binary serializer implementation
 *
 * Simple binary serializer that writes messages in a compact binary format.
 * This is the most efficient serializer in terms of speed and size.
 *
 * Format:
 * - MessageHeader (56 bytes)
 * - Metadata length (4 bytes)
 * - Metadata (variable)
 * - Payload (variable)
 * - Error info length (4 bytes, only for ERROR messages)
 * - Error info (variable, only for ERROR messages)
 */
class BinarySerializer : public ISerializer {
public:
    /**
     * @brief Constructor
     */
    BinarySerializer() = default;

    /**
     * @brief Destructor
     */
    ~BinarySerializer() override = default;

    /**
     * @brief Serializes a message to binary format
     * @param message The message to serialize
     * @return Serialization result
     */
    SerializationResult serialize(const Message& message) override;

    /**
     * @brief Deserializes binary data to a message
     * @param data Serialized data
     * @param size Size of data
     * @return Deserialization result
     */
    DeserializationResult deserialize(const void* data, uint32_t size) override;

    /**
     * @brief Gets the serialization format
     * @return SerializationFormat::BINARY
     */
    SerializationFormat getFormat() const override {
        return SerializationFormat::BINARY;
    }

    /**
     * @brief Gets the serializer name
     * @return "Binary"
     */
    std::string getName() const override {
        return "Binary";
    }

    /**
     * @brief Gets the serializer version
     * @return Version string
     */
    std::string getVersion() const override {
        return "1.0.0";
    }

    /**
     * @brief Validates binary data
     * @param data Data to validate
     * @param size Size of data
     * @return true if valid, false otherwise
     */
    bool validate(const void* data, uint32_t size) const override;

    /**
     * @brief Estimates serialized size
     * @param message Message to estimate
     * @return Estimated size
     */
    uint32_t estimateSerializedSize(const Message& message) const override;

private:
    /**
     * @brief Serializes metadata to a byte vector
     * @param metadata Metadata to serialize
     * @return Serialized metadata
     */
    std::vector<uint8_t> serializeMetadata(const MessageMetadata& metadata) const;

    /**
     * @brief Deserializes metadata from data
     * @param data Data pointer
     * @param size Data size
     * @param metadata Output metadata
     * @return Number of bytes consumed, or 0 on error
     */
    uint32_t deserializeMetadata(const uint8_t* data, uint32_t size,
                                   MessageMetadata& metadata) const;

    /**
     * @brief Serializes error info to a byte vector
     * @param error Error info to serialize
     * @return Serialized error info
     */
    std::vector<uint8_t> serializeErrorInfo(const ErrorInfo& error) const;

    /**
     * @brief Deserializes error info from data
     * @param data Data pointer
     * @param size Data size
     * @param error Output error info
     * @return Number of bytes consumed, or 0 on error
     */
    uint32_t deserializeErrorInfo(const uint8_t* data, uint32_t size,
                                    ErrorInfo& error) const;
};

/**
 * @brief Serializer factory
 *
 * Factory for creating serializer instances based on format.
 */
class SerializerFactory {
public:
    /**
     * @brief Creates a serializer for the specified format
     * @param format Serialization format
     * @return Serializer instance, or nullptr if format not supported
     */
    static SerializerPtr createSerializer(SerializationFormat format);

    /**
     * @brief Gets the default serializer (Binary)
     * @return Default serializer instance
     */
    static SerializerPtr getDefaultSerializer();

    /**
     * @brief Checks if a format is supported
     * @param format Format to check
     * @return true if supported, false otherwise
     */
    static bool isFormatSupported(SerializationFormat format);

    /**
     * @brief Gets a list of supported formats
     * @return Vector of supported formats
     */
    static std::vector<SerializationFormat> getSupportedFormats();

private:
    SerializerFactory() = delete;
};

// Error codes for serialization/deserialization
namespace error_codes {
    constexpr uint32_t SUCCESS = 0;
    constexpr uint32_t INVALID_MESSAGE = 1;
    constexpr uint32_t SIZE_EXCEEDED = 2;
    constexpr uint32_t INVALID_FORMAT = 3;
    constexpr uint32_t CHECKSUM_MISMATCH = 4;
    constexpr uint32_t INSUFFICIENT_DATA = 5;
    constexpr uint32_t SERIALIZATION_ERROR = 6;
    constexpr uint32_t DESERIALIZATION_ERROR = 7;
    constexpr uint32_t UNSUPPORTED_VERSION = 8;
    constexpr uint32_t MEMORY_ALLOCATION_FAILED = 9;
    constexpr uint32_t UNKNOWN_ERROR = 999;
}

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_SERIALIZER_H
