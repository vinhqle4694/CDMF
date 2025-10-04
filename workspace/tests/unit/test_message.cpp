/**
 * @file test_message.cpp
 * @brief Unit tests for Message and Serialization functionality
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Message Types Agent
 */

#include "ipc/message.h"
#include "ipc/message_types.h"
#include "ipc/serializer.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

using namespace cdmf::ipc;

// Test fixture for Message tests
class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// MessageHeader Tests

TEST_F(MessageTest, MessageHeaderDefaultConstructor) {
    MessageHeader header;

    EXPECT_EQ(header.timestamp, 0u);
    EXPECT_EQ(header.type, MessageType::UNKNOWN);
    EXPECT_EQ(header.priority, MessagePriority::NORMAL);
    EXPECT_EQ(header.format, SerializationFormat::BINARY);
    EXPECT_EQ(header.version, constants::PROTOCOL_VERSION);
    EXPECT_EQ(header.flags, 0u);
    EXPECT_EQ(header.payload_size, 0u);
    EXPECT_EQ(header.checksum, 0u);
}

TEST_F(MessageTest, MessageHeaderValidation) {
    MessageHeader header;
    header.type = MessageType::REQUEST;
    EXPECT_TRUE(header.validate());

    header.type = MessageType::UNKNOWN;
    EXPECT_FALSE(header.validate());

    header.type = MessageType::RESPONSE;
    header.payload_size = constants::MAX_PAYLOAD_SIZE + 1;
    EXPECT_FALSE(header.validate());
}

TEST_F(MessageTest, MessageHeaderFlags) {
    MessageHeader header;

    EXPECT_FALSE(header.hasFlag(MessageFlags::REQUIRE_ACK));
    header.setFlag(MessageFlags::REQUIRE_ACK);
    EXPECT_TRUE(header.hasFlag(MessageFlags::REQUIRE_ACK));

    header.setFlag(MessageFlags::COMPRESSED);
    EXPECT_TRUE(header.hasFlag(MessageFlags::COMPRESSED));
    EXPECT_TRUE(header.hasFlag(MessageFlags::REQUIRE_ACK));

    header.clearFlag(MessageFlags::REQUIRE_ACK);
    EXPECT_FALSE(header.hasFlag(MessageFlags::REQUIRE_ACK));
    EXPECT_TRUE(header.hasFlag(MessageFlags::COMPRESSED));
}

// Message Tests

TEST_F(MessageTest, MessageDefaultConstructor) {
    Message msg;

    EXPECT_EQ(msg.getType(), MessageType::UNKNOWN);
    EXPECT_EQ(msg.getPriority(), MessagePriority::NORMAL);
    EXPECT_EQ(msg.getFormat(), SerializationFormat::BINARY);
    EXPECT_EQ(msg.getPayloadSize(), 0u);
    EXPECT_TRUE(msg.isEmpty());
    EXPECT_EQ(msg.getStatus(), MessageStatus::CREATED);
}

TEST_F(MessageTest, MessageTypeConstructor) {
    Message msg(MessageType::REQUEST);

    EXPECT_EQ(msg.getType(), MessageType::REQUEST);
    EXPECT_GT(msg.getTimestamp(), 0u);
}

TEST_F(MessageTest, MessagePayloadConstructor) {
    const char* data = "Hello, World!";
    uint32_t size = static_cast<uint32_t>(strlen(data));

    Message msg(MessageType::REQUEST, data, size);

    EXPECT_EQ(msg.getType(), MessageType::REQUEST);
    EXPECT_EQ(msg.getPayloadSize(), size);
    EXPECT_FALSE(msg.isEmpty());

    const uint8_t* payload = msg.getPayload();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(memcmp(payload, data, size), 0);
}

TEST_F(MessageTest, MessageCopyConstructor) {
    Message msg1(MessageType::EVENT);
    msg1.setSubject("test.event");
    msg1.setPayload("data", 4);

    Message msg2(msg1);

    EXPECT_EQ(msg2.getType(), MessageType::EVENT);
    EXPECT_EQ(msg2.getSubject(), "test.event");
    EXPECT_EQ(msg2.getPayloadSize(), 4u);
}

TEST_F(MessageTest, MessageMoveConstructor) {
    Message msg1(MessageType::EVENT);
    msg1.setPayload("data", 4);

    Message msg2(std::move(msg1));

    EXPECT_EQ(msg2.getType(), MessageType::EVENT);
    EXPECT_EQ(msg2.getPayloadSize(), 4u);
}

