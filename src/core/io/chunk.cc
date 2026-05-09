#include "core/io/chunk.hh"

#include <cassert>

namespace carrot::io {

auto Chunk::GetReadArea() -> std::span<std::byte> {
  return std::span{data_}.subspan(consumed_, filled_ - consumed_);
}

auto Chunk::GetWriteArea() -> std::span<std::byte> {
  return std::span{data_}.subspan(filled_, BUFFER_SIZE - filled_);
}

void Chunk::Fill(size_t size) {
  assert(filled_ + size <= BUFFER_SIZE);
  filled_ += size;
}

void Chunk::Consume(size_t size) {
  assert(ReadableSize() >= size);
  consumed_ += size;
}

} // namespace carrot::io
