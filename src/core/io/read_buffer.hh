#pragma once

#include <deque>
#include <functional>
#include <vector>

#include "carrot/event/io_object.hh"
#include "core/io/chunk.hh"

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

} // namespace carrot::io