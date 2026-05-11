#include "core/io/write_buffer.hh"

#include "core/logging/log.hh"

namespace carrot::io {

WriteBuffer::WriteBuffer(std::function<void(WriteBuffer*, int)> on_write_completed)
    : on_write_completed_{std::move(on_write_completed)} {}

void WriteBuffer::Append(std::span<const std::byte> data) {
  size_t remaining = data.size();
  size_t offset = 0;

  while (remaining > 0) {
    if (chunks_.empty() || chunks_.back()->IsFullForReading()) {
      chunks_.emplace_back(std::make_unique<Chunk>());
    }

    auto write_area = chunks_.back()->GetWriteArea();
    size_t write_size = std::min(write_area.size(), remaining);
    std::copy_n(data.begin() + offset, write_size, write_area.begin());
    chunks_.back()->Fill(write_size);
    offset += write_size;
    remaining -= write_size;
  }
}

auto WriteBuffer::GetPendingWriteSpan() -> std::span<std::byte> {
  if (chunks_.empty()) {
    return {};
  }
  return chunks_.front()->GetReadArea();
}

void WriteBuffer::Drain(size_t size) {
  size_t remaining = size;
  while (remaining > 0 && !chunks_.empty()) {
    size_t available = chunks_.front()->ReadableSize();
    size_t drain_size = std::min(available, remaining);
    chunks_.front()->Consume(drain_size);
    remaining -= drain_size;
    if (chunks_.front()->ReadableSize() == 0) {
      chunks_.pop_front();
    }
  }
}

auto WriteBuffer::HasPendingWrite() const -> bool {
  for (auto const& chunk : chunks_) {
    if (chunk->HasReadableData()) {
      return true;
    }
  }
  return false;
}

void WriteBuffer::HandleCompletion(int res, uint32_t flags) {
  LOG_DEBUG("Got write {} bytes. Flags: {}", res, flags);
  if (res > 0) {
    Drain(static_cast<size_t>(res));
  }
  on_write_completed_(this, res);
}

} // namespace carrot::io