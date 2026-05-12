#pragma once

#include <cstddef>
#include <memory>
#include <span>

namespace carrot::io {

namespace {
// TODO: rename to CHUNK_SIZE.
constexpr size_t BUFFER_SIZE{4096};
} // namespace

class Chunk {
public:
  auto GetReadArea() -> std::span<std::byte>;
  auto GetWriteArea() -> std::span<std::byte>;
  void Fill(size_t size);
  void Consume(size_t size);

  [[nodiscard]] auto ReadableSize() const -> size_t { return filled_ - consumed_; }
  [[nodiscard]] auto WritableSize() const -> size_t { return BUFFER_SIZE - filled_; }
  [[nodiscard]] auto IsFullForReading() const -> bool { return filled_ == BUFFER_SIZE; }
  [[nodiscard]] auto HasReadableData() const -> bool { return ReadableSize() > 0; }

private:
  std::array<std::byte, BUFFER_SIZE> data_{};
  size_t filled_{0};
  size_t consumed_{0};
};

using ChunkPtr = std::unique_ptr<Chunk>;

} // namespace carrot::io
