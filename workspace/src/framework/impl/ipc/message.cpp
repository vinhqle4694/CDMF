/**
 * @file message.cpp
 * @brief Message class implementation
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Message Types Agent
 */

#include "ipc/message.h"
#include "ipc/message_types.h"
#include "utils/log.h"
#include <cstring>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace cdmf {
namespace ipc {

// CRC32 lookup table for checksum computation
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// MessageHeader implementation

MessageHeader::MessageHeader()
    : timestamp(0)
    , type(MessageType::UNKNOWN)
    , priority(MessagePriority::NORMAL)
    , format(SerializationFormat::BINARY)
    , version(constants::PROTOCOL_VERSION)
    , flags(0)
    , payload_size(0)
    , checksum(0) {
    std::memset(message_id, 0, sizeof(message_id));
    std::memset(correlation_id, 0, sizeof(correlation_id));
}

bool MessageHeader::validate() const {
    // Check version
    if (version != constants::PROTOCOL_VERSION) {
        return false;
    }

    // Check payload size
    if (payload_size > constants::MAX_PAYLOAD_SIZE) {
        return false;
    }

    // Check message type
    if (type == MessageType::UNKNOWN) {
        return false;
    }

    return true;
}

bool MessageHeader::hasFlag(MessageFlags flag) const {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void MessageHeader::setFlag(MessageFlags flag) {
    flags |= static_cast<uint32_t>(flag);
}

void MessageHeader::clearFlag(MessageFlags flag) {
    flags &= ~static_cast<uint32_t>(flag);
}

// MessageMetadata implementation

MessageMetadata::MessageMetadata()
    : retry_count(0)
    , max_retries(3) {
}

bool MessageMetadata::isExpired() const {
    auto now = std::chrono::system_clock::now();
    return now > expiration;
}

// ErrorInfo implementation

ErrorInfo::ErrorInfo()
    : error_code(0) {
}

ErrorInfo::ErrorInfo(uint32_t code, const std::string& message)
    : error_code(code)
    , error_message(message) {
}

// Message implementation

Message::Message()
    : status_(MessageStatus::CREATED) {
    generateMessageId();
    updateTimestamp();
}

Message::Message(MessageType type)
    : status_(MessageStatus::CREATED) {
    header_.type = type;
    generateMessageId();
    updateTimestamp();
}

Message::Message(MessageType type, const void* payload, uint32_t size)
    : status_(MessageStatus::CREATED) {
    header_.type = type;
    generateMessageId();
    updateTimestamp();
    setPayload(payload, size);
}

Message::Message(const Message& other) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    header_ = other.header_;
    metadata_ = other.metadata_;
    payload_ = other.payload_;
    error_info_ = other.error_info_;
    status_ = other.status_;
}

Message::Message(Message&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.mutex_);
    header_ = other.header_;
    metadata_ = std::move(other.metadata_);
    payload_ = std::move(other.payload_);
    error_info_ = std::move(other.error_info_);
    status_ = other.status_;
}

Message& Message::operator=(const Message& other) {
    if (this != &other) {
        std::lock(mutex_, other.mutex_);
        std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(other.mutex_, std::adopt_lock);

        header_ = other.header_;
        metadata_ = other.metadata_;
        payload_ = other.payload_;
        error_info_ = other.error_info_;
        status_ = other.status_;
    }
    return *this;
}

Message& Message::operator=(Message&& other) noexcept {
    if (this != &other) {
        std::lock(mutex_, other.mutex_);
        std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(other.mutex_, std::adopt_lock);

        header_ = other.header_;
        metadata_ = std::move(other.metadata_);
        payload_ = std::move(other.payload_);
        error_info_ = std::move(other.error_info_);
        status_ = other.status_;
    }
    return *this;
}

Message::~Message() = default;

const MessageHeader& Message::getHeader() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_;
}

MessageHeader& Message::getHeader() {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_;
}

void Message::getMessageId(uint8_t* id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(id, header_.message_id, sizeof(header_.message_id));
}

void Message::setMessageId(const uint8_t* id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(header_.message_id, id, sizeof(header_.message_id));
}

void Message::generateMessageId() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Generate a UUID-like ID using random number generator
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t* id_ptr = reinterpret_cast<uint64_t*>(header_.message_id);
    id_ptr[0] = dis(gen);
    id_ptr[1] = dis(gen);
}

void Message::getCorrelationId(uint8_t* id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(id, header_.correlation_id, sizeof(header_.correlation_id));
}

void Message::setCorrelationId(const uint8_t* id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(header_.correlation_id, id, sizeof(header_.correlation_id));
}

uint64_t Message::getTimestamp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.timestamp;
}

void Message::setTimestamp(uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.timestamp = timestamp;
}

