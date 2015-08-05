#ifndef TURBO_SANTA_COMMON_BACK_END_MEMORY_TIMER_DIVIDER_FLAG_H_
#define TURBO_SANTA_COMMON_BACK_END_MEMORY_TIMER_DIVIDER_FLAG_H_

#include <cstdint>
#include "backend/memory/flags.h"
#include "submodules/glog/src/glog/logging.h"

namespace back_end {
namespace memory {
namespace timer {

class DividerFlag : public Flag {
 public:
  DividerFlag() : Flag(0xff04) {}

  void Write(uint16_t, uint8_t) override { set_flag(0); }

  void Tick(int ticks) {
    ticks_ += ticks;
    if (ticks >= kTicksToIncrement) {
      set_flag(flag() + 1);
      ticks_ = 0;
    }
  }

 private:
  static const int kTicksToIncrement = 256;
  int ticks_ = 0;
};

} // namespace timer
} // namespace memory
} // namespace back_end

#endif // TURBO_SANTA_COMMON_BACK_END_MEMORY_TIMER_DIVIDER_FLAG_H_