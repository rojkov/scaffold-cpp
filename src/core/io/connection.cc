#include "core/io/connection.hh"

#include <algorithm>

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
    size_t copy_size = std::min(existing, remaining);
    std::copy_n(active_chunk_.begin() + active_chunk_consumed_, copy_size,
                std::back_inserter(new_chunk));
    remaining -= copy_size;
  }

  for (auto const& chunk : chunks_) {
    if (remaining == 0) {
      break;
    }

    auto read_area = chunk->GetReadArea();
    size_t copy_size = std::min(read_area.size(), remaining);
    std::copy_n(read_area.begin(), copy_size, std::back_inserter(new_chunk));
    remaining -= copy_size;
  }

  active_chunk_.swap(new_chunk);
  active_chunk_consumed_ = 0;
}

void ReadBuffer::ConsumeFromChunks(size_t size) {
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
    ConsumeFromChunks(drain_amount);
    remaining -= drain_amount;
    if (active_chunk_consumed_ == active_chunk_.size()) {
      active_chunk_.clear();
      active_chunk_consumed_ = 0;
    }
  }

  if (remaining > 0) {
    ConsumeFromChunks(remaining);
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

Connection::Connection(int connection_fd, event::DispatcherSharedPtr dispatcher)
    : fd_{connection_fd}, dispatcher_{std::move(dispatcher)},
      read_buffer_{[this](ReadBuffer* buffer, int res) { OnReadCompleted(buffer, res); }},
      write_buffer_{[this](WriteBuffer* buffer, int res) { OnWriteCompleted(buffer, res); }} {
  SubmitRead();
}

void Connection::Send(std::span<const std::byte> data) {
  bool had_pending_write = write_buffer_.HasPendingWrite();
  write_buffer_.Append(data);
  if (!had_pending_write) {
    SubmitWrite();
  }
}

void Connection::SubmitRead() {
  auto buffer = read_buffer_.GetWriteSpan();
  if (!buffer.empty()) {
    dispatcher_->PrepareRead(&read_buffer_, fd_, buffer, 0);
  }
}

void Connection::SubmitWrite() {
  if (!write_buffer_.HasPendingWrite()) {
    return;
  }

  auto buffer = write_buffer_.GetPendingWriteSpan();
  if (!buffer.empty()) {
    dispatcher_->PrepareWrite(&write_buffer_, fd_, buffer, 0);
  }
}

void Connection::OnReadCompleted(ReadBuffer* /*unused*/, int res) {
  if (res <= 0) {
    LOG_DEBUG("Connection read closed or error: {}", res);
    return;
  }

  if (read_buffer_.HasReadableData()) {
    LOG_DEBUG("Connection has {} bytes pending in read buffer",
              read_buffer_.GetTotalReadableSize());
  }

  ParseMessages();

  SubmitRead();
  if (write_buffer_.HasPendingWrite()) {
    SubmitWrite();
  }
}

void Connection::OnWriteCompleted(WriteBuffer* /*unused*/, int res) {
  if (res < 0) {
    LOG_DEBUG("Connection write error: {}", res);
    return;
  }

  if (write_buffer_.HasPendingWrite()) {
    SubmitWrite();
  }
}

void Connection::ParseMessages() {
  while (read_buffer_.HasReadableData()) {
    auto current_span = read_buffer_.GetSpan();
    if (current_span.empty()) {
      break;
    }

    auto parse_result = SimpleLineProtocolParser::TryParseMessage(current_span);
    if (!parse_result.found_message) {
      break;
    }

    auto message_bytes = current_span.subspan(0, parse_result.message_length);
    SimpleLineProtocolParser::ProcessMessage(message_bytes);

    read_buffer_.Drain(parse_result.message_length);

    std::string echo_response = "Echo: ";
    auto* begin = reinterpret_cast<const char*>(message_bytes.data());
    size_t len = message_bytes.size();
    if (len > 0 && begin[len - 1] == '\n') {
      len--;
      if (len > 0 && begin[len - 1] == '\r') {
        len--;
      }
    }
    echo_response.append(begin, len);
    echo_response.append("\n");

    auto echo_bytes = std::as_bytes(std::span(echo_response));
    Send(echo_bytes);
  }
}

} // namespace carrot::io