TEST_F(MessageTest, MessageIdGeneration) {
    Message msg;
    uint8_t id1[16];
    msg.getMessageId(id1);

    msg.generateMessageId();
    uint8_t id2[16];
    msg.getMessageId(id2);

    // IDs should be different
    EXPECT_NE(memcmp(id1, id2, 16), 0);
}

TEST_F(MessageTest, MessageIdSetGet) {
    Message msg;
    uint8_t id[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    msg.setMessageId(id);

    uint8_t retrieved_id[16];
    msg.getMessageId(retrieved_id);

    EXPECT_EQ(memcmp(id, retrieved_id, 16), 0);
}

TEST_F(MessageTest, CorrelationId) {
    Message msg;
    uint8_t corr_id[16] = {16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

    msg.setCorrelationId(corr_id);

    uint8_t retrieved_id[16];
    msg.getCorrelationId(retrieved_id);

    EXPECT_EQ(memcmp(corr_id, retrieved_id, 16), 0);
}

TEST_F(MessageTest, Timestamp) {
    Message msg;
    uint64_t ts1 = msg.getTimestamp();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    msg.updateTimestamp();

    uint64_t ts2 = msg.getTimestamp();
    EXPECT_GT(ts2, ts1);
}

TEST_F(MessageTest, MessageType) {
    Message msg;

    msg.setType(MessageType::REQUEST);
    EXPECT_EQ(msg.getType(), MessageType::REQUEST);

    msg.setType(MessageType::RESPONSE);
    EXPECT_EQ(msg.getType(), MessageType::RESPONSE);
}

TEST_F(MessageTest, MessagePriority) {
    Message msg;

    msg.setPriority(MessagePriority::HIGH);
    EXPECT_EQ(msg.getPriority(), MessagePriority::HIGH);

    msg.setPriority(MessagePriority::CRITICAL);
    EXPECT_EQ(msg.getPriority(), MessagePriority::CRITICAL);
}

TEST_F(MessageTest, MessageFlags) {
    Message msg;

    EXPECT_FALSE(msg.hasFlag(MessageFlags::REQUIRE_ACK));

    msg.setFlag(MessageFlags::REQUIRE_ACK);
    EXPECT_TRUE(msg.hasFlag(MessageFlags::REQUIRE_ACK));

    msg.setFlag(MessageFlags::ENCRYPTED);
    EXPECT_TRUE(msg.hasFlag(MessageFlags::ENCRYPTED));

    msg.clearFlag(MessageFlags::REQUIRE_ACK);
    EXPECT_FALSE(msg.hasFlag(MessageFlags::REQUIRE_ACK));
    EXPECT_TRUE(msg.hasFlag(MessageFlags::ENCRYPTED));
}

TEST_F(MessageTest, PayloadSetGet) {
    Message msg;
    const char* data = "Test payload data";
    uint32_t size = static_cast<uint32_t>(strlen(data));

    EXPECT_TRUE(msg.setPayload(data, size));
    EXPECT_EQ(msg.getPayloadSize(), size);

    const uint8_t* payload = msg.getPayload();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(memcmp(payload, data, size), 0);
}

TEST_F(MessageTest, PayloadSizeLimit) {
    Message msg;
    std::vector<uint8_t> large_payload(constants::MAX_PAYLOAD_SIZE + 1, 0xAB);

    EXPECT_FALSE(msg.setPayload(large_payload.data(),
                                  static_cast<uint32_t>(large_payload.size())));

    std::vector<uint8_t> valid_payload(1024, 0xCD);
    EXPECT_TRUE(msg.setPayload(valid_payload.data(),
                                 static_cast<uint32_t>(valid_payload.size())));
}

TEST_F(MessageTest, PayloadMove) {
    Message msg;
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};

    EXPECT_TRUE(msg.setPayload(std::move(data)));
    EXPECT_EQ(msg.getPayloadSize(), 5u);

    const uint8_t* payload = msg.getPayload();
    EXPECT_EQ(payload[0], 1);
    EXPECT_EQ(payload[4], 5);
}

TEST_F(MessageTest, PayloadClear) {
    Message msg;
    msg.setPayload("data", 4);

    EXPECT_FALSE(msg.isEmpty());

    msg.clearPayload();
    EXPECT_TRUE(msg.isEmpty());
    EXPECT_EQ(msg.getPayloadSize(), 0u);
}

TEST_F(MessageTest, PayloadAppend) {
    Message msg;

    EXPECT_TRUE(msg.appendPayload("Hello", 5));
    EXPECT_TRUE(msg.appendPayload(" ", 1));
    EXPECT_TRUE(msg.appendPayload("World", 5));

    EXPECT_EQ(msg.getPayloadSize(), 11u);

    const uint8_t* payload = msg.getPayload();
    std::string result(reinterpret_cast<const char*>(payload), 11);
    EXPECT_EQ(result, "Hello World");
}

TEST_F(MessageTest, Metadata) {
    Message msg;

    msg.setSourceEndpoint("client-001");
    msg.setDestinationEndpoint("server-001");
    msg.setSubject("test.message");

    EXPECT_EQ(msg.getSourceEndpoint(), "client-001");
    EXPECT_EQ(msg.getDestinationEndpoint(), "server-001");
    EXPECT_EQ(msg.getSubject(), "test.message");
}

TEST_F(MessageTest, ErrorInfo) {
    Message msg;

    EXPECT_FALSE(msg.isError());

    msg.setError(404, "Not Found");

    EXPECT_TRUE(msg.isError());
    EXPECT_EQ(msg.getType(), MessageType::ERROR);

    const ErrorInfo& error = msg.getErrorInfo();
    EXPECT_EQ(error.error_code, 404u);
    EXPECT_EQ(error.error_message, "Not Found");
}

TEST_F(MessageTest, Checksum) {
    Message msg;
    msg.setPayload("test data", 9);

    uint32_t checksum1 = msg.computeChecksum();
    EXPECT_NE(checksum1, 0u);

    msg.updateChecksum();
    EXPECT_TRUE(msg.verifyChecksum());

    // Modify payload
    uint8_t* payload = msg.getPayload();
    payload[0] = 'X';

    EXPECT_FALSE(msg.verifyChecksum());
}

TEST_F(MessageTest, Validation) {
    Message msg(MessageType::REQUEST);
    msg.setPayload("valid data", 10);
    msg.updateChecksum();

    EXPECT_TRUE(msg.validate());

    // Invalid type
    msg.setType(MessageType::UNKNOWN);
    EXPECT_FALSE(msg.validate());
}

TEST_F(MessageTest, Status) {
    Message msg;

    EXPECT_EQ(msg.getStatus(), MessageStatus::CREATED);

    msg.setStatus(MessageStatus::QUEUED);
    EXPECT_EQ(msg.getStatus(), MessageStatus::QUEUED);

    msg.setStatus(MessageStatus::SENT);
    EXPECT_EQ(msg.getStatus(), MessageStatus::SENT);
}

TEST_F(MessageTest, TotalSize) {
    Message msg;
    msg.setPayload("test", 4);

    uint32_t total = msg.getTotalSize();
    EXPECT_EQ(total, sizeof(MessageHeader) + 4);
}

TEST_F(MessageTest, Clear) {
    Message msg(MessageType::REQUEST);
    msg.setPayload("data", 4);
    msg.setSubject("test");

    msg.clear();

    EXPECT_TRUE(msg.isEmpty());
    EXPECT_EQ(msg.getPayloadSize(), 0u);
    EXPECT_EQ(msg.getStatus(), MessageStatus::CREATED);
}

TEST_F(MessageTest, CreateResponse) {
    Message request(MessageType::REQUEST);
    request.setSourceEndpoint("client");
    request.setDestinationEndpoint("server");
    request.setSubject("query");

    Message response = request.createResponse();

    EXPECT_EQ(response.getType(), MessageType::RESPONSE);
    EXPECT_EQ(response.getSourceEndpoint(), "server");
    EXPECT_EQ(response.getDestinationEndpoint(), "client");
    EXPECT_EQ(response.getSubject(), "query");

    // Check correlation ID matches request ID
    uint8_t req_id[16], corr_id[16];
    request.getMessageId(req_id);
    response.getCorrelationId(corr_id);
    EXPECT_EQ(memcmp(req_id, corr_id, 16), 0);
}

TEST_F(MessageTest, CreateErrorResponse) {
    Message request(MessageType::REQUEST);

    Message error = request.createErrorResponse(500, "Internal Error");

    EXPECT_EQ(error.getType(), MessageType::ERROR);
    EXPECT_TRUE(error.isError());

    const ErrorInfo& info = error.getErrorInfo();
    EXPECT_EQ(info.error_code, 500u);
    EXPECT_EQ(info.error_message, "Internal Error");
}

TEST_F(MessageTest, ToString) {
    Message msg(MessageType::REQUEST);
    msg.setSubject("test");

    std::string str = msg.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("REQUEST"), std::string::npos);
}

