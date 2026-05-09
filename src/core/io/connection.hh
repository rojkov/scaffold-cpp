#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <deque>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "carrot/event/dispatcher.hh"
#include "carrot/event/io_object.hh"
#include "core/io/chunk.hh"
#include "core/logging/log.hh"

namespace carrot::io {

class ReadBuffer : public event::IOObject {
public:
  explicit ReadBuffer(std::function<void(ReadBuffer*, int)> on_read_completed);

  auto GetWriteSpan() -> std::span<std::byte>;
  auto GetSpan() -> std::span<std::byte>;
  void Pullup(size_t size);
  void Drain(size_t size);
  [[nodiscard]] auto HasReadableData() const -> bool;
  [[nodiscard]] auto GetTotalReadableSize() const -> size_t;

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

private:
  void ConsumeFromChunks(size_t size);

  std::deque<ChunkPtr> chunks_;
  std::function<void(ReadBuffer*, int)> on_read_completed_;
  std::vector<std::byte> active_chunk_;
  size_t active_chunk_consumed_{0};
};

class WriteBuffer : public event::IOObject {
public:
  explicit WriteBuffer(std::function<void(WriteBuffer*, int)> on_write_completed);

  void Append(std::span<const std::byte> data);
  auto GetPendingWriteSpan() -> std::span<std::byte>;
  void Drain(size_t size);
  [[nodiscard]] auto HasPendingWrite() const -> bool;

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

private:
  std::deque<ChunkPtr> chunks_;
  std::function<void(WriteBuffer*, int)> on_write_completed_;
};

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

  static void ProcessMessage(std::span<std::byte> message_data) {
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
};

class Connection : public event::IOObject {
public:
  explicit Connection(int connection_fd, event::DispatcherSharedPtr dispatcher);

  void HandleCompletion(int res, uint32_t flags) override {}
  void ProcessCommand(event::Command cmd) override {}

  void Send(std::span<const std::byte> data);

private:
  void SubmitRead();
  void SubmitWrite();
  void OnReadCompleted(ReadBuffer* read_buffer, int res);
  void OnWriteCompleted(WriteBuffer* write_buffer, int res);
  void ParseMessages();

  int fd_;
  event::DispatcherSharedPtr dispatcher_;
  ReadBuffer read_buffer_;
  WriteBuffer write_buffer_;
};

} // namespace carrot::io
