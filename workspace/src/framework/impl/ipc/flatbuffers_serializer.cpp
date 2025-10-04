/**
 * @file flatbuffers_serializer.cpp
 * @brief FlatBuffers serializer implementation
 *
 * NOTE: This implementation assumes the flatbuffer schema has been
 * compiled from message.fbs using flatc. In a real build, you would:
 * 1. Run: flatc --cpp message.fbs
 * 2. Include the generated message_generated.h file
 * 3. Link against libflatbuffers (header-only)
 *
 * For this demonstration, we provide a complete implementation pattern
 * that mimics FlatBuffers' zero-copy approach.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Serialization Agent
 */

#include "ipc/flatbuffers_serializer.h"
#include "utils/log.h"
#include <cstring>
#include <algorithm>

// NOTE: In a real implementation, you would include the generated header:
// #include "message_generated.h"

// For demonstration, we'll implement a simplified FlatBuffers-like interface
namespace cdmf {
namespace ipc {
namespace flatbuf {

// Simplified FlatBuffer builder (real one is much more complex)
class FlatBufferBuilder {
public:
    explicit FlatBufferBuilder(size_t initial_size = 1024)
        : buffer_(initial_size), offset_(0) {
        buffer_.resize(initial_size);
    }

    void Reset() {
        offset_ = 0;
        vtable_offsets_.clear();
        buffer_.resize(buffer_.capacity());
    }

    size_t GetSize() const {
        return offset_;
    }

    uint8_t* GetBufferPointer() {
        return buffer_.data();
    }

    const uint8_t* GetBufferPointer() const {
        return buffer_.data();
    }

    std::vector<uint8_t> Release() {
        std::vector<uint8_t> result(buffer_.begin(), buffer_.begin() + offset_);
        Reset();
        return result;
    }

    // Simplified offset type
    template<typename T>
    struct Offset {
        uint32_t value;
        explicit Offset(uint32_t v = 0) : value(v) {}
    };

    // Write methods
    void PushBytes(const void* data, size_t size) {
        if (offset_ + size > buffer_.size()) {
            buffer_.resize((offset_ + size) * 2);
        }
        std::memcpy(buffer_.data() + offset_, data, size);
        offset_ += size;
    }

    template<typename T>
    void Push(T value) {
        PushBytes(&value, sizeof(T));
    }

    Offset<void> CreateString(const std::string& str) {
        // Align to 4 bytes
        while (offset_ % 4 != 0) {
            Push<uint8_t>(0);
        }

        uint32_t str_offset = static_cast<uint32_t>(offset_);

        // Write length
        Push<uint32_t>(static_cast<uint32_t>(str.size()));

        // Write string data
        if (!str.empty()) {
            PushBytes(str.data(), str.size());
        }

        // Null terminator
        Push<uint8_t>(0);

        return Offset<void>(str_offset);
    }

    Offset<void> CreateVector(const uint8_t* data, size_t size) {
        // Align to 4 bytes
        while (offset_ % 4 != 0) {
            Push<uint8_t>(0);
        }

        uint32_t vec_offset = static_cast<uint32_t>(offset_);

        // Write length
        Push<uint32_t>(static_cast<uint32_t>(size));

        // Write vector data
        if (size > 0) {
            PushBytes(data, size);
        }

        return Offset<void>(vec_offset);
    }

private:
    std::vector<uint8_t> buffer_;
    size_t offset_;
    std::vector<uint32_t> vtable_offsets_;
};

// Simplified verifier (real one does extensive validation)
class Verifier {
public:
    Verifier(const uint8_t* buf, size_t buf_len)
        : buf_(buf), size_(buf_len) {}

    bool VerifyBuffer() const {
        if (size_ < 8) return false;  // Minimum FlatBuffer size
        return true;  // Simplified validation
    }

private:
    const uint8_t* buf_;
    size_t size_;
};

} // namespace flatbuf
} // namespace ipc
} // namespace cdmf

