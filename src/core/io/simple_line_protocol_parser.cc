#include "core/io/simple_line_protocol_parser.hh"

#include <algorithm>
#include <functional>
#include <string>

#include "core/logging/log.hh"

namespace carrot::io {

auto SimpleLineProtocolParser::TryParseMessage(std::span<const std::byte> data) -> ParseResult {
  auto* begin = reinterpret_cast<const char*>(data.data());
  auto* end = begin + data.size();
  auto* newline = std::find(begin, end, '\n');

  if (newline != end) {
    return {.found_message = true,
            .message_length = static_cast<size_t>(std::distance(begin, newline)) + 1};
  }

  return {.found_message = false, .message_length = 0};
}

void SimpleLineProtocolParser::ProcessMessage(
    std::span<const std::byte> message_data,
    std::function<void(std::span<const std::byte>)> response_callback) {
  auto* begin = reinterpret_cast<const char*>(message_data.data());
  size_t len = message_data.size();

  if (len > 0 && begin[len - 1] == '\n') {
    len--;
    if (len > 0 && begin[len - 1] == '\r') {
      len--;
    }
  }

  std::string message(begin, len);
  LOG_DEBUG("Received message ({} bytes): {}", len, message);

  std::string echo_response = "Echo: ";
  echo_response.append(message);
  echo_response.append("\n");

  auto response_bytes = std::as_bytes(std::span(echo_response.data(), echo_response.size()));
  response_callback(response_bytes);
}

} // namespace carrot::io
