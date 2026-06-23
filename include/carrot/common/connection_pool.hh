#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "carrot/common/types.hh"

namespace carrot::common {

struct NodeConnection {
  std::string node_id;
  int fd{-1};
  bool alive{false};

  uint64_t cpu_used{0};
  uint64_t mem_used{0};

  struct Capacity {
    uint64_t cpu_max{0};
    uint64_t mem_max{0};
  } capacity;
};

class ConnectionPool {
public:
  struct Handler {
    virtual void onTaskResult(uint64_t task_id, Chunk chunk, bool is_final) = 0;
    virtual void onTaskError(uint64_t task_id, std::string_view message) = 0;
    virtual ~Handler() = default;
  };

  explicit ConnectionPool(Handler& handler) : handler_(handler) {}

  void addNode(const NodeInfo& info);
  void removeNode(std::string_view node_id);
  void updateNode(const NodeInfo& info);

  auto getConnection(std::string_view node_id) -> NodeConnection*;
  auto isAlive(std::string_view node_id) const -> bool;
  auto hasCapacity(std::string_view node_id, const Task& task) const -> bool;

  void reserveCapacity(std::string_view node_id, const Task& task);
  void releaseCapacity(std::string_view node_id, const Task& task);
  void markDead(std::string_view node_id);

private:
  Handler& handler_;
  std::unordered_map<std::string, NodeConnection> connections_;
};

} // namespace carrot::common