// Thread Safety Tests

TEST_F(MessageTest, ThreadSafetyReadWrite) {
    Message msg;
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Writer thread
    threads.emplace_back([&msg, &stop]() {
        for (int i = 0; i < 100 && !stop; ++i) {
            msg.setPayload("data", 4);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Reader threads
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([&msg, &stop]() {
            for (int i = 0; i < 100 && !stop; ++i) {
                msg.getPayloadSize();
                msg.getType();
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    stop = true;
}

// Serialization Tests

class SerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer = std::make_shared<BinarySerializer>();
    }

    SerializerPtr serializer;
};

TEST_F(SerializerTest, BinarySerializerProperties) {
    EXPECT_EQ(serializer->getFormat(), SerializationFormat::BINARY);
    EXPECT_EQ(serializer->getName(), "Binary");
    EXPECT_FALSE(serializer->getVersion().empty());
}

TEST_F(SerializerTest, SerializeDeserializeSimpleMessage) {
    Message msg(MessageType::REQUEST);
    msg.setPayload("Hello", 5);
    msg.updateChecksum();

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());

    auto deser_result = serializer->deserialize(result.data);
    ASSERT_TRUE(deser_result.success);
    ASSERT_NE(deser_result.message, nullptr);

    EXPECT_EQ(deser_result.message->getType(), MessageType::REQUEST);
    EXPECT_EQ(deser_result.message->getPayloadSize(), 5u);

    const uint8_t* payload = deser_result.message->getPayload();
    EXPECT_EQ(memcmp(payload, "Hello", 5), 0);
}

TEST_F(SerializerTest, SerializeDeserializeWithMetadata) {
    Message msg(MessageType::EVENT);
    msg.setSourceEndpoint("client-001");
    msg.setDestinationEndpoint("server-001");
    msg.setSubject("user.login");
    msg.setPayload("user_data", 9);
    msg.updateChecksum();

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);

    auto deser_result = serializer->deserialize(result.data);
    ASSERT_TRUE(deser_result.success);

    EXPECT_EQ(deser_result.message->getSourceEndpoint(), "client-001");
    EXPECT_EQ(deser_result.message->getDestinationEndpoint(), "server-001");
    EXPECT_EQ(deser_result.message->getSubject(), "user.login");
}

TEST_F(SerializerTest, SerializeDeserializeErrorMessage) {
    Message msg;
    msg.setError(404, "Resource not found");
    msg.getErrorInfo().error_category = "HTTP";
    msg.getErrorInfo().error_context = "GET /api/users/123";

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);

    auto deser_result = serializer->deserialize(result.data);
    ASSERT_TRUE(deser_result.success);

    EXPECT_TRUE(deser_result.message->isError());

    const ErrorInfo& error = deser_result.message->getErrorInfo();
    EXPECT_EQ(error.error_code, 404u);
    EXPECT_EQ(error.error_message, "Resource not found");
    EXPECT_EQ(error.error_category, "HTTP");
    EXPECT_EQ(error.error_context, "GET /api/users/123");
}

TEST_F(SerializerTest, SerializeDeserializeLargePayload) {
    std::vector<uint8_t> large_data(100000, 0xAB);

    Message msg(MessageType::REQUEST);
    ASSERT_TRUE(msg.setPayload(large_data.data(),
                                 static_cast<uint32_t>(large_data.size())));
    msg.updateChecksum();

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);

    auto deser_result = serializer->deserialize(result.data);
    ASSERT_TRUE(deser_result.success);

    EXPECT_EQ(deser_result.message->getPayloadSize(), 100000u);

    const uint8_t* payload = deser_result.message->getPayload();
    EXPECT_EQ(payload[0], 0xAB);
    EXPECT_EQ(payload[99999], 0xAB);
}

