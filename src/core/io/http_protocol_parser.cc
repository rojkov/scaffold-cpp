#include "core/io/http_protocol_parser.hh"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <string>

#include "core/logging/log.hh"

namespace carrot::io {

namespace {

bool IsHttp2PrefacePrefix(std::span<const std::byte> data) {
  size_t prefix_length = std::min(data.size(), sizeof(HttpProtocolParser::kHttp2Preface) - 1);
  return std::memcmp(data.data(), HttpProtocolParser::kHttp2Preface, prefix_length) == 0;
}

} // namespace

HttpProtocolParser::HttpProtocolParser() {
  nghttp2_session_callbacks* callbacks = nullptr;
  nghttp2_session_callbacks_new(&callbacks);
  nghttp2_session_callbacks_set_send_callback(callbacks, &HttpProtocolParser::SendCallback);
  nghttp2_session_callbacks_set_on_header_callback(callbacks,
                                                   &HttpProtocolParser::OnHeaderCallback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       &HttpProtocolParser::OnFrameRecvCallback);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
      callbacks, &HttpProtocolParser::OnDataChunkRecvCallback);
  nghttp2_session_callbacks_set_on_stream_close_callback(
      callbacks, &HttpProtocolParser::OnStreamCloseCallback);

  nghttp2_session_server_new(&session_, callbacks, this);
  nghttp2_session_callbacks_del(callbacks);
}

HttpProtocolParser::~HttpProtocolParser() {
  if (session_) {
    nghttp2_session_del(session_);
  }
}

auto HttpProtocolParser::TryParseMessage(std::span<const std::byte> data) -> ParseResult {
  if (data.empty()) {
    return {.found_message = false, .message_length = 0};
  }

  if (protocol_type_ == ProtocolType::Unknown) {
    if (IsHttp2PrefacePrefix(data)) {
      if (data.size() < sizeof(kHttp2Preface) - 1) {
        return {.found_message = false, .message_length = 0};
      }
      protocol_type_ = ProtocolType::Http2;
      return {.found_message = true, .message_length = sizeof(kHttp2Preface) - 1};
    }
    protocol_type_ = ProtocolType::Http1;
  }

  if (protocol_type_ == ProtocolType::Http2) {
    return {.found_message = true, .message_length = data.size()};
  }

  auto* begin = reinterpret_cast<const char*>(data.data());
  std::string_view view(begin, data.size());
  size_t header_end = view.find("\r\n\r\n");
  if (header_end == std::string_view::npos) {
    return {.found_message = false, .message_length = 0};
  }

  size_t headers_length = header_end + 4;
  size_t content_length = 0;
  if (auto content_length_value = FindHeaderValue(view.substr(0, header_end), "content-length")) {
    if (auto parsed = ParseUInt(*content_length_value)) {
      content_length = *parsed;
    }
  }

  size_t total_length = headers_length + content_length;
  if (view.size() < total_length) {
    return {.found_message = false, .message_length = 0};
  }

  return {.found_message = true, .message_length = total_length};
}

