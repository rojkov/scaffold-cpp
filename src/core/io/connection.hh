#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "carrot/event/dispatcher.hh"
#include "carrot/event/io_object.hh"

namespace carrot::io {

namespace {
constexpr size_t BUFFER_SIZE{4096};
}

class Chunk {
public:
  auto GetReadArea() -> std::span<std::byte> {
    return {data_.data() + consumed_, filled_ - consumed_};
  }

  auto GetWriteArea() -> std::span<std::byte> {
    return {data_.data() + filled_, BUFFER_SIZE - filled_};
  }

  void Fill(size_t size) {
    assert(filled_ + size <= BUFFER_SIZE);
    filled_ += size;
  }

  [[nodiscard]] auto ReadableSize() const -> size_t { return filled_ - consumed_; }
  [[nodiscard]] auto WritableSize() const -> size_t { return BUFFER_SIZE - filled_; }
  [[nodiscard]] auto IsFullForReading() const -> bool { return filled_ == BUFFER_SIZE; }
  [[nodiscard]] auto HasReadableData() const -> bool { return ReadableSize() > 0; }

  void Consume(size_t size) {
    assert(ReadableSize() >= size);
    consumed_ += size;
  }

private:
  std::array<std::byte, BUFFER_SIZE> data_{};
  size_t filled_{0};
  size_t consumed_{0};
};

using ChunkPtr = std::unique_ptr<Chunk>;

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

  int fd_;
  event::DispatcherSharedPtr dispatcher_;
  ReadBuffer read_buffer_;
  WriteBuffer write_buffer_;
};

} // namespace carrot::io
