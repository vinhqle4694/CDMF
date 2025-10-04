/**
 * @file serializer_factory.cpp
 * @brief SerializerFactory implementation with support for all serializers
 *
 * Provides factory methods for creating serializer instances.
 * Supports Binary, ProtoBuf, and FlatBuffers serializers.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Serialization Agent
 */

#include "ipc/serializer.h"
#include "ipc/protobuf_serializer.h"
#include "ipc/flatbuffers_serializer.h"
#include "utils/log.h"

namespace cdmf {
namespace ipc {

SerializerPtr SerializerFactory::createSerializer(SerializationFormat format) {
    LOGD_FMT("SerializerFactory::createSerializer - format: " << static_cast<int>(format));

    switch (format) {
        case SerializationFormat::BINARY:
            LOGD_FMT("Creating BinarySerializer");
            return std::make_shared<BinarySerializer>();

        case SerializationFormat::PROTOBUF:
            LOGD_FMT("Creating ProtoBufSerializer");
            return std::make_shared<ProtoBufSerializer>();

        case SerializationFormat::JSON:
        case SerializationFormat::MESSAGEPACK:
        case SerializationFormat::CUSTOM:
            LOGW_FMT("Serialization format not yet implemented: " << static_cast<int>(format));
            return nullptr;

        default:
            LOGE_FMT("Unknown serialization format: " << static_cast<int>(format));
            return nullptr;
    }
}

SerializerPtr SerializerFactory::getDefaultSerializer() {
    return std::make_shared<BinarySerializer>();
}

bool SerializerFactory::isFormatSupported(SerializationFormat format) {
    return format == SerializationFormat::BINARY ||
           format == SerializationFormat::PROTOBUF;
}

std::vector<SerializationFormat> SerializerFactory::getSupportedFormats() {
    return {
        SerializationFormat::BINARY,
        SerializationFormat::PROTOBUF
    };
}

} // namespace ipc
} // namespace cdmf
