#ifndef TURBO_SANTA_COMMON_BACK_END_OPCODE_PARSER_H_
#define TURBO_SANTA_COMMON_BACK_END_OPCODE_PARSER_H_

#include <memory>

#include "back_end/memory/memory_mapper.h"
#include "back_end/opcode_executor/opcode_handlers.h"
#include "back_end/opcode_executor/opcodes.h"
#include "back_end/opcode_executor/opcode_map.h"
#include "back_end/opcode_executor/registers.h"

namespace test_harness {
class TestHarness;
} // namespace test_harness

namespace back_end {
namespace clocktroller {
class ClocktrollerTest;
} // namespace clocktroller
} // namespace back_end

namespace back_end {
namespace handlers {

class OpcodeExecutor {
  public: 
    OpcodeExecutor(unsigned char* rom, unsigned long rom_size);
    unsigned int ReadInstruction();

  private:
    memory::MemoryMapper memory_mapper_;
    registers::GB_CPU cpu_;

    friend class test_harness::TestHarness;
    friend class back_end::clocktroller::ClocktrollerTest;
};

struct ExecutorContext {
  ExecutorContext(unsigned short* instruction_ptr_, 
                  Opcode* opcode_, 
                  MemoryMapper* memory_mapper_, 
                  registers::GB_CPU* cpu_)
      : instruction_ptr(instruction_ptr_),
      opcode(opcode_),
      memory_mapper(memory_mapper_), 
      cpu(cpu_) {}

  ExecutorContext(ExecutorContext* context) : 
      instruction_ptr(context->instruction_ptr),
      opcode(context->opcode),
      memory_mapper(context->memory_mapper),
      cpu(context->cpu) {}

  unsigned short* instruction_ptr;
  Opcode* opcode;
  MemoryMapper* memory_mapper;
  registers::GB_CPU* cpu;
};


} // namespace handlers
} // namespace back_end
#endif // TURBO_SANTA_COMMON_BACK_END_OPCODE_PARSER_H_
