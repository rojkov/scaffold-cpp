#pragma once

#include <deque>
#include <functional>

#include "carrot/event/io_object.hh"
#include "core/io/chunk.hh"

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

} // namespace carrot::io