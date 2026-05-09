#include "core/io/connection.hh"

#include "core/logging/log.hh"

namespace carrot::io {
ReadBuffer::ReadBuffer(std::function<void(ReadBuffer*)> on_read_completed)
    : on_read_completed_{std::move(on_read_completed)} {}

auto ReadBuffer::GetSpan() -> std::span<std::byte> {
  if (chunks_.empty() || chunks_.back()->IsFullForReading()) {
    chunks_.emplace_back(std::make_unique<Chunk>());
  }

  return chunks_.back()->GetReadArea();
}

// Linearize the read buffer to make it contiguous.
// TODO: make it return not void, but something indicating success or failure.
void ReadBuffer::Pullup(size_t size) {
  if (active_chunk_.size() == 0 && chunks_.front()->ReadableSize() >= size) {
    return; // true
  }

  if (active_chunk_.size() > 0 && (active_chunk_.size() - active_chunk_consumed_) >= size) {
    return; // true
  }

  // Still can fit in active_chunk_.
  if (active_chunk_.capacity() - active_chunk_consumed_ >= size) {
    // In IO buffer there's enough data not to get to the next one.
    if (chunks_.front()->ReadableSize() >=
        (size - (active_chunk_.size() - active_chunk_consumed_))) {
      // std::memcpy(d,s,n);
      active_chunk_.append_range(std::span(
          chunks_.front()->Consume(size - (active_chunk_.size() - active_chunk_consumed_))));
      return; // true
    }
  }
}

void ReadBuffer::HandleCompletion(int res, uint32_t flags) {
  LOG_DEBUG("Got read {} bytes. Flags: {}", res, flags);
  if (res > 0) {
    on_read_completed_(this);
    return;
  }

  // TODO: call on_disconnect_();
}

Connection::Connection(int connection_fd, event::DispatcherSharedPtr dispatcher)
    : fd_{connection_fd}, dispatcher_{std::move(dispatcher)} {
  dispatcher_->PrepareRead(this, fd_, buffer_, 0);
}

void Connection::HandleCompletion(int res, uint32_t flags) {
  LOG_DEBUG("Got read {} bytes. Flags: {}", res, flags);
  if (res != 0) {
    // TODO: decode the buffer and send decoded messages to the session object
    dispatcher_->PrepareRead(this, fd_, buffer_, 0);
    dispatcher_->PrepareWrite(this, fd_, std::span(buffer_).subspan(0, res), 0);
  } else {
    // TODO: close and delete the connection
  }
}

} // namespace carrot::io