void Message::updateTimestamp() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    header_.timestamp = micros.count();
}

MessageType Message::getType() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.type;
}

void Message::setType(MessageType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.type = type;
}

MessagePriority Message::getPriority() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.priority;
}

void Message::setPriority(MessagePriority priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.priority = priority;
}

SerializationFormat Message::getFormat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.format;
}

void Message::setFormat(SerializationFormat format) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.format = format;
}

uint8_t Message::getVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.version;
}

bool Message::hasFlag(MessageFlags flag) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.hasFlag(flag);
}

void Message::setFlag(MessageFlags flag) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.setFlag(flag);
}

void Message::clearFlag(MessageFlags flag) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.clearFlag(flag);
}

uint32_t Message::getFlags() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.flags;
}

void Message::setFlags(uint32_t flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.flags = flags;
}

const uint8_t* Message::getPayload() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return payload_.empty() ? nullptr : payload_.data();
}

uint8_t* Message::getPayload() {
    std::lock_guard<std::mutex> lock(mutex_);
    return payload_.empty() ? nullptr : payload_.data();
}

uint32_t Message::getPayloadSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(payload_.size());
}

bool Message::setPayload(const void* data, uint32_t size) {
    if (size > constants::MAX_PAYLOAD_SIZE) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    payload_.resize(size);
    if (size > 0 && data != nullptr) {
        std::memcpy(payload_.data(), data, size);
    }
    header_.payload_size = size;
    return true;
}

bool Message::setPayload(std::vector<uint8_t>&& data) {
    if (data.size() > constants::MAX_PAYLOAD_SIZE) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    payload_ = std::move(data);
    header_.payload_size = static_cast<uint32_t>(payload_.size());
    return true;
}

void Message::clearPayload() {
    std::lock_guard<std::mutex> lock(mutex_);
    payload_.clear();
    header_.payload_size = 0;
    header_.checksum = 0;
}

bool Message::appendPayload(const void* data, uint32_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (payload_.size() + size > constants::MAX_PAYLOAD_SIZE) {
        return false;
    }

    size_t old_size = payload_.size();
    payload_.resize(old_size + size);
    if (size > 0 && data != nullptr) {
        std::memcpy(payload_.data() + old_size, data, size);
    }
    header_.payload_size = static_cast<uint32_t>(payload_.size());
    return true;
}

const MessageMetadata& Message::getMetadata() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_;
}

MessageMetadata& Message::getMetadata() {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_;
}

void Message::setSourceEndpoint(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.source_endpoint = endpoint;
}

std::string Message::getSourceEndpoint() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_.source_endpoint;
}

void Message::setDestinationEndpoint(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.destination_endpoint = endpoint;
}

std::string Message::getDestinationEndpoint() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_.destination_endpoint;
}

void Message::setSubject(const std::string& subject) {
    std::lock_guard<std::mutex> lock(mutex_);
    metadata_.subject = subject;
}

std::string Message::getSubject() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_.subject;
}

const ErrorInfo& Message::getErrorInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_info_;
}

ErrorInfo& Message::getErrorInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_info_;
}

void Message::setError(uint32_t code, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.type = MessageType::ERROR;
    error_info_.error_code = code;
    error_info_.error_message = message;
}

bool Message::isError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return header_.type == MessageType::ERROR;
}

bool Message::validate() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate header
    if (!header_.validate()) {
        return false;
    }

    // Check payload size matches header
    if (header_.payload_size != payload_.size()) {
        return false;
    }

    // Verify checksum if payload is not empty
    if (!payload_.empty() && header_.checksum != 0) {
        uint32_t computed = crc32(payload_.data(), static_cast<uint32_t>(payload_.size()));
        if (computed != header_.checksum) {
            return false;
        }
    }

    return true;
}

uint32_t Message::computeChecksum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (payload_.empty()) {
        return 0;
    }
    return crc32(payload_.data(), static_cast<uint32_t>(payload_.size()));
}

void Message::updateChecksum() {
    std::lock_guard<std::mutex> lock(mutex_);
    header_.checksum = payload_.empty() ? 0 :
        crc32(payload_.data(), static_cast<uint32_t>(payload_.size()));
    LOGD_FMT("Updated message checksum: " << header_.checksum
             << ", payload_size: " << payload_.size());
}

