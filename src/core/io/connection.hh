#pragma once

#include <array>
#include <deque>
#include <functional>

#include "carrot/event/dispatcher.hh"
#include "carrot/event/io_object.hh"

namespace carrot::io {

namespace {
constexpr size_t BUFFER_SIZE{4096};
}

class Chunk {
public:
  auto GetReadArea() -> std::span<std::byte> {
    return {data_.data() + filled_, BUFFER_SIZE - filled_};
  }
  [[nodiscard]] auto IsFullForReading() const -> bool { return filled_ == BUFFER_SIZE; }
  [[nodiscard]] auto ReadableSize() const -> size_t { return filled_ - consumed_; }

private:
  std::array<std::byte, BUFFER_SIZE> data_{};
  size_t filled_{0};
  size_t consumed_{0};
};

using ChunkPtr = std::unique_ptr<Chunk>;

class ReadBuffer : public event::IOObject {
public:
  ReadBuffer(std::function<void(ReadBuffer*)> on_read_completed);
  auto GetSpan() -> std::span<std::byte>;
  void Pullup(size_t size);

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

private:
  std::deque<ChunkPtr> chunks_;
  std::function<void(ReadBuffer*)> on_read_completed_;
  std::vector<std::byte> active_chunk_;
  size_t active_chunk_consumed_{0};
};

class Connection : public event::IOObject {
public:
  explicit Connection(int connection_fd, event::DispatcherSharedPtr dispatcher);

  // IOObject interface
  void HandleCompletion(int res, uint32_t flags) override;
  void ProcessCommand(event::Command cmd) override {}

private:
  int fd_;
  event::DispatcherSharedPtr dispatcher_;
  std::array<std::byte, BUFFER_SIZE> buffer_{};
};

} // namespace carrot::io