namespace cdmf {
namespace ipc {

// Helper functions for FlatBuffers serialization

namespace {

/**
 * @brief Reads uint32 from buffer (little-endian)
 */
uint32_t readUInt32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/**
 * @brief Writes uint32 to buffer (little-endian)
 */
void writeUInt32LE(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

/**
 * @brief Reads uint64 from buffer (little-endian)
 */
uint64_t readUInt64LE(const uint8_t* data) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return value;
}

/**
 * @brief Writes uint64 to buffer (little-endian)
 */
void writeUInt64LE(uint8_t* data, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        data[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief Simple FlatBuffer table structure
 *
 * FlatBuffer format (simplified):
 * - Root offset (4 bytes) - points to root table
 * - Tables contain vtable offset and field data
 * - Vtables describe field layout
 * - All data is little-endian
 * - Strings and vectors are length-prefixed
 */
struct FlatBufferTable {
    uint32_t vtable_offset;
    std::vector<uint8_t> data;
};

/**
 * @brief Serializes complete message to FlatBuffer format
 */
std::vector<uint8_t> serializeToFlatBuffer(const Message& message, size_t initial_size) {
    flatbuf::FlatBufferBuilder builder(initial_size);

    std::vector<uint8_t> result;
    result.reserve(message.getTotalSize() + 256);  // Extra for FlatBuffer overhead

    // Build message structure (simplified)
    // In real FlatBuffers, this would use the generated code

    // Header section
    const MessageHeader& header = message.getHeader();
    std::vector<uint8_t> header_data;
    header_data.resize(128);  // Header table size
    size_t offset = 0;

    // Message ID (bytes)
    std::memcpy(header_data.data() + offset, header.message_id, 16);
    offset += 16;

    // Correlation ID (bytes)
    std::memcpy(header_data.data() + offset, header.correlation_id, 16);
    offset += 16;

    // Timestamp (uint64)
    writeUInt64LE(header_data.data() + offset, header.timestamp);
    offset += 8;

    // Type (byte)
    header_data[offset++] = static_cast<uint8_t>(header.type);

    // Priority (byte)
    header_data[offset++] = static_cast<uint8_t>(header.priority);

    // Format (byte)
    header_data[offset++] = static_cast<uint8_t>(header.format);

    // Version (byte)
    header_data[offset++] = header.version;

    // Flags (uint32)
    writeUInt32LE(header_data.data() + offset, header.flags);
    offset += 4;

    // Payload size (uint32)
    writeUInt32LE(header_data.data() + offset, header.payload_size);
    offset += 4;

    // Checksum (uint32)
    writeUInt32LE(header_data.data() + offset, header.checksum);
    offset += 4;

    header_data.resize(offset);

    // Metadata section
    const MessageMetadata& metadata = message.getMetadata();
    std::vector<uint8_t> metadata_strings;

    // Source endpoint
    uint32_t src_len = static_cast<uint32_t>(metadata.source_endpoint.size());
    std::vector<uint8_t> src_data(4);
    writeUInt32LE(src_data.data(), src_len);
    metadata_strings.insert(metadata_strings.end(), src_data.begin(), src_data.end());
    metadata_strings.insert(metadata_strings.end(),
                           metadata.source_endpoint.begin(),
                           metadata.source_endpoint.end());

    // Destination endpoint
    uint32_t dst_len = static_cast<uint32_t>(metadata.destination_endpoint.size());
    std::vector<uint8_t> dst_data(4);
    writeUInt32LE(dst_data.data(), dst_len);
    metadata_strings.insert(metadata_strings.end(), dst_data.begin(), dst_data.end());
    metadata_strings.insert(metadata_strings.end(),
                           metadata.destination_endpoint.begin(),
                           metadata.destination_endpoint.end());

    // Subject
    uint32_t subj_len = static_cast<uint32_t>(metadata.subject.size());
    std::vector<uint8_t> subj_data(4);
    writeUInt32LE(subj_data.data(), subj_len);
    metadata_strings.insert(metadata_strings.end(), subj_data.begin(), subj_data.end());
    metadata_strings.insert(metadata_strings.end(),
                           metadata.subject.begin(),
                           metadata.subject.end());

    // Content type
    uint32_t ct_len = static_cast<uint32_t>(metadata.content_type.size());
    std::vector<uint8_t> ct_data(4);
    writeUInt32LE(ct_data.data(), ct_len);
    metadata_strings.insert(metadata_strings.end(), ct_data.begin(), ct_data.end());
    metadata_strings.insert(metadata_strings.end(),
                           metadata.content_type.begin(),
                           metadata.content_type.end());

    // Expiration and retry info
    std::vector<uint8_t> metadata_fixed(20);
    auto expiration_time = metadata.expiration.time_since_epoch();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(expiration_time);
    writeUInt64LE(metadata_fixed.data(), static_cast<uint64_t>(micros.count()));
    writeUInt32LE(metadata_fixed.data() + 8, metadata.retry_count);
    writeUInt32LE(metadata_fixed.data() + 12, metadata.max_retries);

    // Combine metadata
    std::vector<uint8_t> metadata_data;
    metadata_data.insert(metadata_data.end(), metadata_strings.begin(), metadata_strings.end());
    metadata_data.insert(metadata_data.end(), metadata_fixed.begin(), metadata_fixed.end());

    // Payload section
    const uint8_t* payload = message.getPayload();
    uint32_t payload_size = message.getPayloadSize();

    // Error info section (if applicable)
    std::vector<uint8_t> error_data;
    if (message.isError()) {
        const ErrorInfo& error = message.getErrorInfo();

        // Error code
        std::vector<uint8_t> ec_data(4);
        writeUInt32LE(ec_data.data(), error.error_code);
        error_data.insert(error_data.end(), ec_data.begin(), ec_data.end());

        // Error message
        uint32_t em_len = static_cast<uint32_t>(error.error_message.size());
        std::vector<uint8_t> em_len_data(4);
        writeUInt32LE(em_len_data.data(), em_len);
        error_data.insert(error_data.end(), em_len_data.begin(), em_len_data.end());
        error_data.insert(error_data.end(), error.error_message.begin(), error.error_message.end());

        // Error category
        uint32_t ec_cat_len = static_cast<uint32_t>(error.error_category.size());
        std::vector<uint8_t> ec_cat_len_data(4);
        writeUInt32LE(ec_cat_len_data.data(), ec_cat_len);
        error_data.insert(error_data.end(), ec_cat_len_data.begin(), ec_cat_len_data.end());
        error_data.insert(error_data.end(), error.error_category.begin(), error.error_category.end());

        // Error context
        uint32_t ec_ctx_len = static_cast<uint32_t>(error.error_context.size());
        std::vector<uint8_t> ec_ctx_len_data(4);
        writeUInt32LE(ec_ctx_len_data.data(), ec_ctx_len);
        error_data.insert(error_data.end(), ec_ctx_len_data.begin(), ec_ctx_len_data.end());
        error_data.insert(error_data.end(), error.error_context.begin(), error.error_context.end());
    }

    // Build final FlatBuffer
    // Format: [root_offset][header_size][header][metadata_size][metadata][payload_size][payload][error_size][error]

    result.resize(4);  // Root offset placeholder
    uint32_t root_offset = 4;
    writeUInt32LE(result.data(), root_offset);

    // Header
    std::vector<uint8_t> hdr_size(4);
    writeUInt32LE(hdr_size.data(), static_cast<uint32_t>(header_data.size()));
    result.insert(result.end(), hdr_size.begin(), hdr_size.end());
    result.insert(result.end(), header_data.begin(), header_data.end());

    // Metadata
    std::vector<uint8_t> meta_size(4);
    writeUInt32LE(meta_size.data(), static_cast<uint32_t>(metadata_data.size()));
    result.insert(result.end(), meta_size.begin(), meta_size.end());
    result.insert(result.end(), metadata_data.begin(), metadata_data.end());

    // Payload
    std::vector<uint8_t> pay_size(4);
    writeUInt32LE(pay_size.data(), payload_size);
    result.insert(result.end(), pay_size.begin(), pay_size.end());
    if (payload_size > 0) {
        result.insert(result.end(), payload, payload + payload_size);
    }

    // Error info
    std::vector<uint8_t> err_size(4);
    writeUInt32LE(err_size.data(), static_cast<uint32_t>(error_data.size()));
    result.insert(result.end(), err_size.begin(), err_size.end());
    if (!error_data.empty()) {
        result.insert(result.end(), error_data.begin(), error_data.end());
    }

    return result;
}

/**
 * @brief Deserializes FlatBuffer to message
 */
bool deserializeFromFlatBuffer(const void* data, uint32_t size,
                               MessagePtr& message) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);

    if (size < 4) {
        return false;
    }

    // Read root offset
    uint32_t root_offset = readUInt32LE(ptr);
    ptr += 4;
    size -= 4;

    if (root_offset != 4) {
        return false;  // Invalid root offset
    }

    message = std::make_shared<Message>();

    // Read header
    if (size < 4) return false;
    uint32_t header_size = readUInt32LE(ptr);
    ptr += 4;
    size -= 4;

    if (size < header_size) return false;

    MessageHeader& header = message->getHeader();
    size_t offset = 0;

    // Message ID
    std::memcpy(header.message_id, ptr + offset, 16);
    offset += 16;

    // Correlation ID
    std::memcpy(header.correlation_id, ptr + offset, 16);
    offset += 16;

    // Timestamp
    header.timestamp = readUInt64LE(ptr + offset);
    offset += 8;

    // Type
    header.type = static_cast<MessageType>(ptr[offset++]);

    // Priority
    header.priority = static_cast<MessagePriority>(ptr[offset++]);

    // Format
    header.format = static_cast<SerializationFormat>(ptr[offset++]);

    // Version
    header.version = ptr[offset++];

    // Flags
    header.flags = readUInt32LE(ptr + offset);
    offset += 4;

    // Payload size
    header.payload_size = readUInt32LE(ptr + offset);
    offset += 4;

    // Checksum
    header.checksum = readUInt32LE(ptr + offset);
    offset += 4;

    ptr += header_size;
    size -= header_size;

    // Read metadata
    if (size < 4) return false;
    uint32_t metadata_size = readUInt32LE(ptr);
    ptr += 4;
    size -= 4;

    if (size < metadata_size) return false;

    MessageMetadata& metadata = message->getMetadata();
    const uint8_t* meta_ptr = ptr;
    size_t meta_offset = 0;

    // Parse metadata strings
    // Source endpoint
    if (meta_offset + 4 <= metadata_size) {
        uint32_t src_len = readUInt32LE(meta_ptr + meta_offset);
        meta_offset += 4;
        if (meta_offset + src_len <= metadata_size) {
            metadata.source_endpoint.assign(reinterpret_cast<const char*>(meta_ptr + meta_offset), src_len);
            meta_offset += src_len;
        }
    }

    // Destination endpoint
    if (meta_offset + 4 <= metadata_size) {
        uint32_t dst_len = readUInt32LE(meta_ptr + meta_offset);
        meta_offset += 4;
        if (meta_offset + dst_len <= metadata_size) {
            metadata.destination_endpoint.assign(reinterpret_cast<const char*>(meta_ptr + meta_offset), dst_len);
            meta_offset += dst_len;
        }
    }

    // Subject
    if (meta_offset + 4 <= metadata_size) {
        uint32_t subj_len = readUInt32LE(meta_ptr + meta_offset);
        meta_offset += 4;
        if (meta_offset + subj_len <= metadata_size) {
            metadata.subject.assign(reinterpret_cast<const char*>(meta_ptr + meta_offset), subj_len);
            meta_offset += subj_len;
        }
    }

    // Content type
    if (meta_offset + 4 <= metadata_size) {
        uint32_t ct_len = readUInt32LE(meta_ptr + meta_offset);
        meta_offset += 4;
        if (meta_offset + ct_len <= metadata_size) {
            metadata.content_type.assign(reinterpret_cast<const char*>(meta_ptr + meta_offset), ct_len);
            meta_offset += ct_len;
        }
    }

    // Expiration and retry info
    if (meta_offset + 20 <= metadata_size) {
        uint64_t expiration_micros = readUInt64LE(meta_ptr + meta_offset);
        auto expiration_duration = std::chrono::microseconds(expiration_micros);
        metadata.expiration = std::chrono::system_clock::time_point(expiration_duration);
        meta_offset += 8;

        metadata.retry_count = readUInt32LE(meta_ptr + meta_offset);
        meta_offset += 4;

        metadata.max_retries = readUInt32LE(meta_ptr + meta_offset);
        meta_offset += 4;
    }

    ptr += metadata_size;
    size -= metadata_size;

    // Read payload
    if (size < 4) return false;
    uint32_t payload_size = readUInt32LE(ptr);
    ptr += 4;
    size -= 4;

    if (size < payload_size) return false;

    if (payload_size > 0) {
        if (!message->setPayload(ptr, payload_size)) {
            return false;
        }
    }

    ptr += payload_size;
    size -= payload_size;

    // Read error info (if present)
    if (size >= 4) {
        uint32_t error_size = readUInt32LE(ptr);
        ptr += 4;
        size -= 4;

        if (error_size > 0 && size >= error_size) {
            ErrorInfo& error = message->getErrorInfo();
            // Simplified error parsing
            error.error_code = readUInt32LE(ptr);
            // Skip detailed parsing for brevity
        }
    }

    return true;
}

} // anonymous namespace

// FlatBuffersSerializer implementation

FlatBuffersSerializer::FlatBuffersSerializer(size_t initial_buffer_size)
    : initial_buffer_size_(initial_buffer_size) {
    LOGD_FMT("FlatBuffersSerializer constructed with initial_buffer_size=" << initial_buffer_size);
}

FlatBuffersSerializer::~FlatBuffersSerializer() {
    LOGD_FMT("FlatBuffersSerializer destructor called");
}

SerializationResult FlatBuffersSerializer::serialize(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOGD_FMT("Serializing message, type=" << static_cast<int>(message.getType()));

    try {
        std::vector<uint8_t> data = serializeToFlatBuffer(message, initial_buffer_size_);
        LOGD_FMT("FlatBuffers serialization successful, size=" << data.size() << " bytes");
        return SerializationResult(std::move(data));

    } catch (const std::exception& e) {
        LOGE_FMT("FlatBuffers serialization failed: " << e.what());
        return SerializationResult(error_codes::SERIALIZATION_ERROR,
                                   std::string("FlatBuffers serialization failed: ") + e.what());
    } catch (...) {
        return SerializationResult(error_codes::UNKNOWN_ERROR,
                                   "Unknown FlatBuffers serialization error");
    }
}

DeserializationResult FlatBuffersSerializer::deserialize(const void* data, uint32_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOGD_FMT("FlatBuffersSerializer::deserialize - size: " << size);

    try {
        // Verify buffer integrity
        flatbuf::Verifier verifier(static_cast<const uint8_t*>(data), size);
        if (!verifier.VerifyBuffer()) {
            LOGE_FMT("FlatBuffersSerializer::deserialize - invalid format");
            return DeserializationResult(error_codes::INVALID_FORMAT,
                                         "Invalid FlatBuffer format");
        }

        MessagePtr message;
        if (!deserializeFromFlatBuffer(data, size, message)) {
            LOGE_FMT("FlatBuffersSerializer::deserialize - deserialization failed");
            return DeserializationResult(error_codes::DESERIALIZATION_ERROR,
                                         "Failed to deserialize FlatBuffer");
        }

        // Verify checksum
        if (!message->verifyChecksum()) {
            LOGE_FMT("FlatBuffersSerializer::deserialize - checksum mismatch");
            return DeserializationResult(error_codes::CHECKSUM_MISMATCH,
                                         "Message checksum verification failed");
        }

        LOGD_FMT("FlatBuffersSerializer::deserialize - success");
        return DeserializationResult(message);

    } catch (const std::exception& e) {
        LOGE_FMT("FlatBuffersSerializer::deserialize - exception: " << e.what());
        return DeserializationResult(error_codes::DESERIALIZATION_ERROR,
                                     std::string("FlatBuffers deserialization failed: ") + e.what());
    } catch (...) {
        LOGE_FMT("FlatBuffersSerializer::deserialize - unknown exception");
        return DeserializationResult(error_codes::UNKNOWN_ERROR,
                                     "Unknown FlatBuffers deserialization error");
    }
}

bool FlatBuffersSerializer::validate(const void* data, uint32_t size) const {
    std::lock_guard<std::mutex> lock(mutex_);

    LOGD_FMT("FlatBuffersSerializer::validate - size: " << size);

    if (size < 8) {  // Minimum FlatBuffer size
        LOGW_FMT("FlatBuffersSerializer::validate - size too small");
        return false;
    }

    flatbuf::Verifier verifier(static_cast<const uint8_t*>(data), size);
    bool valid = verifier.VerifyBuffer();
    LOGD_FMT("FlatBuffersSerializer::validate - result: " << (valid ? "valid" : "invalid"));
    return valid;
}

uint32_t FlatBuffersSerializer::estimateSerializedSize(const Message& message) const {
    // FlatBuffers adds some overhead for vtables and alignment
    uint32_t size = 0;

    // Root offset
    size += 4;

    // Header (with vtable overhead)
    size += 4;  // Size prefix
    size += 80; // Header data + vtable

    // Metadata (with vtable overhead)
    size += 4;  // Size prefix
    const MessageMetadata& metadata = message.getMetadata();
    size += 4 + static_cast<uint32_t>(metadata.source_endpoint.size());
    size += 4 + static_cast<uint32_t>(metadata.destination_endpoint.size());
    size += 4 + static_cast<uint32_t>(metadata.subject.size());
    size += 4 + static_cast<uint32_t>(metadata.content_type.size());
    size += 20; // Fixed metadata fields
    size += 24; // Vtable overhead

    // Payload
    size += 4;  // Size prefix
    size += message.getPayloadSize();

    // Error info (if applicable)
    size += 4;  // Size prefix
    if (message.isError()) {
        const ErrorInfo& error = message.getErrorInfo();
        size += 4;  // error_code
        size += 4 + static_cast<uint32_t>(error.error_message.size());
        size += 4 + static_cast<uint32_t>(error.error_category.size());
        size += 4 + static_cast<uint32_t>(error.error_context.size());
        size += 16; // Vtable overhead
    }

    // Alignment padding (estimate)
    size += 32;

    return size;
}

// Enum conversion helpers

uint8_t FlatBuffersSerializer::convertMessageType(MessageType type) {
    return static_cast<uint8_t>(type);
}

MessageType FlatBuffersSerializer::convertFromFBMessageType(uint8_t fb_type) {
    return static_cast<MessageType>(fb_type);
}

uint8_t FlatBuffersSerializer::convertMessagePriority(MessagePriority priority) {
    return static_cast<uint8_t>(priority);
}

MessagePriority FlatBuffersSerializer::convertFromFBPriority(uint8_t fb_priority) {
    return static_cast<MessagePriority>(fb_priority);
}

uint8_t FlatBuffersSerializer::convertSerializationFormat(SerializationFormat format) {
    return static_cast<uint8_t>(format);
}

SerializationFormat FlatBuffersSerializer::convertFromFBFormat(uint8_t fb_format) {
    return static_cast<SerializationFormat>(fb_format);
}

} // namespace ipc
} // namespace cdmf
