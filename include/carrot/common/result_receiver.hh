#pragma once

#include "carrot/common/types.hh"

namespace carrot::common {

class ResultReceiver {
public:
  ResultReceiver() = default;
  virtual ~ResultReceiver() = default;

  ResultReceiver(const ResultReceiver&) = delete;
  auto operator=(const ResultReceiver&) -> ResultReceiver& = delete;
  ResultReceiver(ResultReceiver&&) noexcept = delete;
  auto operator=(ResultReceiver&&) noexcept -> ResultReceiver& = delete;

  virtual void sendChunk(Chunk chunk, bool is_final) = 0;
};

} // namespace carrot::common