void HttpProtocolParser::ProcessMessage(
    std::span<const std::byte> message_data,
    std::function<void(std::span<const std::byte>)> response_callback) {
  if (protocol_type_ == ProtocolType::Unknown) {
    protocol_type_ = ProtocolType::Http1;
  }

  if (protocol_type_ == ProtocolType::Http2) {
    response_callback_ = std::move(response_callback);
    const uint8_t* payload = reinterpret_cast<const uint8_t*>(message_data.data());
    ssize_t consumed = nghttp2_session_mem_recv(session_, payload, message_data.size());
    if (consumed < 0) {
      LOG_ERROR("nghttp2_session_mem_recv failed: {}", consumed);
      return;
    }

    nghttp2_session_send(session_);
    if (!output_buffer_.empty()) {
      response_callback_(std::span<const std::byte>(output_buffer_.data(), output_buffer_.size()));
      output_buffer_.clear();
    }
    return;
  }

  const auto* begin = reinterpret_cast<const char*>(message_data.data());
  std::string_view view(begin, message_data.size());
  size_t header_end = view.find("\r\n\r\n");
  if (header_end == std::string_view::npos) {
    return;
  }

  auto request_line = view.substr(0, view.find("\r\n"));
  std::string method;
  std::string path;
  std::string version;
  {
    size_t first_space = request_line.find(' ');
    size_t second_space = std::string_view::npos;
    if (first_space != std::string_view::npos) {
      second_space = request_line.find(' ', first_space + 1);
    }
    if (first_space != std::string_view::npos && second_space != std::string_view::npos) {
      method = std::string(request_line.substr(0, first_space));
      path = std::string(request_line.substr(first_space + 1, second_space - first_space - 1));
      version = std::string(request_line.substr(second_space + 1));
    }
  }

  std::string body;
  if (message_data.size() > header_end + 4) {
    auto payload = view.substr(header_end + 4);
    body.assign(payload.begin(), payload.end());
  }

  std::string response_body = std::format("Echoed request line: {} {} {}\n", method, path, version);
  if (!body.empty()) {
    response_body += "Body:\n";
    response_body += body;
    response_body += '\n';
  }

  std::string response = std::format("HTTP/1.1 200 OK\r\nContent-Length: {}\r\nContent-Type: "
                                     "text/plain\r\nConnection: close\r\n\r\n{}",
                                     response_body.size(), response_body);

  auto response_bytes = std::as_bytes(std::span(response.data(), response.size()));
  response_callback(response_bytes);
}

ssize_t HttpProtocolParser::SendCallback(nghttp2_session* /*session*/, const uint8_t* data,
                                         size_t length, int /*flags*/, void* user_data) {
  auto* parser = static_cast<HttpProtocolParser*>(user_data);
  parser->AppendOutput(data, length);
  return static_cast<ssize_t>(length);
}

int HttpProtocolParser::OnHeaderCallback(nghttp2_session* /*session*/, const nghttp2_frame* frame,
                                         const uint8_t* name, size_t namelen, const uint8_t* value,
                                         size_t valuelen, uint8_t /*flags*/, void* user_data) {
  if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  auto* parser = static_cast<HttpProtocolParser*>(user_data);
  auto& stream_state = parser->stream_states_[frame->hd.stream_id];
  if (!stream_state) {
    stream_state = std::make_unique<StreamState>();
  }

  std::string header_name(reinterpret_cast<const char*>(name), namelen);
  std::string header_value(reinterpret_cast<const char*>(value), valuelen);
  stream_state->headers.emplace_back(std::move(header_name), std::move(header_value));
  return 0;
}

int HttpProtocolParser::OnFrameRecvCallback(nghttp2_session* session, const nghttp2_frame* frame,
                                            void* user_data) {
  if (!user_data) {
    return 0;
  }

  auto* parser = static_cast<HttpProtocolParser*>(user_data);
  if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
      parser->MaybeSubmitResponse(frame->hd.stream_id);
    }
  }
  return 0;
}

int HttpProtocolParser::OnDataChunkRecvCallback(nghttp2_session* /*session*/, uint8_t /*flags*/,
                                                int32_t stream_id, const uint8_t* data, size_t len,
                                                void* user_data) {
  auto* parser = static_cast<HttpProtocolParser*>(user_data);
  auto it = parser->stream_states_.find(stream_id);
  if (it == parser->stream_states_.end()) {
    return 0;
  }

  auto* stream_state = it->second.get();
  stream_state->body.insert(stream_state->body.end(), reinterpret_cast<const std::byte*>(data),
                            reinterpret_cast<const std::byte*>(data + len));
  return 0;
}

int HttpProtocolParser::OnStreamCloseCallback(nghttp2_session* /*session*/, int32_t stream_id,
                                              uint32_t /*error_code*/, void* user_data) {
  auto* parser = static_cast<HttpProtocolParser*>(user_data);
  parser->MaybeSubmitResponse(stream_id);
  return 0;
}

