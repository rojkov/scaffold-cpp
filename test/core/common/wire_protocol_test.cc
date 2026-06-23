#include "carrot/common/wire_protocol.hh"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace carrot::common::wire {
namespace {

TEST(WireProtocolTest, EncodeDecodeHeaderRoundtrip) {
  auto header = encodeHeader(MessageType::kSubmitTask, 12345);
  EXPECT_EQ(header.size(), 5);

  auto decoded = decodeHeader(header);
  EXPECT_EQ(decoded.type, MessageType::kSubmitTask);
  EXPECT_EQ(decoded.length, 12345u);
}

TEST(WireProtocolTest, EncodeDecodeAllMessageTypes) {
  auto test_type = [](MessageType type, uint32_t len) {
    auto header = encodeHeader(type, len);
    auto decoded = decodeHeader(header);
    EXPECT_EQ(decoded.type, type);
    EXPECT_EQ(decoded.length, len);
  };

  test_type(MessageType::kSubmitTask, 0);
  test_type(MessageType::kChunk, 1);
  test_type(MessageType::kComplete, 100);
  test_type(MessageType::kError, 65535);
  test_type(MessageType::kRegister, 100000);
  test_type(MessageType::kHeartbeat, 0xFFFFFFFF);
}

TEST(WireProtocolTest, SerializeDeserializeTask) {
  Task original;
  original.type = "echo";
  original.body = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
  original.function = "my_func";
  original.result_endpoint = "gw:8080";
  original.requirements = {{"cpu", 2}, {"mem", 512}};

  auto data = serializeTask(original);
  auto restored = deserializeTask(data);

  EXPECT_EQ(restored.type, original.type);
  EXPECT_EQ(restored.body, original.body);
  EXPECT_EQ(restored.function, original.function);
  EXPECT_EQ(restored.result_endpoint, original.result_endpoint);
  ASSERT_EQ(restored.requirements.size(), 2);
  EXPECT_EQ(restored.requirements[0].type, "cpu");
  EXPECT_EQ(restored.requirements[0].amount, 2u);
  EXPECT_EQ(restored.requirements[1].type, "mem");
  EXPECT_EQ(restored.requirements[1].amount, 512u);
}

TEST(WireProtocolTest, SerializeDeserializeTaskOptionalFields) {
  Task original;
  original.type = "ping";
  original.body = {std::byte{0x00}};

  auto data = serializeTask(original);
  auto restored = deserializeTask(data);

  EXPECT_EQ(restored.type, "ping");
  EXPECT_EQ(restored.body, original.body);
  EXPECT_FALSE(restored.function.has_value());
  EXPECT_FALSE(restored.result_endpoint.has_value());
  EXPECT_TRUE(restored.requirements.empty());
}

TEST(WireProtocolTest, SerializeChunkWithData) {
  Chunk chunk = {std::byte{0xAA}, std::byte{0xBB}};
  auto data = serializeChunk(42, chunk, false);

  auto header = encodeHeader(MessageType::kChunk, data.size());
  auto decoded_header = decodeHeader(header);
  EXPECT_EQ(decoded_header.type, MessageType::kChunk);
  EXPECT_EQ(decoded_header.length, data.size());
  EXPECT_FALSE(data.empty());
}

TEST(WireProtocolTest, SerializeComplete) {
  auto data = serializeComplete(99);
  EXPECT_FALSE(data.empty());

  auto header = encodeHeader(MessageType::kComplete, data.size());
  auto decoded = decodeHeader(header);
  EXPECT_EQ(decoded.type, MessageType::kComplete);
}

TEST(WireProtocolTest, SerializeError) {
  auto data = serializeError(7, "something went wrong");
  EXPECT_FALSE(data.empty());

  auto header = encodeHeader(MessageType::kError, data.size());
  auto decoded = decodeHeader(header);
  EXPECT_EQ(decoded.type, MessageType::kError);
}

} // namespace
} // namespace carrot::common::wire
