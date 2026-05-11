#pragma once

#include <span>

#include "core/io/protocol_parser.hh"

namespace carrot::io {

class SimpleLineProtocolParser : public ProtocolParser {
public:
  auto TryParseMessage(std::span<const std::byte> data) -> ParseResult override;
  void ProcessMessage(std::span<const std::byte> message_data,
                      std::function<void(std::span<const std::byte>)> response_callback) override;
};

} // namespace carrot::io
