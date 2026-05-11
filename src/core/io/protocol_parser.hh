#pragma once

#include <cstddef>
#include <functional>
#include <span>

namespace carrot::io {

class ProtocolParser {
public:
  struct ParseResult {
    bool found_message = false;
    size_t message_length = 0;
  };

  virtual ~ProtocolParser() = default;

  virtual auto TryParseMessage(std::span<const std::byte> data) -> ParseResult = 0;
  virtual void
  ProcessMessage(std::span<const std::byte> message_data,
                 std::function<void(std::span<const std::byte>)> response_callback) = 0;
};

} // namespace carrot::io
