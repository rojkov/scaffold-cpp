#pragma once

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <span>

#include "carrot/event/io_object.hh"
#include "llhttp.h"

namespace carrot::io {

class Chunk final {
public:
  auto Data() -> std::span<std::byte> { return data_; }

private:
  std::array<std::byte, 4096> data_;
};

using ChunkPtr = std::unique_ptr<Chunk>;

class LlhttpParser : public event::IOObject {
public:
  LlhttpParser(std::function<void(int res, uint32_t flags)>&& on_read_completion,
               std::function<void(std::span<const std::byte>)>&& on_request);
  virtual ~LlhttpParser();

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

  auto ReadBuffer() -> std::span<std::byte>;

private:
  static int on_body(llhttp_t* parser, const char* at, size_t length);
  static int on_message_complete(llhttp_t* parser);

  void Parse(size_t length);
  auto onBody(llhttp_t* parser, const char* at, size_t length) -> int;
  auto onMessageComplete(llhttp_t* parser) -> int;

  std::function<void(int res, uint32_t flags)> on_read_completion_;
  std::function<void(std::span<const std::byte>)> on_request_;

  llhttp_t parser_;
  llhttp_settings_t settings_;
  std::deque<ChunkPtr> chunks_;
};

} // namespace carrot::io
