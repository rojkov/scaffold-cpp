#pragma once

#include <deque>
#include <functional>
#include <vector>

#include "carrot/event/io_object.hh"
#include "core/io/chunk.hh"

namespace carrot::io {

/**
 * ReadBuffer is used to read incoming data from a file descriptor.
 */
class ReadBuffer : public event::IOObject {
public:
  explicit ReadBuffer(std::function<void(ReadBuffer*, int)> on_read_completed);

  /**
   * GetWriteSpan returns a contiguous buffer to write to from a file
   * descriptor for a read operation scheduled with Dispatcher::PrepareRead().
   */
  auto GetWriteSpan() -> std::span<std::byte>;

  /**
   * GetSpan returns a contiguous buffer to read from after a read
   * operation has completed.
   */
  auto GetSpan() -> std::span<std::byte>;

  /**
   * Pullup lazily linearizes the data contained in the ReadBuffer
   * @param size How much data to place into a contiguous memory area.
   */
  void Pullup(size_t size);

  /**
   * Drain discards a number of bytes from the front of the ReadBuffer,
   * effectively moving the buffer's read pointer forward.
   * @param size How many bytes to discard.
   */
  void Drain(size_t size);

  [[nodiscard]] auto HasReadableData() const -> bool;
  [[nodiscard]] auto GetTotalReadableSize() const -> size_t;

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

private:
  void consumeFromChunks(size_t size);

  std::deque<ChunkPtr> chunks_;
  std::function<void(ReadBuffer*, int)> on_read_completed_;
  std::vector<std::byte> active_chunk_;
  size_t active_chunk_consumed_{0};
};

} // namespace carrot::io