bool Message::verifyChecksum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (payload_.empty()) {
        bool valid = (header_.checksum == 0);
        LOGD_FMT("Verifying checksum for empty payload: " << (valid ? "PASS" : "FAIL")
                 << ", stored: " << header_.checksum);
        return valid;
    }
    uint32_t computed = crc32(payload_.data(), static_cast<uint32_t>(payload_.size()));
    bool valid = (computed == header_.checksum);
    if (!valid) {
        LOGE_FMT("Checksum verification FAILED - computed: " << computed
                 << ", stored: " << header_.checksum
                 << ", payload_size: " << payload_.size());
    } else {
        LOGD_FMT("Checksum verification PASSED - checksum: " << computed
                 << ", payload_size: " << payload_.size());
    }
    return valid;
}

MessageStatus Message::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void Message::setStatus(MessageStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}

uint32_t Message::getTotalSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sizeof(MessageHeader) + static_cast<uint32_t>(payload_.size());
}

bool Message::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return payload_.empty();
}

void Message::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    payload_.clear();
    header_.payload_size = 0;
    header_.checksum = 0;
    metadata_ = MessageMetadata();
    error_info_ = ErrorInfo();
    status_ = MessageStatus::CREATED;
}

Message Message::createResponse() const {
    Message response(MessageType::RESPONSE);

    std::lock_guard<std::mutex> lock(mutex_);

    // Set correlation ID to this message's ID
    std::memcpy(response.header_.correlation_id,
                header_.message_id,
                sizeof(header_.message_id));

    // Copy some metadata
    response.metadata_.destination_endpoint = metadata_.source_endpoint;
    response.metadata_.source_endpoint = metadata_.destination_endpoint;
    response.metadata_.subject = metadata_.subject;

    return response;
}

Message Message::createErrorResponse(uint32_t code, const std::string& message) const {
    Message response = createResponse();
    response.setError(code, message);
    return response;
}

std::string Message::toString() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;
    oss << "Message{";
    oss << "type=" << messageTypeToString(header_.type);
    oss << ", priority=" << static_cast<int>(header_.priority);
    oss << ", format=" << serializationFormatToString(header_.format);
    oss << ", payload_size=" << header_.payload_size;
    oss << ", timestamp=" << header_.timestamp;
    oss << ", status=" << messageStatusToString(status_);

    if (!metadata_.source_endpoint.empty()) {
        oss << ", source=" << metadata_.source_endpoint;
    }
    if (!metadata_.destination_endpoint.empty()) {
        oss << ", dest=" << metadata_.destination_endpoint;
    }
    if (!metadata_.subject.empty()) {
        oss << ", subject=" << metadata_.subject;
    }

    if (header_.type == MessageType::ERROR) {
        oss << ", error_code=" << error_info_.error_code;
        oss << ", error_msg=" << error_info_.error_message;
    }

    oss << "}";
    return oss.str();
}

uint32_t Message::crc32(const void* data, uint32_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < size; ++i) {
        uint8_t index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }

    return crc ^ 0xFFFFFFFF;
}

// Utility functions

const char* messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::REQUEST:   return "REQUEST";
        case MessageType::RESPONSE:  return "RESPONSE";
        case MessageType::EVENT:     return "EVENT";
        case MessageType::ERROR:     return "ERROR";
        case MessageType::HEARTBEAT: return "HEARTBEAT";
        case MessageType::CONTROL:   return "CONTROL";
        case MessageType::UNKNOWN:   return "UNKNOWN";
        default:                     return "INVALID";
    }
}

const char* messageStatusToString(MessageStatus status) {
    switch (status) {
        case MessageStatus::CREATED:            return "CREATED";
        case MessageStatus::QUEUED:             return "QUEUED";
        case MessageStatus::SENT:               return "SENT";
        case MessageStatus::DELIVERED:          return "DELIVERED";
        case MessageStatus::PROCESSED:          return "PROCESSED";
        case MessageStatus::SEND_FAILED:        return "SEND_FAILED";
        case MessageStatus::DELIVERY_FAILED:    return "DELIVERY_FAILED";
        case MessageStatus::PROCESSING_FAILED:  return "PROCESSING_FAILED";
        case MessageStatus::TIMEOUT:            return "TIMEOUT";
        case MessageStatus::REJECTED:           return "REJECTED";
        case MessageStatus::INVALID_FORMAT:     return "INVALID_FORMAT";
        case MessageStatus::SIZE_EXCEEDED:      return "SIZE_EXCEEDED";
        default:                                return "UNKNOWN";
    }
}

const char* serializationFormatToString(SerializationFormat format) {
    switch (format) {
        case SerializationFormat::BINARY:      return "BINARY";
        case SerializationFormat::JSON:        return "JSON";
        case SerializationFormat::PROTOBUF:    return "PROTOBUF";
        case SerializationFormat::MESSAGEPACK: return "MESSAGEPACK";
        case SerializationFormat::CUSTOM:      return "CUSTOM";
        default:                               return "UNKNOWN";
    }
}

} // namespace ipc
} // namespace cdmf
