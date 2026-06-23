#include "carrot/common/wire_protocol.hh"

#include <arpa/inet.h>
#include <cstring>

#include <msgpack.h>

namespace carrot::common::wire {

auto encodeHeader(MessageType type, uint32_t payload_length) -> std::array<std::byte, 5> {
  std::array<std::byte, 5> header{};
  header[0] = static_cast<std::byte>(type);
  uint32_t net_len = htonl(payload_length);
  std::memcpy(&header[1], &net_len, sizeof(net_len));
  return header;
}

auto decodeHeader(std::span<const std::byte> data) -> TlvHeader {
  TlvHeader header{};
  header.type = static_cast<MessageType>(data[0]);
  uint32_t net_len;
  std::memcpy(&net_len, &data[1], sizeof(net_len));
  header.length = ntohl(net_len);
  return header;
}

namespace {

void pack_requirements(msgpack_packer& pk, const std::vector<Requirement>& reqs) {
  msgpack_pack_array(&pk, reqs.size());
  for (const auto& req : reqs) {
    msgpack_pack_map(&pk, 2);
    msgpack_pack_str(&pk, 4);
    msgpack_pack_str_body(&pk, "type", 4);
    msgpack_pack_str(&pk, req.type.size());
    msgpack_pack_str_body(&pk, req.type.data(), req.type.size());
    msgpack_pack_str(&pk, 6);
    msgpack_pack_str_body(&pk, "amount", 6);
    msgpack_pack_uint64(&pk, req.amount);
  }
}

void pack_string(msgpack_packer& pk, const std::string& key, const std::string& value) {
  msgpack_pack_str(&pk, key.size());
  msgpack_pack_str_body(&pk, key.data(), key.size());
  msgpack_pack_str(&pk, value.size());
  msgpack_pack_str_body(&pk, value.data(), value.size());
}

void pack_binary(msgpack_packer& pk, const std::string& key, std::span<const std::byte> data) {
  msgpack_pack_str(&pk, key.size());
  msgpack_pack_str_body(&pk, key.data(), key.size());
  msgpack_pack_bin(&pk, data.size());
  msgpack_pack_bin_body(&pk, data.data(), data.size());
}

void pack_optional_string(msgpack_packer& pk, const std::string& key,
                          const std::optional<std::string>& value) {
  msgpack_pack_str(&pk, key.size());
  msgpack_pack_str_body(&pk, key.data(), key.size());
  if (value) {
    msgpack_pack_str(&pk, value->size());
    msgpack_pack_str_body(&pk, value->data(), value->size());
  } else {
    msgpack_pack_nil(&pk);
  }
}

} // namespace

auto serializeTask(const Task& task) -> std::vector<std::byte> {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_map(&pk, 5);
  pack_string(pk, "type", task.type);
  pack_binary(pk, "body", task.body);
  pack_optional_string(pk, "function", task.function);
  pack_optional_string(pk, "result_endpoint", task.result_endpoint);

  msgpack_pack_str(&pk, 12);
  msgpack_pack_str_body(&pk, "requirements", 12);
  pack_requirements(pk, task.requirements);

  auto result =
      std::vector<std::byte>(reinterpret_cast<const std::byte*>(sbuf.data),
                             reinterpret_cast<const std::byte*>(sbuf.data) + sbuf.size);
  msgpack_sbuffer_destroy(&sbuf);
  return result;
}

auto deserializeTask(std::span<const std::byte> data) -> Task {
  msgpack_unpacked msg;
  msgpack_unpacked_init(&msg);

  size_t off = 0;
  msgpack_unpack_next(&msg, reinterpret_cast<const char*>(data.data()), data.size(), &off);
  auto obj = msg.data;

  Task task;
  if (obj.type == MSGPACK_OBJECT_MAP) {
    for (uint32_t i = 0; i < obj.via.map.size; ++i) {
      auto key = obj.via.map.ptr[i].key;
      auto val = obj.via.map.ptr[i].val;
      if (key.type != MSGPACK_OBJECT_STR) {
        continue;
      }
      std::string_view key_sv(key.via.str.ptr, key.via.str.size);

      if (key_sv == "type") {
        task.type.assign(val.via.str.ptr, val.via.str.size);
      } else if (key_sv == "body") {
        if (val.type == MSGPACK_OBJECT_BIN) {
          task.body.assign(reinterpret_cast<const std::byte*>(val.via.bin.ptr),
                           reinterpret_cast<const std::byte*>(val.via.bin.ptr) + val.via.bin.size);
        } else if (val.type == MSGPACK_OBJECT_STR) {
          task.body.assign(reinterpret_cast<const std::byte*>(val.via.str.ptr),
                           reinterpret_cast<const std::byte*>(val.via.str.ptr) + val.via.str.size);
        }
      } else if (key_sv == "function") {
        if (val.type != MSGPACK_OBJECT_NIL) {
          task.function = std::string(val.via.str.ptr, val.via.str.size);
        }
      } else if (key_sv == "result_endpoint") {
        if (val.type != MSGPACK_OBJECT_NIL) {
          task.result_endpoint = std::string(val.via.str.ptr, val.via.str.size);
        }
      } else if (key_sv == "requirements") {
        if (val.type == MSGPACK_OBJECT_ARRAY) {
          for (uint32_t j = 0; j < val.via.array.size; ++j) {
            auto req_obj = val.via.array.ptr[j];
            if (req_obj.type != MSGPACK_OBJECT_MAP) {
              continue;
            }
            Requirement req;
            for (uint32_t k = 0; k < req_obj.via.map.size; ++k) {
              auto rk = req_obj.via.map.ptr[k].key;
              auto rv = req_obj.via.map.ptr[k].val;
              if (rk.type != MSGPACK_OBJECT_STR) {
                continue;
              }
              std::string_view rk_sv(rk.via.str.ptr, rk.via.str.size);
              if (rk_sv == "type" && rv.type == MSGPACK_OBJECT_STR) {
                req.type.assign(rv.via.str.ptr, rv.via.str.size);
              } else if (rk_sv == "amount" && rv.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                req.amount = rv.via.u64;
              }
            }
            task.requirements.push_back(std::move(req));
          }
        }
      }
    }
  }

  msgpack_unpacked_destroy(&msg);
  return task;
}

auto serializeChunk(uint64_t task_id, Chunk chunk, bool is_final) -> std::vector<std::byte> {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_map(&pk, 3);
  pack_string(pk, "task_id", std::to_string(task_id));
  pack_binary(pk, "data", chunk);
  msgpack_pack_str(&pk, 8);
  msgpack_pack_str_body(&pk, "is_final", 8);
  if (is_final) {
    msgpack_pack_true(&pk);
  } else {
    msgpack_pack_false(&pk);
  }

  auto result =
      std::vector<std::byte>(reinterpret_cast<const std::byte*>(sbuf.data),
                             reinterpret_cast<const std::byte*>(sbuf.data) + sbuf.size);
  msgpack_sbuffer_destroy(&sbuf);
  return result;
}

auto serializeComplete(uint64_t task_id) -> std::vector<std::byte> {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_map(&pk, 1);
  pack_string(pk, "task_id", std::to_string(task_id));

  auto result =
      std::vector<std::byte>(reinterpret_cast<const std::byte*>(sbuf.data),
                             reinterpret_cast<const std::byte*>(sbuf.data) + sbuf.size);
  msgpack_sbuffer_destroy(&sbuf);
  return result;
}

auto serializeError(uint64_t task_id, std::string_view message) -> std::vector<std::byte> {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_map(&pk, 2);
  pack_string(pk, "task_id", std::to_string(task_id));
  msgpack_pack_str(&pk, 7);
  msgpack_pack_str_body(&pk, "message", 7);
  msgpack_pack_str(&pk, message.size());
  msgpack_pack_str_body(&pk, message.data(), message.size());

  auto result =
      std::vector<std::byte>(reinterpret_cast<const std::byte*>(sbuf.data),
                             reinterpret_cast<const std::byte*>(sbuf.data) + sbuf.size);
  msgpack_sbuffer_destroy(&sbuf);
  return result;
}

} // namespace carrot::common::wire
