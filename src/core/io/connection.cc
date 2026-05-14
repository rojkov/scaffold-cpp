#include "core/io/connection.hh"

#include "core/io/http_protocol_parser.hh"
#include "core/logging/log.hh"

namespace carrot::io {

Connection::~Connection() = default;

Connection::Connection(int connection_fd, event::DispatcherSharedPtr dispatcher,
                       std::unique_ptr<ProtocolParser> parser)
    : fd_{connection_fd}, dispatcher_{std::move(dispatcher)},
      read_buffer_{[this](ReadBuffer* buffer, int res) { OnReadCompleted(buffer, res); }},
      write_buffer_{[this](WriteBuffer* buffer, int res) { OnWriteCompleted(buffer, res); }},
      parser_{parser ? std::move(parser) : std::make_unique<HttpProtocolParser>()} {
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
    parser_->Feed(read_buffer_, write_buffer_);
    LOG_DEBUG("in write_buffer {}", write_buffer_.HasPendingWrite());
    auto buffer = read_buffer_.GetWriteSpan();
    assert(!buffer.empty());
    dispatcher_->PrepareRead(&read_buffer_, fd_, buffer, 0);
    // TODO: check if there's no pending write completion.
    if (write_buffer_.HasPendingWrite()) {
      SubmitWrite();
    }
  }

  /*
  ParseMessages();

  SubmitRead();
  if (write_buffer_.HasPendingWrite()) {
    SubmitWrite();
  }
  */
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

    auto parse_result = parser_->TryParseMessage(current_span);
    if (!parse_result.found_message) {
      break;
    }

    auto message_bytes = current_span.subspan(0, parse_result.message_length);
    parser_->ProcessMessage(message_bytes,
                            [this](std::span<const std::byte> response) { Send(response); });

    read_buffer_.Drain(parse_result.message_length);
  }
}

} // namespace carrot::io
