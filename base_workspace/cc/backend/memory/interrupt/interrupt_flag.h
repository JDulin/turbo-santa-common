#ifndef TURBO_SANTA_COMMON_BACK_END_MEMORY_INTERRUPT_FLAG_H_
#define TURBO_SANTA_COMMON_BACK_END_MEMORY_INTERRUPT_FLAG_H_

#include "cc/backend/memory/flags.h"
#include "glog/logging.h"

namespace backend {
namespace memory {
class InterruptBase : public Flag {
 private:
  void set_value_bit(int bit, bool value) {
    if (value) {
      unsigned char mask = 0b00000001;
      mask <<= bit;
      value_ |= mask;
    } else {
      unsigned char mask = 0b00000001;
      mask <<= bit;
      value_ &= ~mask;
    }
  }

  bool value_bit(int bit) {
    unsigned char mask = 0b00000001;
    mask <<= bit;
    unsigned char bit_value = value_ & mask;
    return bit_value != 0;
  }

  unsigned char value_ = 0x00;

 public:
  InterruptBase(unsigned short address) : Flag(address) {}

  virtual unsigned char Read(unsigned short) { return value_; }
  virtual void Write(unsigned short, unsigned char value) { 
    LOG(INFO) << "Interrupt flag written to.";
    LOG(INFO) << "v_blank = " << v_blank();
    LOG(INFO) << "lcd_stat = " << lcd_stat();
    LOG(INFO) << "timer = " << timer();
    LOG(INFO) << "serial = " << serial();
    LOG(INFO) << "joypad = " << joypad();
    value_ = value;
    LOG(INFO) << "v_blank = " << v_blank();
    LOG(INFO) << "lcd_stat = " << lcd_stat();
    LOG(INFO) << "timer = " << timer();
    LOG(INFO) << "serial = " << serial();
    LOG(INFO) << "joypad = " << joypad();
  }
  virtual bool v_blank() { return value_bit(0); }
  virtual bool lcd_stat() { return value_bit(1); }
  virtual bool timer() { return value_bit(2); }
  virtual bool serial() { return value_bit(3); }
  virtual bool joypad() { return value_bit(4); }
  virtual void set_v_blank(bool value) { set_value_bit(0, value); }
  virtual void set_lcd_stat(bool value) { set_value_bit(1, value); }
  virtual void set_timer(bool value) { set_value_bit(2, value); }
  virtual void set_serial(bool value) { set_value_bit(3, value); }
  virtual void set_joypad(bool value) { set_value_bit(4, value); }
  virtual void Clear() { Write(address(), 0x00); }
};

class InterruptFlag : public InterruptBase {
 public:
  InterruptFlag() : InterruptBase(0xff0f) {}
};

class InterruptEnable : public InterruptBase {
 public:
  InterruptEnable() : InterruptBase(0xffff) {}
};
} // namespace memory
} // namespace backend

#endif // TURBO_SANTA_COMMON_BACK_END_MEMORY_INTERRUPT_FLAG_H_
