#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "carrot/common/types.hh"

namespace carrot::common::wire {

enum class MessageType : uint8_t {
  kSubmitTask = 0x01,
  kChunk = 0x02,
  kComplete = 0x03,
  kError = 0x04,
  kRegister = 0x05,
  kHeartbeat = 0x06,
};

struct TlvHeader {
  MessageType type;
  uint32_t length;
};

auto encodeHeader(MessageType type, uint32_t payload_length) -> std::array<std::byte, 5>;

auto decodeHeader(std::span<const std::byte> data) -> TlvHeader;

auto serializeTask(const Task& task) -> std::vector<std::byte>;

auto deserializeTask(std::span<const std::byte> data) -> Task;

auto serializeChunk(uint64_t task_id, Chunk chunk, bool is_final) -> std::vector<std::byte>;

auto serializeComplete(uint64_t task_id) -> std::vector<std::byte>;

auto serializeError(uint64_t task_id, std::string_view message) -> std::vector<std::byte>;

} // namespace carrot::common::wire
