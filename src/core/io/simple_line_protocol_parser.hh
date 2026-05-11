#pragma once

#include <algorithm>
#include <span>

namespace carrot::io {

class SimpleLineProtocolParser {
public:
  struct ParseResult {
    bool found_message = false;
    size_t message_length = 0;
  };

  static auto TryParseMessage(std::span<std::byte> data) -> ParseResult {
    auto* begin = reinterpret_cast<const char*>(data.data());
    auto* end = begin + data.size();
    auto* newline = std::find(begin, end, '\n');

    if (newline != end) {
      return {.found_message = true,
              .message_length = static_cast<size_t>(std::distance(begin, newline)) + 1};
    }

    return {.found_message = false, .message_length = 0};
  }

  static void ProcessMessage(std::span<std::byte> message_data);
};

} // namespace carrot::io
