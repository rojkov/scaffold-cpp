#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>

#include "carrot/event/dispatcher.hh"
#include "carrot/event/io_object.hh"
#include "core/io/read_buffer.hh"
#include "core/io/write_buffer.hh"

namespace carrot::io {

class ProtocolParser;

class Connection : public event::IOObject {
public:
  explicit Connection(int connection_fd, event::DispatcherSharedPtr dispatcher,
                      std::unique_ptr<ProtocolParser> parser = nullptr);

  void HandleCompletion(int res, uint32_t flags) override {}
  void ProcessCommand(event::Command cmd) override {}

  void Send(std::span<const std::byte> data);

private:
  void SubmitRead();
  void SubmitWrite();
  void OnReadCompleted(ReadBuffer* read_buffer, int res);
  void OnWriteCompleted(WriteBuffer* write_buffer, int res);
  void ParseMessages();

  int fd_;
  event::DispatcherSharedPtr dispatcher_;
  ReadBuffer read_buffer_;
  WriteBuffer write_buffer_;
  std::unique_ptr<ProtocolParser> parser_;
};

} // namespace carrot::io
