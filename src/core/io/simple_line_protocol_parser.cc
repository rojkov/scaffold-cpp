#include "core/io/simple_line_protocol_parser.hh"

#include <string>

#include "core/logging/log.hh"

namespace carrot::io {

void SimpleLineProtocolParser::ProcessMessage(std::span<std::byte> message_data) {
  auto* begin = reinterpret_cast<const char*>(message_data.data());
  size_t len = message_data.size();

  if (len > 0 && begin[len - 1] == '\n') {
    len--;
    if (len > 0 && begin[len - 1] == '\r') {
      len--;
    }
  }

  LOG_DEBUG("Received message ({} bytes): {}", len, std::string(begin, len));
}

} // namespace carrot::io
