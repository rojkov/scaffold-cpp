#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "core/io/http_protocol_parser.hh"
#include "core/io/simple_line_protocol_parser.hh"

namespace carrot::io {
namespace {

static std::vector<std::byte> MakeBytes(std::string_view text) {
  std::vector<std::byte> bytes;
  bytes.reserve(text.size());
  for (unsigned char ch : text) {
    bytes.push_back(static_cast<std::byte>(ch));
  }
  return bytes;
}

static std::string BytesToString(std::span<const std::byte> bytes) {
  std::string result;
  result.reserve(bytes.size());
  for (auto b : bytes) {
    result.push_back(static_cast<char>(b));
  }
  return result;
}

TEST(SimpleLineProtocolParserTest, TryParseMessageFindsNewline) {
  SimpleLineProtocolParser parser;
  auto data = MakeBytes("hello\r\n");

  auto result = parser.TryParseMessage(data);

  EXPECT_TRUE(result.found_message);
  EXPECT_EQ(result.message_length, data.size());
}

TEST(SimpleLineProtocolParserTest, ProcessMessageEchoesLine) {
  SimpleLineProtocolParser parser;
  auto data = MakeBytes("hello\r\n");
  std::string output;

  parser.ProcessMessage(
      data, [&](std::span<const std::byte> response) { output = BytesToString(response); });

  EXPECT_EQ(output, "Echo: hello\n");
}

TEST(HttpProtocolParserTest, DetectsHttp2Preface) {
  HttpProtocolParser parser;
  auto data = MakeBytes(HttpProtocolParser::kHttp2Preface);

  auto result = parser.TryParseMessage(data);

  EXPECT_TRUE(result.found_message);
  EXPECT_EQ(result.message_length, std::strlen(HttpProtocolParser::kHttp2Preface));
}

TEST(HttpProtocolParserTest, ParsesHttp1RequestAndReturnsEchoResponse) {
  HttpProtocolParser parser;
  std::string request = "GET /hello HTTP/1.1\r\nHost: example.com\r\n\r\n";
  auto data = MakeBytes(request);
  std::string output;

  auto result = parser.TryParseMessage(data);
  EXPECT_TRUE(result.found_message);
  EXPECT_EQ(result.message_length, data.size());

  parser.ProcessMessage(
      data, [&](std::span<const std::byte> response) { output = BytesToString(response); });

  EXPECT_NE(output.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(output.find("Echoed request line: GET /hello HTTP/1.1"), std::string::npos);
}

TEST(HttpProtocolParserTest, ParsesHttp1RequestWithBody) {
  HttpProtocolParser parser;
  std::string request =
      "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nhello";
  auto data = MakeBytes(request);
  std::string output;

  auto result = parser.TryParseMessage(data);
  EXPECT_TRUE(result.found_message);
  EXPECT_EQ(result.message_length, data.size());

  parser.ProcessMessage(
      data, [&](std::span<const std::byte> response) { output = BytesToString(response); });

  EXPECT_NE(output.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(output.find("Echoed request line: POST /submit HTTP/1.1"), std::string::npos);
  EXPECT_NE(output.find("Body:\nhello\n"), std::string::npos);
}

} // namespace
} // namespace carrot::io
