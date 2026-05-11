#pragma once

#include <nghttp2/nghttp2.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/io/protocol_parser.hh"

namespace carrot::io {

class HttpProtocolParser : public ProtocolParser {
public:
  HttpProtocolParser();
  ~HttpProtocolParser() override;

  auto TryParseMessage(std::span<const std::byte> data) -> ParseResult override;
  void ProcessMessage(std::span<const std::byte> message_data,
                      std::function<void(std::span<const std::byte>)> response_callback) override;

  static constexpr const char kHttp2Preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

private:
  enum class ProtocolType { Unknown, Http1, Http2 };

  struct StreamState {
    std::vector<std::byte> body;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string method;
    std::string path;
    std::string response_body;
    size_t response_offset = 0;
    bool response_submitted = false;
  };

  ProtocolType protocol_type_{ProtocolType::Unknown};
  nghttp2_session* session_{nullptr};
  std::vector<std::byte> output_buffer_;
  std::unordered_map<int32_t, std::unique_ptr<StreamState>> stream_states_;
  std::function<void(std::span<const std::byte>)> response_callback_;

  static ssize_t SendCallback(nghttp2_session* session, const uint8_t* data, size_t length,
                              int flags, void* user_data);

  static int OnHeaderCallback(nghttp2_session* session, const nghttp2_frame* frame,
                              const uint8_t* name, size_t namelen, const uint8_t* value,
                              size_t valuelen, uint8_t flags, void* user_data);

  static int OnFrameRecvCallback(nghttp2_session* session, const nghttp2_frame* frame,
                                 void* user_data);

  static int OnDataChunkRecvCallback(nghttp2_session* session, uint8_t flags, int32_t stream_id,
                                     const uint8_t* data, size_t len, void* user_data);

  static int OnStreamCloseCallback(nghttp2_session* session, int32_t stream_id, uint32_t error_code,
                                   void* user_data);

  static ssize_t DataSourceReadCallback(nghttp2_session* session, int32_t stream_id, uint8_t* buf,
                                        size_t length, uint32_t* data_flags,
                                        nghttp2_data_source* source, void* user_data);

  static nghttp2_nv MakeNV(std::string_view name, std::string_view value);

  void AppendOutput(const uint8_t* data, size_t length);
  void SubmitHttp2Response(int32_t stream_id);
  void MaybeSubmitResponse(int32_t stream_id);

  static auto ParseUInt(std::string_view text) -> std::optional<size_t>;
  static auto FindHeaderValue(std::string_view headers, std::string_view header_name)
      -> std::optional<std::string_view>;
};

} // namespace carrot::io