TEST_F(SerializerTest, SerializeEmptyMessage) {
    Message msg(MessageType::HEARTBEAT);

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);

    auto deser_result = serializer->deserialize(result.data);
    ASSERT_TRUE(deser_result.success);

    EXPECT_EQ(deser_result.message->getType(), MessageType::HEARTBEAT);
    EXPECT_TRUE(deser_result.message->isEmpty());
}

TEST_F(SerializerTest, DeserializeInvalidData) {
    uint8_t invalid_data[] = {0x00, 0x01, 0x02, 0x03};

    auto result = serializer->deserialize(invalid_data, sizeof(invalid_data));
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_code, 0u);
}

TEST_F(SerializerTest, ValidateData) {
    Message msg(MessageType::REQUEST);
    msg.setPayload("test", 4);
    msg.updateChecksum();

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);

    EXPECT_TRUE(serializer->validate(result.data.data(),
                                       static_cast<uint32_t>(result.data.size())));

    // Invalid data
    uint8_t bad_data[] = {0xFF, 0xFF, 0xFF};
    EXPECT_FALSE(serializer->validate(bad_data, sizeof(bad_data)));
}

TEST_F(SerializerTest, EstimateSerializedSize) {
    Message msg(MessageType::REQUEST);
    msg.setPayload("data", 4);

    uint32_t estimated = serializer->estimateSerializedSize(msg);
    EXPECT_GT(estimated, 0u);

    auto result = serializer->serialize(msg);
    ASSERT_TRUE(result.success);

    // Estimate should be close to actual size (within reasonable margin)
    EXPECT_LE(result.data.size(), estimated + 100);
}

