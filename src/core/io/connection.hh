#pragma once

#include <cassert>
#include <cstddef>
#include <deque>
#include <functional>
#include <span>
#include <vector>

#include "carrot/event/dispatcher.hh"
#include "carrot/event/io_object.hh"
#include "core/io/chunk.hh"
#include "core/io/read_buffer.hh"

namespace carrot::io {

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
