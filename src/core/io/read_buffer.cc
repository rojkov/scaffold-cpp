#include "core/io/read_buffer.hh"

#include "core/logging/log.hh"

namespace carrot::io {

ReadBuffer::ReadBuffer(std::function<void(ReadBuffer*, int)> on_read_completed)
    : on_read_completed_{std::move(on_read_completed)} {}

auto ReadBuffer::GetWriteSpan() -> std::span<std::byte> {
  if (chunks_.empty() || chunks_.back()->IsFullForReading()) {
    chunks_.emplace_back(std::make_unique<Chunk>());
  }

  return chunks_.back()->GetWriteArea();
}

auto ReadBuffer::GetSpan() -> std::span<std::byte> {
  if (active_chunk_consumed_ < active_chunk_.size()) {
    return {active_chunk_.data() + active_chunk_consumed_,
            active_chunk_.size() - active_chunk_consumed_};
  }

  if (chunks_.empty()) {
    return {};
  }

  return chunks_.front()->GetReadArea();
}

void ReadBuffer::Pullup(size_t size) {
  if (GetTotalReadableSize() < size) {
    return;
  }

  if (active_chunk_consumed_ < active_chunk_.size()) {
    if (active_chunk_.size() - active_chunk_consumed_ >= size) {
      return;
    }
  } else if (!chunks_.empty() && chunks_.front()->ReadableSize() >= size) {
    return;
  }

  std::vector<std::byte> new_chunk;
  new_chunk.reserve(size);
  size_t remaining = size;

  if (active_chunk_consumed_ < active_chunk_.size()) {
    size_t existing = active_chunk_.size() - active_chunk_consumed_;
    // TODO: isn't `existing` guaranteed to be less than `remaining`?
    assert(existing < remaining);
    size_t copy_size = std::min(existing, remaining);
    std::copy_n(active_chunk_.begin() + active_chunk_consumed_, copy_size,
                std::back_inserter(new_chunk));
    remaining -= copy_size;
  }

  for (auto const& chunk : chunks_) {
    if (remaining == 0) {
      break;
    }

    // TODO: can we release memory owned by the Chunks and destroy the
    // corresponding Chunk instances here, at the moment of Pullup?
    auto read_area = chunk->GetReadArea();
    size_t copy_size = std::min(read_area.size(), remaining);
    std::copy_n(read_area.begin(), copy_size, std::back_inserter(new_chunk));
    remaining -= copy_size;
  }

  active_chunk_.swap(new_chunk);
  active_chunk_consumed_ = 0;
}

void ReadBuffer::consumeFromChunks(size_t size) {
  while (size > 0 && !chunks_.empty()) {
    size_t available = chunks_.front()->ReadableSize();
    size_t consume_size = std::min(size, available);
    chunks_.front()->Consume(consume_size);
    size -= consume_size;
    if (chunks_.front()->ReadableSize() == 0) {
      chunks_.pop_front();
    }
  }
}

void ReadBuffer::Drain(size_t size) {
  size_t remaining = size;

  if (active_chunk_consumed_ < active_chunk_.size()) {
    size_t available = active_chunk_.size() - active_chunk_consumed_;
    size_t drain_amount = std::min(available, remaining);
    active_chunk_consumed_ += drain_amount;
    consumeFromChunks(drain_amount);
    remaining -= drain_amount;
    if (active_chunk_consumed_ == active_chunk_.size()) {
      active_chunk_.clear();
      active_chunk_consumed_ = 0;
    }
  }

  if (remaining > 0) {
    consumeFromChunks(remaining);
  }
}

auto ReadBuffer::HasReadableData() const -> bool {
  if (active_chunk_consumed_ < active_chunk_.size()) {
    return true;
  }

  for (auto const& chunk : chunks_) {
    if (chunk->HasReadableData()) {
      return true;
    }
  }

  return false;
}

auto ReadBuffer::GetTotalReadableSize() const -> size_t {
  size_t total = 0;
  if (active_chunk_consumed_ < active_chunk_.size()) {
    total += active_chunk_.size() - active_chunk_consumed_;
  }

  for (auto const& chunk : chunks_) {
    total += chunk->ReadableSize();
  }
  return total;
}

void ReadBuffer::HandleCompletion(int res, uint32_t flags) {
  LOG_DEBUG("Got read {} bytes. Flags: {}", res, flags);
  if (res > 0) {
    if (chunks_.empty()) {
      chunks_.emplace_back(std::make_unique<Chunk>());
    }
    chunks_.back()->Fill(static_cast<size_t>(res));
    on_read_completed_(this, res);
    return;
  }

  on_read_completed_(this, res);
}

} // namespace carrot::io
