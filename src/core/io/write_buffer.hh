#pragma once

#include <deque>
#include <functional>

#include "carrot/event/io_object.hh"
#include "core/io/chunk.hh"

namespace carrot::io {

/**
 * WriteBuffer is used to write outgoing data to a file descriptor.
 */
class WriteBuffer : public event::IOObject {
public:
  explicit WriteBuffer(std::function<void(WriteBuffer*, int)> on_write_completed);

  /**
   * Append appends given input data to WriteBuffer
   * @param data is copied to the instance of WriteBuffer
   * TODO: is this method needed? Can we pre-allocate a chunk and give it to
   * a writer to fill in before submitting a write operation?
   */
  void Append(std::span<const std::byte> data);
  auto GetPendingWriteSpan() -> std::span<std::byte>;
  auto GetNewPreAllocatedSpan() -> std::span<std::byte>;
  void CommitWrite();
  void Drain(size_t size);
  [[nodiscard]] auto HasPendingWrite() const -> bool;

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

private:
  std::deque<ChunkPtr> chunks_;
  std::function<void(WriteBuffer*, int)> on_write_completed_;
  bool is_completion_pending_{false};
};

} // namespace carrot::io