ssize_t HttpProtocolParser::DataSourceReadCallback(nghttp2_session* /*session*/,
                                                   int32_t /*stream_id*/, uint8_t* buf,
                                                   size_t length, uint32_t* data_flags,
                                                   nghttp2_data_source* /*source*/,
                                                   void* user_data) {
  if (data_flags) {
    *data_flags = NGHTTP2_DATA_FLAG_NONE;
  }
  auto* stream_state = static_cast<StreamState*>(user_data);
  size_t remaining = stream_state->response_body.size() - stream_state->response_offset;
  size_t to_copy = std::min(length, remaining);
  if (to_copy > 0) {
    std::memcpy(buf, stream_state->response_body.data() + stream_state->response_offset, to_copy);
    stream_state->response_offset += to_copy;
  }
  return static_cast<ssize_t>(to_copy);
}

nghttp2_nv HttpProtocolParser::MakeNV(std::string_view name, std::string_view value) {
  return nghttp2_nv{const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name.data())),
                    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value.data())),
                    name.size(), value.size(), NGHTTP2_NV_FLAG_NONE};
}

void HttpProtocolParser::AppendOutput(const uint8_t* data, size_t length) {
  output_buffer_.insert(output_buffer_.end(), reinterpret_cast<const std::byte*>(data),
                        reinterpret_cast<const std::byte*>(data + length));
}

void HttpProtocolParser::MaybeSubmitResponse(int32_t stream_id) {
  auto it = stream_states_.find(stream_id);
  if (it == stream_states_.end()) {
    return;
  }

  auto* stream_state = it->second.get();
  if (stream_state->response_submitted) {
    return;
  }

  std::string method = "GET";
  std::string path = "/";
  for (auto& header : stream_state->headers) {
    if (header.first == ":method") {
      method = header.second;
    } else if (header.first == ":path") {
      path = header.second;
    }
  }

  stream_state->response_body = std::format("Echoed HTTP/2 request: {} {}\n", method, path);
  if (!stream_state->body.empty()) {
    stream_state->response_body += "Body:\n";
    stream_state->response_body.append(reinterpret_cast<const char*>(stream_state->body.data()),
                                       stream_state->body.size());
    stream_state->response_body += '\n';
  }

  std::vector<nghttp2_nv> headers;
  headers.push_back(MakeNV(":status", "200"));
  headers.push_back(MakeNV("content-type", "text/plain"));
  headers.push_back(MakeNV("content-length", std::to_string(stream_state->response_body.size())));

  stream_state->response_submitted = true;
  nghttp2_data_provider data_provider;
  data_provider.read_callback = &HttpProtocolParser::DataSourceReadCallback;
  data_provider.source.ptr = stream_state;

  nghttp2_submit_response(session_, stream_id, headers.data(), headers.size(), &data_provider);
}

auto HttpProtocolParser::ParseUInt(std::string_view text) -> std::optional<size_t> {
  size_t value = 0;
  auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec == std::errc()) {
    return value;
  }
  return std::nullopt;
}

auto HttpProtocolParser::FindHeaderValue(std::string_view headers, std::string_view header_name)
    -> std::optional<std::string_view> {
  header_name = std::string_view(header_name.data(), header_name.size());
  for (size_t pos = 0; pos < headers.size();) {
    size_t end = headers.find('\n', pos);
    if (end == std::string_view::npos) {
      end = headers.size();
    }

    std::string_view line = headers.substr(pos, end - pos);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }

    size_t colon = line.find(':');
    if (colon != std::string_view::npos) {
      std::string_view name = line.substr(0, colon);
      std::string_view value = line.substr(colon + 1);
      while (!value.empty() && value.front() == ' ') {
        value.remove_prefix(1);
      }

      std::string lc_name(name);
      std::transform(lc_name.begin(), lc_name.end(), lc_name.begin(), ::tolower);
      if (lc_name == header_name) {
        return value;
      }
    }

    pos = end + 1;
  }

  return std::nullopt;
}

} // namespace carrot::io
