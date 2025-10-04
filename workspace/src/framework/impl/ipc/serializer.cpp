/**
 * @file serializer.cpp
 * @brief Serializer implementation for IPC messages
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Message Types Agent
 */

#include "ipc/serializer.h"
#include "utils/log.h"
#include <cstring>
#include <algorithm>

namespace cdmf {
namespace ipc {

// Helper functions for binary serialization

namespace {

/**
 * @brief Writes a uint32_t value to buffer in little-endian format
 */
void writeUInt32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

/**
 * @brief Reads a uint32_t value from buffer in little-endian format
 */
uint32_t readUInt32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/**
 * @brief Writes a uint64_t value to buffer in little-endian format
 */
void writeUInt64(std::vector<uint8_t>& buffer, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

/**
 * @brief Reads a uint64_t value from buffer in little-endian format
 */
uint64_t readUInt64(const uint8_t* data) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return value;
}

/**
 * @brief Writes a string to buffer with length prefix
 */
void writeString(std::vector<uint8_t>& buffer, const std::string& str) {
    uint32_t length = static_cast<uint32_t>(str.size());
    writeUInt32(buffer, length);
    if (length > 0) {
        buffer.insert(buffer.end(), str.begin(), str.end());
    }
}

/**
 * @brief Reads a string from buffer with length prefix
 * @return Number of bytes consumed, or 0 on error
 */
uint32_t readString(const uint8_t* data, uint32_t size, std::string& str) {
    if (size < 4) {
        return 0;
    }

    uint32_t length = readUInt32(data);
    if (length > size - 4) {
        return 0;
    }

    if (length > 0) {
        str.assign(reinterpret_cast<const char*>(data + 4), length);
    } else {
        str.clear();
    }

    return 4 + length;
}

} // anonymous namespace

// BinarySerializer implementation

SerializationResult BinarySerializer::serialize(const Message& message) {
    try {
        const MessageHeader& header = message.getHeader();
        LOGD_FMT("BinarySerializer::serialize - payload_size: " << header.payload_size
                 << ", checksum: " << header.checksum
                 << ", type: " << static_cast<int>(header.type));

        std::vector<uint8_t> data;

        // Reserve approximate size
        uint32_t estimated_size = estimateSerializedSize(message);
        data.reserve(estimated_size);

        // Serialize header (56 bytes)
        const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
        data.insert(data.end(), header_bytes, header_bytes + sizeof(MessageHeader));

        // Serialize metadata
        std::vector<uint8_t> metadata_data = serializeMetadata(message.getMetadata());
        writeUInt32(data, static_cast<uint32_t>(metadata_data.size()));
        data.insert(data.end(), metadata_data.begin(), metadata_data.end());

        // Serialize payload
        uint32_t payload_size = message.getPayloadSize();
        if (payload_size > 0) {
            const uint8_t* payload = message.getPayload();
            data.insert(data.end(), payload, payload + payload_size);
        }

        // Serialize error info if this is an error message
        if (message.isError()) {
            std::vector<uint8_t> error_data = serializeErrorInfo(message.getErrorInfo());
            writeUInt32(data, static_cast<uint32_t>(error_data.size()));
            data.insert(data.end(), error_data.begin(), error_data.end());
        }

        LOGD_FMT("BinarySerializer::serialize complete - total_size: " << data.size());
        return SerializationResult(std::move(data));

    } catch (const std::exception& e) {
        LOGE_FMT("BinarySerializer::serialize failed: " << e.what());
        return SerializationResult(error_codes::SERIALIZATION_ERROR, e.what());
    } catch (...) {
        LOGE_FMT("BinarySerializer::serialize failed with unknown error");
        return SerializationResult(error_codes::UNKNOWN_ERROR, "Unknown serialization error");
    }
}

DeserializationResult BinarySerializer::deserialize(const void* data, uint32_t size) {
    try {
        LOGD_FMT("BinarySerializer::deserialize - data_size: " << size);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint32_t offset = 0;

        // Check minimum size (header + metadata length)
        if (size < sizeof(MessageHeader) + 4) {
            LOGE_FMT("BinarySerializer::deserialize - insufficient data for header");
            return DeserializationResult(error_codes::INSUFFICIENT_DATA,
                                          "Insufficient data for message header");
        }

        // Create message
        auto message = std::make_shared<Message>();

        // Deserialize header
        MessageHeader& header = message->getHeader();
        std::memcpy(&header, bytes, sizeof(MessageHeader));
        offset += sizeof(MessageHeader);

        LOGD_FMT("BinarySerializer::deserialize - header deserialized - payload_size: "
                 << header.payload_size << ", checksum: " << header.checksum
                 << ", type: " << static_cast<int>(header.type));

        // Validate header
        if (!header.validate()) {
            return DeserializationResult(error_codes::INVALID_FORMAT,
                                          "Invalid message header");
        }

        // Deserialize metadata
        if (offset + 4 > size) {
            return DeserializationResult(error_codes::INSUFFICIENT_DATA,
                                          "Insufficient data for metadata length");
        }

        uint32_t metadata_size = readUInt32(bytes + offset);
        offset += 4;

        if (offset + metadata_size > size) {
            return DeserializationResult(error_codes::INSUFFICIENT_DATA,
                                          "Insufficient data for metadata");
        }

        MessageMetadata& metadata = message->getMetadata();
        uint32_t consumed = deserializeMetadata(bytes + offset, metadata_size, metadata);
        if (consumed == 0) {
            return DeserializationResult(error_codes::DESERIALIZATION_ERROR,
                                          "Failed to deserialize metadata");
        }
        offset += metadata_size;

        // Deserialize payload
        if (header.payload_size > 0) {
            if (offset + header.payload_size > size) {
                return DeserializationResult(error_codes::INSUFFICIENT_DATA,
                                              "Insufficient data for payload");
            }

            if (!message->setPayload(bytes + offset, header.payload_size)) {
                return DeserializationResult(error_codes::SIZE_EXCEEDED,
                                              "Payload size exceeds maximum");
            }
            offset += header.payload_size;
        }

        // Deserialize error info if this is an error message
        if (header.type == MessageType::ERROR) {
            if (offset + 4 > size) {
                return DeserializationResult(error_codes::INSUFFICIENT_DATA,
                                              "Insufficient data for error info length");
            }

            uint32_t error_size = readUInt32(bytes + offset);
            offset += 4;

            if (offset + error_size > size) {
                return DeserializationResult(error_codes::INSUFFICIENT_DATA,
                                              "Insufficient data for error info");
            }

            ErrorInfo& error_info = message->getErrorInfo();
            consumed = deserializeErrorInfo(bytes + offset, error_size, error_info);
            if (consumed == 0) {
                return DeserializationResult(error_codes::DESERIALIZATION_ERROR,
                                              "Failed to deserialize error info");
            }
            offset += error_size;
        }

        // Verify checksum
        if (!message->verifyChecksum()) {
            LOGE_FMT("BinarySerializer::deserialize - checksum verification failed");
            return DeserializationResult(error_codes::CHECKSUM_MISMATCH,
                                          "Message checksum verification failed");
        }

        LOGD_FMT("BinarySerializer::deserialize complete - message valid");
        return DeserializationResult(message);

    } catch (const std::exception& e) {
        LOGE_FMT("BinarySerializer::deserialize failed: " << e.what());
        return DeserializationResult(error_codes::DESERIALIZATION_ERROR, e.what());
    } catch (...) {
        LOGE_FMT("BinarySerializer::deserialize failed with unknown error");
        return DeserializationResult(error_codes::UNKNOWN_ERROR, "Unknown deserialization error");
    }
}

bool BinarySerializer::validate(const void* data, uint32_t size) const {
    if (size < sizeof(MessageHeader)) {
        return false;
    }

    const MessageHeader* header = static_cast<const MessageHeader*>(data);
    return header->validate();
}

uint32_t BinarySerializer::estimateSerializedSize(const Message& message) const {
    uint32_t size = 0;

    // Header
    size += sizeof(MessageHeader);

    // Metadata length prefix
    size += 4;

    // Metadata fields (approximate)
    const MessageMetadata& metadata = message.getMetadata();
    size += 4 + static_cast<uint32_t>(metadata.source_endpoint.size());
    size += 4 + static_cast<uint32_t>(metadata.destination_endpoint.size());
    size += 4 + static_cast<uint32_t>(metadata.subject.size());
    size += 4 + static_cast<uint32_t>(metadata.content_type.size());
    size += 8; // expiration timestamp
    size += 8; // retry_count + max_retries

    // Payload
    size += message.getPayloadSize();

    // Error info (if applicable)
    if (message.isError()) {
        size += 4; // error info length prefix
        const ErrorInfo& error = message.getErrorInfo();
        size += 4; // error_code
        size += 4 + static_cast<uint32_t>(error.error_message.size());
        size += 4 + static_cast<uint32_t>(error.error_category.size());
        size += 4 + static_cast<uint32_t>(error.error_context.size());
    }

    return size;
}

std::vector<uint8_t> BinarySerializer::serializeMetadata(const MessageMetadata& metadata) const {
    std::vector<uint8_t> data;

    // Serialize strings
    writeString(data, metadata.source_endpoint);
    writeString(data, metadata.destination_endpoint);
    writeString(data, metadata.subject);
    writeString(data, metadata.content_type);

    // Serialize expiration timestamp
    auto expiration_time = metadata.expiration.time_since_epoch();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(expiration_time);
    writeUInt64(data, static_cast<uint64_t>(micros.count()));

    // Serialize retry counts
    writeUInt32(data, metadata.retry_count);
    writeUInt32(data, metadata.max_retries);

    return data;
}

uint32_t BinarySerializer::deserializeMetadata(const uint8_t* data, uint32_t size,
                                                 MessageMetadata& metadata) const {
    uint32_t offset = 0;
    uint32_t consumed;

    // Deserialize strings
    consumed = readString(data + offset, size - offset, metadata.source_endpoint);
    if (consumed == 0) return 0;
    offset += consumed;

    consumed = readString(data + offset, size - offset, metadata.destination_endpoint);
    if (consumed == 0) return 0;
    offset += consumed;

    consumed = readString(data + offset, size - offset, metadata.subject);
    if (consumed == 0) return 0;
    offset += consumed;

    consumed = readString(data + offset, size - offset, metadata.content_type);
    if (consumed == 0) return 0;
    offset += consumed;

    // Deserialize expiration timestamp
    if (offset + 8 > size) return 0;
    uint64_t expiration_micros = readUInt64(data + offset);
    offset += 8;

    auto duration = std::chrono::microseconds(expiration_micros);
    metadata.expiration = std::chrono::system_clock::time_point(duration);

    // Deserialize retry counts
    if (offset + 8 > size) return 0;
    metadata.retry_count = readUInt32(data + offset);
    offset += 4;
    metadata.max_retries = readUInt32(data + offset);
    offset += 4;

    return offset;
}

std::vector<uint8_t> BinarySerializer::serializeErrorInfo(const ErrorInfo& error) const {
    std::vector<uint8_t> data;

    // Serialize error code
    writeUInt32(data, error.error_code);

    // Serialize error strings
    writeString(data, error.error_message);
    writeString(data, error.error_category);
    writeString(data, error.error_context);

    return data;
}

uint32_t BinarySerializer::deserializeErrorInfo(const uint8_t* data, uint32_t size,
                                                  ErrorInfo& error) const {
    uint32_t offset = 0;
    uint32_t consumed;

    // Deserialize error code
    if (offset + 4 > size) return 0;
    error.error_code = readUInt32(data + offset);
    offset += 4;

    // Deserialize error strings
    consumed = readString(data + offset, size - offset, error.error_message);
    if (consumed == 0) return 0;
    offset += consumed;

    consumed = readString(data + offset, size - offset, error.error_category);
    if (consumed == 0) return 0;
    offset += consumed;

    consumed = readString(data + offset, size - offset, error.error_context);
    if (consumed == 0) return 0;
    offset += consumed;

    return offset;
}

// SerializerFactory implementation is in serializer_factory.cpp

} // namespace ipc
} // namespace cdmf
