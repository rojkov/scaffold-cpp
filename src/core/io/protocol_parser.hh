#pragma once

#include <functional>
#include <span>

#include "carrot/common/pure.hh"
#include "core/io/read_buffer.hh"
#include "core/io/write_buffer.hh"

namespace carrot::io {

class ProtocolParser {
public:
  struct ParseResult {
    bool found_message = false;
    size_t message_length = 0;
  };

  ProtocolParser() = default;
  virtual ~ProtocolParser() = default;

  ProtocolParser(const ProtocolParser&) = delete;
  auto operator=(const ProtocolParser&) -> ProtocolParser& = delete;
  ProtocolParser(ProtocolParser&&) noexcept = delete;
  auto operator=(ProtocolParser&&) noexcept -> ProtocolParser& = delete;

  virtual auto TryParseMessage(std::span<const std::byte> data) -> ParseResult PURE;
  virtual void
  ProcessMessage(std::span<const std::byte> message_data,
                 std::function<void(std::span<const std::byte>)> response_callback) PURE;
  virtual auto Feed(ReadBuffer& input, WriteBuffer& output) -> bool PURE;
};

} // namespace carrot::io