// SerializerFactory Tests

TEST(SerializerFactoryTest, CreateBinarySerializer) {
    auto serializer = SerializerFactory::createSerializer(SerializationFormat::BINARY);
    ASSERT_NE(serializer, nullptr);
    EXPECT_EQ(serializer->getFormat(), SerializationFormat::BINARY);
}

TEST(SerializerFactoryTest, CreateUnsupportedSerializer) {
    auto serializer = SerializerFactory::createSerializer(SerializationFormat::JSON);
    EXPECT_EQ(serializer, nullptr);
}

TEST(SerializerFactoryTest, GetDefaultSerializer) {
    auto serializer = SerializerFactory::getDefaultSerializer();
    ASSERT_NE(serializer, nullptr);
    EXPECT_EQ(serializer->getFormat(), SerializationFormat::BINARY);
}

TEST(SerializerFactoryTest, IsFormatSupported) {
    EXPECT_TRUE(SerializerFactory::isFormatSupported(SerializationFormat::BINARY));
    EXPECT_FALSE(SerializerFactory::isFormatSupported(SerializationFormat::JSON));
    EXPECT_TRUE(SerializerFactory::isFormatSupported(SerializationFormat::PROTOBUF));
}

TEST(SerializerFactoryTest, GetSupportedFormats) {
    auto formats = SerializerFactory::getSupportedFormats();
    EXPECT_FALSE(formats.empty());
    EXPECT_EQ(formats.size(), 2u);
    EXPECT_EQ(formats[0], SerializationFormat::BINARY);
    EXPECT_EQ(formats[1], SerializationFormat::PROTOBUF);
}

// Utility Function Tests

TEST(UtilityTest, MessageTypeToString) {
    EXPECT_STREQ(messageTypeToString(MessageType::REQUEST), "REQUEST");
    EXPECT_STREQ(messageTypeToString(MessageType::RESPONSE), "RESPONSE");
    EXPECT_STREQ(messageTypeToString(MessageType::EVENT), "EVENT");
    EXPECT_STREQ(messageTypeToString(MessageType::ERROR), "ERROR");
    EXPECT_STREQ(messageTypeToString(MessageType::HEARTBEAT), "HEARTBEAT");
    EXPECT_STREQ(messageTypeToString(MessageType::CONTROL), "CONTROL");
    EXPECT_STREQ(messageTypeToString(MessageType::UNKNOWN), "UNKNOWN");
}

TEST(UtilityTest, MessageStatusToString) {
    EXPECT_STREQ(messageStatusToString(MessageStatus::CREATED), "CREATED");
    EXPECT_STREQ(messageStatusToString(MessageStatus::SENT), "SENT");
    EXPECT_STREQ(messageStatusToString(MessageStatus::DELIVERED), "DELIVERED");
}

TEST(UtilityTest, SerializationFormatToString) {
    EXPECT_STREQ(serializationFormatToString(SerializationFormat::BINARY), "BINARY");
    EXPECT_STREQ(serializationFormatToString(SerializationFormat::JSON), "JSON");
    EXPECT_STREQ(serializationFormatToString(SerializationFormat::PROTOBUF), "PROTOBUF");
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
