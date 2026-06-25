#include "carrot/common/connection_pool.hh"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fcntl.h>

namespace carrot::common {

void ConnectionPool::addNode(const NodeInfo& info) {
  NodeConnection conn;
  conn.node_id = info.id;
  conn.alive = false;

  conn.fd = socket(AF_INET, SOCK_STREAM, 0);
  if (conn.fd < 0) {
    return;
  }

  auto colon = info.address.find(':');
  if (colon == std::string_view::npos) {
    close(conn.fd);
    conn.fd = -1;
    return;
  }
  auto host = info.address.substr(0, colon);
  auto port_str = info.address.substr(colon + 1);
  uint32_t port = std::stoul(std::string(port_str));

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(static_cast<uint16_t>(port))};
  if (inet_pton(AF_INET, std::string(host).c_str(), &addr.sin_addr) <= 0) {
    close(conn.fd);
    conn.fd = -1;
    return;
  }

  if (connect(conn.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
    int flags = fcntl(conn.fd, F_GETFL, 0);
    fcntl(conn.fd, F_SETFL, flags | O_NONBLOCK);
    conn.alive = true;
  }

  for (const auto& req : info.max_capacity) {
    if (req.type == "cpu") {
      conn.capacity.cpu_max = req.amount;
    } else if (req.type == "mem") {
      conn.capacity.mem_max = req.amount;
    }
  }

  connections_[info.id] = std::move(conn);
}

void ConnectionPool::removeNode(std::string_view node_id) {
  auto key = std::string(node_id);
  auto it = connections_.find(key);
  if (it == connections_.end()) {
    return;
  }
  if (it->second.fd >= 0) {
    close(it->second.fd);
  }
  connections_.erase(it);
}

void ConnectionPool::updateNode(const NodeInfo& info) {
  removeNode(info.id);
  addNode(info);
}

auto ConnectionPool::getConnection(std::string_view node_id) -> NodeConnection* {
  auto key = std::string(node_id);
  auto it = connections_.find(key);
  return it != connections_.end() ? &it->second : nullptr;
}

auto ConnectionPool::isAlive(std::string_view node_id) const -> bool {
  auto key = std::string(node_id);
  auto it = connections_.find(key);
  return it != connections_.end() && it->second.alive;
}

auto ConnectionPool::hasCapacity(std::string_view node_id, const Task& task) const -> bool {
  auto key = std::string(node_id);
  auto it = connections_.find(key);
  if (it == connections_.end()) {
    return false;
  }
  const auto& conn = it->second;
  uint64_t needed_cpu = 0;
  uint64_t needed_mem = 0;
  for (const auto& req : task.requirements) {
    if (req.type == "cpu") {
      needed_cpu = req.amount;
    } else if (req.type == "mem") {
      needed_mem = req.amount;
    }
  }
  return (conn.cpu_used + needed_cpu <= conn.capacity.cpu_max ||
          conn.capacity.cpu_max == 0) &&
         (conn.mem_used + needed_mem <= conn.capacity.mem_max ||
          conn.capacity.mem_max == 0);
}

void ConnectionPool::reserveCapacity(std::string_view node_id, const Task& task) {
  auto* conn = getConnection(node_id);
  if (conn == nullptr) {
    return;
  }
  for (const auto& req : task.requirements) {
    if (req.type == "cpu") {
      conn->cpu_used += req.amount;
    } else if (req.type == "mem") {
      conn->mem_used += req.amount;
    }
  }
}

void ConnectionPool::releaseCapacity(std::string_view node_id, const Task& task) {
  auto* conn = getConnection(node_id);
  if (conn == nullptr) {
    return;
  }
  for (const auto& req : task.requirements) {
    if (req.type == "cpu") {
      conn->cpu_used -= std::min(conn->cpu_used, req.amount);
    } else if (req.type == "mem") {
      conn->mem_used -= std::min(conn->mem_used, req.amount);
    }
  }
}

void ConnectionPool::markDead(std::string_view node_id) {
  auto* conn = getConnection(node_id);
  if (conn != nullptr) {
    conn->alive = false;
  }
}

} // namespace carrot::common
