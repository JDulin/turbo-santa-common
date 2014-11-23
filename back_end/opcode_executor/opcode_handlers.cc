#include "back_end/opcode_executor/opcode_handlers.h"
#include "back_end/opcode_executor/opcode_executor.h"
#include "back_end/opcode_executor/opcodes.h"
#include <glog/logging.h>

namespace back_end {
namespace handlers {

using opcodes::Opcode;
using registers::GB_CPU;
using memory::MemoryMapper;

unsigned short* GetRegister16(MemoryMapper* memory_mapper, int instruction_ptr, unsigned char, GB_CPU* cpu) {
  unsigned char opcode = memory_mapper->Read(instruction_ptr);
  unsigned char register_index = (opcode >> 4);
  switch (register_index) {
    case 0:
      return &cpu->rBC;
    case 1:
      return &cpu->rDE;
    case 2:
      return &cpu->rHL;
    case 3:
      return &cpu->rSP;
    case 4:
      return &cpu->rAF;
    default:
      return &cpu->rBC;
  }
}

unsigned short GetRegisterValue16(MemoryMapper* memory_mapper, int instruction_ptr, unsigned char, GB_CPU* cpu) {
  unsigned short shortRegs[] = { cpu->rBC, cpu->rDE, cpu->rHL, cpu->rSP, cpu->rAF };
  unsigned char opcode = memory_mapper->Read(instruction_ptr);
  unsigned char register_index = (opcode >> 4);
  return shortRegs[register_index];
}

unsigned char GetParameterValue(MemoryMapper* memory_mapper, int instruction_ptr) {
  return memory_mapper->Read(instruction_ptr + 1);
}

unsigned short GetParameterValue16(MemoryMapper* memory_mapper, int instruction_ptr) {
  return (((short) memory_mapper->Read(instruction_ptr + 2)) << 8) | (short) memory_mapper->Read(instruction_ptr + 1);
}

unsigned short GetAddress16(MemoryMapper* memory_mapper, int instruction_ptr) {
  unsigned char upper = GetParameterValue(memory_mapper, instruction_ptr + 1);
  unsigned char lower = GetParameterValue(memory_mapper, instruction_ptr);
  unsigned short address = upper;
  address = address << 8;
  address |= lower;
  return address;
}

void SetZFlag(unsigned char register_value, GB_CPU* cpu) {
  cpu->flag_struct.rF.Z = register_value == 0;
}

void SetNFlag(bool performed_subtraction, GB_CPU* cpu) {
  cpu->flag_struct.rF.N = performed_subtraction;
}

unsigned char NthBit(unsigned int byte, int n) {
  return (byte >> n) & 1;
}

// TODO(Brendan): These opcodes are not in the same order as with ALU operations.
// 8-bit loads. Consider using opcodes::Opcode to address this.
int Ld8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  *reg = *opcode->reg2;
  return instruction_ptr + 2;
}

int Ld8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  *reg = GetParameterValue(context->memory_mapper, instruction_ptr);
  return instruction_ptr + 2;
}

int LoadToA8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA = *opcode->reg1;
  return instruction_ptr + 1;
}

int LoadToA8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = GetParameterValue(context->memory_mapper, instruction_ptr);
  return instruction_ptr + 2;
}

// See comment below about loading to an immediate.
int LoadToA16Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = GetRegisterValue16(context->memory_mapper, instruction_ptr, opcode.opcode_name, context->cpu);
  return instruction_ptr + 1;
}

int LoadToA16BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = GetParameterValue16(context->memory_mapper, instruction_ptr);
  return instruction_ptr + 3;
}

int LoadFromA8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  *reg = *opcode->reg2;
  return instruction_ptr + 2;
}

int LoadFromA16Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  *reg = *opcode->reg2;
  return instruction_ptr + 2;
}

// ALU.
bool DoesOverflow(unsigned int left, unsigned int right, int bit) {
  if (NthBit(left, bit) && NthBit(right, bit)) {
    return true;
  }
  if (NthBit(left, bit) || NthBit(right, bit)) {
    if (!NthBit(left + right, bit)) {
      return true;
    }
  }
  return false;
}

bool DoesUnderflow(unsigned int left, unsigned int right, int bit) {
  if (!NthBit(left, bit) && (NthBit(right, bit) || NthBit(left - right, bit))) {
    return true;
  }
  return false;
}

bool DoesBorrow8(unsigned char left, unsigned char right) {
  return DoesUnderflow(left, right, 7);
}

bool DoesBorrow16(unsigned short left, unsigned short right) {
  return DoesUnderflow(left, right, 15);
}

bool DoesHalfBorrow8(unsigned char left, unsigned char right) {
  return DoesUnderflow(left, right, 3);
}

bool DoesHalfBorrow16(unsigned short left, unsigned short right) {
  return DoesUnderflow(left, right, 11);
}

bool DoesHalfCarry8(unsigned char left, unsigned char right) {
  return DoesOverflow(left, right, 3);
}

bool DoesCarry8(unsigned char left, unsigned char right) {
  return DoesOverflow(left, right, 7);
}

bool DoesHalfCarry16(unsigned char left, unsigned char right) {
  return DoesOverflow(left, right, 11);
}

bool DoesCarry16(unsigned int left, unsigned int right) {
  return DoesOverflow(left, right, 15);
}

void Add8BitImpl(unsigned char value, GB_CPU* cpu) {
  cpu->flag_struct.rF.C = DoesCarry8(cpu->flag_struct.rA, value);
  cpu->flag_struct.rF.H = DoesHalfCarry8(cpu->flag_struct.rA, value);
  cpu->flag_struct.rA += value;
  SetZFlag(cpu->flag_struct.rA, cpu);
  SetNFlag(false, cpu);
}

int Add8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  Add8BitImpl(*opcode.reg1, context->cpu);
  return instruction_ptr + 1;
}

int Add8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  Add8BitImpl(context->memory_mapper->Read(*opcode.reg1), context->cpu);
  return instruction_ptr + 1;
}

int Add8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  Add8BitImpl(GetParameterValue(context->memory_mapper, instruction_ptr), context->cpu);
  return instruction_ptr + 2;
}

void ADC8BitImpl(unsigned char value, GB_CPU* cpu) {
  char carry = cpu->flag_struct.rF.C;
  cpu->flag_struct.rF.C = DoesCarry8(cpu->flag_struct.rA, value + carry);
  cpu->flag_struct.rF.H = DoesHalfCarry8(cpu->flag_struct.rA, value + carry);
  cpu->flag_struct.rA += value;
  cpu->flag_struct.rA += carry;
  SetZFlag(cpu->flag_struct.rA, cpu);
  SetNFlag(false, cpu);
}

int ADC8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  ADC8BitImpl(*opcode->reg1, context->cpu);
  return instruction_ptr + 1;
}

int ADC8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  ADC8BitImpl(context->memory_mapper->Read(*opcode->reg1), context->cpu);
  return instruction_ptr + 1;
}

int ADC8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  ADC8BitImpl(GetParameterValue(context->memory_mapper, instruction_ptr), context->cpu);
  return instruction_ptr + 2;
}

void Sub8BitImpl(unsigned char value, GB_CPU* cpu) {
  cpu->flag_struct.rF.C = !DoesBorrow8(cpu->flag_struct.rA, value);
  cpu->flag_struct.rF.H = !DoesHalfBorrow8(cpu->flag_struct.rA, value);
  cpu->flag_struct.rA -= value;
  SetZFlag(cpu->flag_struct.rA, cpu);
  SetNFlag(true, cpu);
}

int Sub8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  Sub8BitImpl(*opcode->reg1, context->cpu);
  return instruction_ptr + 1;
}

int Sub8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  Sub8BitImpl(context->memory_mapper->Read(*opcode->reg1), context->cpu);
  return instruction_ptr + 1;
}
  
int Sub8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  Sub8BitImpl(GetParameterValue(context->memory_mapper, instruction_ptr), context->cpu);
  return instruction_ptr + 2;
}

void SBC8BitImpl(unsigned char value, GB_CPU* cpu) {
    char carry = cpu->flag_struct.rF.C;
    cpu->flag_struct.rF.C = !DoesBorrow8(cpu->flag_struct.rA, value + carry);
    cpu->flag_struct.rF.H = !DoesHalfBorrow8(cpu->flag_struct.rA, value + carry);
    cpu->flag_struct.rA -= value;
    cpu->flag_struct.rA -= carry;
    SetZFlag(cpu->flag_struct.rA, cpu);
    SetNFlag(true, cpu);
}

int SBC8Bit(handlers::ExecutorContext* context) {
    SBC8BitImpl(*context->opcode->reg1, context->cpu);
    return *context->instruction_ptr + 1;
}

int SBC8BitAddress(handlers::ExecutorContext* context) {
  SBC8BitImpl(context->memory_mapper->Read(*context->opcode->reg1), context->cpu);
  return *context->instruction_ptr + 1;
}

int SBC8BitLiteral(handlers::ExecutorContext* context) {
    SBC8BitImpl(GetParameterValue(context->memory_mapper, *context->instruction_ptr), context->cpu);
    return *context->instruction_ptr + 2;
}

int And8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA &= *opcode->reg1;
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}
  
int And8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA &= context->memory_mapper->Read(*opcode->reg1);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int And8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA &= GetParameterValue(context->memory_mapper, instruction_ptr);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int Or8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA |= *opcode->reg1;
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int Or8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA |= context->memory_mapper->Read(*opcode->reg1);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}
  
int Or8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA |= GetParameterValue(context->memory_mapper, instruction_ptr);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int Xor8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA ^= *opcode->reg1;
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int Xor8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  context->cpu->flag_struct.rA ^= context->memory_mapper->Read(*opcode->reg1);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int Xor8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA ^= GetParameterValue(context->memory_mapper, instruction_ptr);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr + 1;
}

int Cp8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char result = context->cpu->flag_struct.rA - *opcode->reg1;;
  SetZFlag(result, context->cpu);
  SetNFlag(true, context->cpu); // Performed subtraction.
  context->cpu->flag_struct.rF.H = NthBit(context->cpu->flag_struct.rA, 4) != NthBit(result, 4);
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 7) != NthBit(result, 7);
  return instruction_ptr + 1;
}

int Cp8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char result = context->cpu->flag_struct.rA - context->memory_mapper->Read(*opcode->reg1);
  SetZFlag(result, context->cpu);
  SetNFlag(true, context->cpu); // Performed subtraction.
  context->cpu->flag_struct.rF.H = NthBit(context->cpu->flag_struct.rA, 4) != NthBit(result, 4);
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 7) != NthBit(result, 7);
  return instruction_ptr + 1;
}
  
  
int Cp8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char result = context->cpu->flag_struct.rA - GetParameterValue(context->memory_mapper, instruction_ptr);
  SetZFlag(result, context->cpu);
  SetNFlag(true, context->cpu); // Performed subtraction.
  context->cpu->flag_struct.rF.H = NthBit(context->cpu->flag_struct.rA, 4) != NthBit(result, 4);
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 7) != NthBit(result, 7);
  return instruction_ptr + 1;
}

int Inc8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char* reg = (unsigned char*) opcode.reg1;
  unsigned char fourth_bit = NthBit(*reg, 3);
  ++(*reg);
  bool borrowed_h = fourth_bit != NthBit(*reg, 3);
  SetZFlag(*reg, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = borrowed_h;
  return instruction_ptr + 1;
}

int Inc8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char val = context->memory_mapper->Read(*opcode.reg1);
  context->memory_mapper->Write(*opcode.reg1, ++val);
  unsigned char forth_bit = NthBit(val, 3);
  bool borrowed_h = forth_bit != NthBit(val, 3);
  SetZFlag(val, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = borrowed_h;
  return instruction_ptr + 1;
}
  
int Dec8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char* reg = (unsigned char*) opcode.reg1;
  unsigned char fourth_bit = NthBit(*reg, 4);
  --(*reg);
  bool borrowed_h = fourth_bit != NthBit(*reg, 4);
  SetZFlag(*reg, context->cpu);
  SetNFlag(true, context->cpu);
  context->cpu->flag_struct.rF.H = borrowed_h;
  return instruction_ptr + 1;
}

int Dec8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char val = context->memory_mapper->Read(*opcode.reg1);
  context->memory_mapper->Write(*opcode.reg1, --val);
  unsigned char fourth_bit = NthBit(val, 4);
  bool borrowed_h = fourth_bit != NthBit(val, 4);
  SetZFlag(val, context->cpu);
  SetNFlag(true, context->cpu);
  context->cpu->flag_struct.rF.H = borrowed_h;
  return instruction_ptr + 1;
}
  
int Add16Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  short value = GetRegisterValue16(context->memory_mapper, instruction_ptr, opcode.opcode_name, context->cpu);
  context->cpu->flag_struct.rF.H = DoesHalfCarry16(context->cpu->rHL, value);
  context->cpu->flag_struct.rF.C = DoesCarry16(context->cpu->rHL, value);
  context->cpu->rHL += value;
  SetNFlag(false, context->cpu);
  return instruction_ptr + 1;
}

int AddSPLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char value = GetParameterValue(context->memory_mapper, instruction_ptr);
  if (NthBit(value, 7)) {
    context->cpu->flag_struct.rF.H = DoesHalfBorrow16(context->cpu->rSP, value);
    context->cpu->flag_struct.rF.C = DoesBorrow16(context->cpu->rSP, value);
  } else {
    context->cpu->flag_struct.rF.H = DoesHalfCarry16(context->cpu->rSP, value);
    context->cpu->flag_struct.rF.C = DoesCarry16(context->cpu->rSP, value);
  }
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.Z = 0;
  context->cpu->rSP += value;
  return instruction_ptr + 2;
}

int Inc16Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned short* reg = GetRegister16(context->memory_mapper, instruction_ptr, opcode.opcode_name, context->cpu);
  *reg += 1;
  return instruction_ptr + 1;
}

int Dec16Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned short* reg = GetRegister16(context->memory_mapper, instruction_ptr, opcode.opcode_name, context->cpu);
  *reg -= 1;
  return instruction_ptr + 1;
}

int Swap(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  unsigned char value = *reg;
  *reg = (value << 4) | (value >> 4);
  SetZFlag(*reg, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return instruction_ptr;
}
  
int SwapAddress(handlers::ExecutorContext* context) {
  unsigned char val = context->memory_mapper->Read(*context->opcode->reg1);
  unsigned char swapVal = (val << 4) | (val >> 4);
  context->memory_mapper->Write(*context->opcode->reg1, swapVal);
  SetZFlag(swapVal, context->cpu);
  SetNFlag(false, context->cpu);
  context->cpu->flag_struct.rF.H = 0;
  context->cpu->flag_struct.rF.C = 0;
  return *context->instruction_ptr;
}

int DAA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;

  unsigned char mul = 0x6;
  unsigned char sum = context->cpu->flag_struct.rA;
  unsigned char n = sum / 10;
  while (n > 0) {
    sum += n * mul;
    n /= 10;
    mul *= 0x0F;
  }
  context->cpu->flag_struct.rF.C = context->cpu->flag_struct.rA > 0x64;
  context->cpu->flag_struct.rA = sum;
  context->cpu->flag_struct.rF.H = 0;
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  return instruction_ptr;
}

int CPL(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = ~context->cpu->flag_struct.rA;
  context->cpu->flag_struct.rF.H = 1;
  SetNFlag(true, context->cpu);
  return instruction_ptr + 1;
}

int CCF(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = ~context->cpu->flag_struct.rA;
  context->cpu->flag_struct.rF.C = !context->cpu->flag_struct.rF.C;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  return instruction_ptr + 1;
}

int SCF(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rF.C = 1;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  return instruction_ptr + 1;
}

int NOP(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  return instruction_ptr + 1;
}

int Halt(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  // TODO: We should actually halt instead of just nop
  return instruction_ptr + 1;
}

int Stop(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  // TODO: We should actually stop instead of just nop
  return instruction_ptr + 1;
}

int DI(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  *context->interrupt_master_enable = false;
  return instruction_ptr + 1;
}

int EI(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  *context->interrupt_master_enable = true;
  return instruction_ptr + 1;
}

int RLCA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 7);
  context->cpu->flag_struct.rA = context->cpu->flag_struct.rA << 1;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  return instruction_ptr + 1;
}

int RLA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char carry = context->cpu->flag_struct.rF.C;
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 7);
  context->cpu->flag_struct.rA = context->cpu->flag_struct.rA << 1;
  context->cpu->flag_struct.rA |= carry;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  return instruction_ptr + 1;
}

int RRCA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 0);
  context->cpu->flag_struct.rA = context->cpu->flag_struct.rA >> 1;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  return instruction_ptr + 1;
}

int RRA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rF.C = NthBit(context->cpu->flag_struct.rA, 0);
  context->cpu->flag_struct.rA = context->cpu->flag_struct.rA >> 1;
  context->cpu->flag_struct.rA |= (((unsigned char) context->cpu->flag_struct.rF.C) << 7);
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(context->cpu->flag_struct.rA, context->cpu);
  return instruction_ptr + 1;
}

int RLC(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  context->cpu->flag_struct.rF.C = NthBit(*reg, 7);
  *reg = *reg << 1;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(*reg, context->cpu);
  return instruction_ptr + 2;
}

int RLCAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char value = context->memory_mapper->Read(*opcode->reg1);
  context->cpu->flag_struct.rF.C = NthBit(value, 7);
  value = value << 1;
  context->memory_mapper->Write(*opcode->reg1, value);
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(value, context->cpu);
  return instruction_ptr + 1;
}

int RL(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  context->cpu->flag_struct.rF.C = NthBit(*reg, 7);
  *reg = *reg << 1;
  *reg |= context->cpu->flag_struct.rF.C;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(*reg, context->cpu);
  return instruction_ptr + 2;
}

int RRC(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  context->cpu->flag_struct.rF.C = NthBit(*reg, 0);
  *reg = *reg >> 1;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(*reg, context->cpu);
  return instruction_ptr + 2;
}

int RR(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  context->cpu->flag_struct.rF.C = NthBit(*reg, 0);
  *reg = *reg >> 1;
  *reg |= (((unsigned char) context->cpu->flag_struct.rF.C) << 7);
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(*reg, context->cpu);
  return instruction_ptr + 2;
}

int SLA(handlers::ExecutorContext* context) {
  return RLC(context);
}

int SRA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  context->cpu->flag_struct.rF.C = NthBit(*reg, 0);
  unsigned char msb = NthBit(*reg, 7) << 7;
  *reg = *reg >> 1;
  *reg |= msb;
  context->cpu->flag_struct.rF.H = 0;
  SetNFlag(false, context->cpu);
  SetZFlag(*reg, context->cpu);
  return instruction_ptr + 2;
}

int SRL(handlers::ExecutorContext* context) {
  return RRC(context);
}

int Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  unsigned char bit = NthBit(*reg, GetParameterValue(context->memory_mapper, instruction_ptr));
  context->cpu->flag_struct.rF.H = 1;
  SetZFlag(bit, context->cpu);
  SetNFlag(false, context->cpu);
  return instruction_ptr + 2;
}

int Set(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  unsigned char bit = GetParameterValue(context->memory_mapper, instruction_ptr);
  *reg |= (0x1 << bit);
  return instruction_ptr + 2;
}

int Res(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode* opcode = context->opcode;
  unsigned char* reg = (unsigned char*) opcode->reg1;
  unsigned char bit = GetParameterValue(context->memory_mapper, instruction_ptr);
  *reg &= ~(0x1 << bit);
  return instruction_ptr + 2;
}


int Jump(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  instruction_ptr = GetAddress16(context->memory_mapper, instruction_ptr);
  return instruction_ptr;
}

int JumpConditional(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  switch (opcode.opcode_name) {
    case 0xC2:
      if (!context->cpu->flag_struct.rF.Z) {
        return Jump(context);
      }
      return instruction_ptr + 1;
    case 0xCA:
      if (context->cpu->flag_struct.rF.Z) {
        return Jump(context);
      }
      return instruction_ptr + 1;
    case 0xD2:
      if (!context->cpu->flag_struct.rF.C) {
        return Jump(context);
      }
      return instruction_ptr + 1;
    case 0xDA:
      if (context->cpu->flag_struct.rF.C) {
        return Jump(context);
      }
      return instruction_ptr + 1;
  }
  return instruction_ptr + 1;
}

int JumpHL(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  instruction_ptr = context->cpu->rHL;
  return instruction_ptr;
}

int JumpRelative(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  instruction_ptr += GetParameterValue(context->memory_mapper, instruction_ptr);
  return instruction_ptr;
}

int JumpRelativeConditional(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  switch (opcode.opcode_name) {
    case 0x20:
      if (!context->cpu->flag_struct.rF.Z) {
        return JumpRelative(context);
      }
      return instruction_ptr + 1;
    case 0x28:
      if (context->cpu->flag_struct.rF.Z) {
        return JumpRelative(context);
      }
      return instruction_ptr + 1;
    case 0x30:
      if (!context->cpu->flag_struct.rF.C) {
        return JumpRelative(context);
      }
      return instruction_ptr + 1;
    case 0x38:
      if (context->cpu->flag_struct.rF.C) {
        return JumpRelative(context);
      }
  }
  return instruction_ptr + 1;
}

// TODO(Brendan, Diego, Aaron, Dave): We should make sure we are doing endian
// specific work in functions that we can either swap out at compile time or at
// runtime to preserve correct endianness.
unsigned char GetLSB(unsigned short value) {
  return static_cast<unsigned char>(value);
}

unsigned char GetMSB(unsigned short value) {
  return static_cast<unsigned char>(value >> 8);
}

void PushRegister(MemoryMapper* memory_mapper, GB_CPU* cpu, unsigned short* reg) {
  unsigned short* rSP = &cpu->rSP;
  // memory_mapper->Write(0xfffe, GetLSB(*reg));
  memory_mapper->Write(*rSP, GetLSB(*reg));
  --*rSP;
  memory_mapper->Write(*rSP, GetMSB(*reg));
  --*rSP;
}

int Call(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  GB_CPU* cpu = context->cpu;
  unsigned short* rPC = &cpu->rPC;

  ++*rPC;
  PushRegister(context->memory_mapper, cpu, rPC);

  unsigned short address = GetParameterValue16(context->memory_mapper, instruction_ptr);
  instruction_ptr = address;
  return instruction_ptr;
}

int CallConditional(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  switch (opcode.opcode_name) {
    case 0xC4:
      if (!context->cpu->flag_struct.rF.Z) {
        return Call(context);
      }
      return instruction_ptr + 1;
    case 0xCC:
      if (context->cpu->flag_struct.rF.Z) {
        return Call(context);
      }
      return instruction_ptr + 1;
    case 0xD4:
      if (!context->cpu->flag_struct.rF.C) {
        return Call(context);
      }
      return instruction_ptr + 1;
    case 0xDC:
      if (context->cpu->flag_struct.rF.C) {
        return Call(context);
      }
      return instruction_ptr + 1;
  }
  return instruction_ptr + 1;
}

int Restart(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char accum = 0;
  switch (opcode.opcode_name) {
    case 0xFF:
      accum += 0x08;
    case 0xF7:
      accum += 0x08;
    case 0xEF:
      accum += 0x08;
    case 0xE7:
      accum += 0x08;
    case 0xDF:
      accum += 0x08;
    case 0xD7:
      accum += 0x08;
    case 0xCF:
      accum += 0x08;
    case 0xC7:
      instruction_ptr = accum;
  }
  return instruction_ptr;
}

int Return(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned short address = 0; // = context->cpu->rSP[0];
  context->cpu->rSP -= 2;
  instruction_ptr = address;
  return instruction_ptr;
}

int ReturnConditional(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  switch (opcode.opcode_name) {
    case 0xC0:
      if (!context->cpu->flag_struct.rF.Z) {
        return Return(context);
      }
      return instruction_ptr + 1;
    case 0xC8:
      if (context->cpu->flag_struct.rF.Z) {
        return Return(context);
      }
      return instruction_ptr + 1;
    case 0xD0:
      if (!context->cpu->flag_struct.rF.C) {
        return Return(context);
      }
      return instruction_ptr + 1;
    case 0xD8:
      if (context->cpu->flag_struct.rF.C) {
        return Return(context);
      }
  }
  return instruction_ptr + 1;
}

int ReturnInterrupt(handlers::ExecutorContext* context) {
  unsigned short instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  // TODO(Brendan, Diego): Implement call stack.
  unsigned short address = 0; // = context->cpu->rSP[0];
  context->cpu->rSP -= 2;
  instruction_ptr = address;
  ExecutorContext new_context(context);
  new_context.instruction_ptr = &instruction_ptr;
  EI(&new_context);
  return instruction_ptr;
}

int LoadN(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char value = GetParameterValue(context->memory_mapper, instruction_ptr);
  *opcode.reg1 = value;
  return instruction_ptr + 2;
}

int LoadRR8Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char* reg1 = (unsigned char*)opcode.reg1;
  unsigned char* reg2 = (unsigned char*)opcode.reg2;
  *reg1 = *reg2;
  return instruction_ptr + 1;
}

int LoadRR8BitAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned char* reg1 = (unsigned char*)opcode.reg1;
  *reg1 = context->memory_mapper->Read(*opcode.reg2);
  return instruction_ptr + 1;
}

int LoadRR8BitIntoAddress(handlers::ExecutorContext* context) {
  Opcode opcode = *context->opcode;
  context->memory_mapper->Write(*opcode.reg1, *opcode.reg2);
  return *context->instruction_ptr + 1;
}
  
int Load8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned short val = (unsigned short)GetParameterValue(context->memory_mapper, instruction_ptr);
  context->memory_mapper->Write(*opcode.reg1, val);
  return instruction_ptr + 2;

}
  
int LoadRR16Bit(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  *opcode.reg1 = *opcode.reg2;
  return instruction_ptr + 1;
}

int LoadAN(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = context->memory_mapper->Read(*opcode.reg1);
  return instruction_ptr + 1;
}

int LoadAN16BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  unsigned short address = GetParameterValue16(context->memory_mapper, instruction_ptr);
  context->cpu->flag_struct.rA = context->memory_mapper->Read(address);
  return instruction_ptr + 3;
}
  
int LoadAN8BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = GetParameterValue(context->memory_mapper, instruction_ptr);
  return instruction_ptr + 2;
}

int LoadNA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  *opcode.reg1 = (unsigned char)context->cpu->flag_struct.rA;
  return instruction_ptr + 1;
}

int LoadNAAddress(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->memory_mapper->Write(*opcode.reg1, context->cpu->flag_struct.rA);
  return instruction_ptr + 1;
}
  
int LoadNA16BitLiteral(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  unsigned short address = GetParameterValue16(context->memory_mapper, instruction_ptr);
  context->memory_mapper->Write(address, context->cpu->flag_struct.rA);
  return instruction_ptr + 3;
}

int LoadAC(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->flag_struct.rA = context->memory_mapper->Read(0xFF00 + context->cpu->bc_struct.rC);
  return instruction_ptr + 1;
}

int LoadCA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->memory_mapper->Write(0xFF00 + context->cpu->bc_struct.rC, context->cpu->flag_struct.rA);
  return instruction_ptr + 1;
}

int LoadDecAHL(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  context->cpu->flag_struct.rA = context->memory_mapper->Read(context->cpu->rHL);
  context->cpu->rHL--;
  return instruction_ptr + 1;
}

int LoadDecHLA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  context->memory_mapper->Write(context->cpu->rHL, context->cpu->flag_struct.rA);
  context->cpu->rHL--;
  return instruction_ptr + 1;
}

int LoadIncAHL(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  context->cpu->flag_struct.rA = context->memory_mapper->Read(context->cpu->rHL);
  context->cpu->rHL++;
  return instruction_ptr + 1;
}

int LoadIncHLA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  context->memory_mapper->Write(context->cpu->rHL, context->cpu->flag_struct.rA);
  context->cpu->rHL++;
  return instruction_ptr + 1;
}

int LoadHNA(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  MemoryMapper* memory_mapper = context->memory_mapper;
  memory_mapper->Write(0xFF00 + GetParameterValue(memory_mapper, instruction_ptr), context->cpu->flag_struct.rA);
  return instruction_ptr + 2;
}

int LoadHAN(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  MemoryMapper* memory_mapper = context->memory_mapper;
  context->cpu->flag_struct.rA = memory_mapper->Read(0xFF00 + GetParameterValue(memory_mapper, instruction_ptr));
  return instruction_ptr + 2;
}

int LoadNN(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  *opcode.reg1 = GetParameterValue16(context->memory_mapper, instruction_ptr);
  return instruction_ptr + 3;
}

int LoadSPHL(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->cpu->rSP = context->cpu->rHL;
  return instruction_ptr + 1;
}

int LoadHLSP(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  signed char val = (signed) GetParameterValue(context->memory_mapper, instruction_ptr);
  if (val < 0) {
    context->cpu->flag_struct.rF.H = DoesHalfBorrow8(context->cpu->rSP, val);
    context->cpu->flag_struct.rF.C = DoesBorrow8(context->cpu->rSP, val);
  } else {
    context->cpu->flag_struct.rF.H = DoesHalfCarry8(context->cpu->rSP, val);
    context->cpu->flag_struct.rF.C = DoesCarry8(context->cpu->rSP, val);
  }
  context->cpu->rHL = context->cpu->rSP + val;
  context->cpu->flag_struct.rF.Z = 0;
  SetNFlag(false, context->cpu);

  return instruction_ptr + 1;
}

int LoadNNSP(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  MemoryMapper* memory_mapper = context->memory_mapper;
  unsigned short address = GetParameterValue16(memory_mapper, instruction_ptr);
  unsigned char lsb = (unsigned char)(0x00FF & context->cpu->rSP);
  unsigned char msb = (unsigned char)((0xFF00 & context->cpu->rSP) >> 8);
  memory_mapper->Write(address++, lsb);
  memory_mapper->Write(address, msb);
  return instruction_ptr + 2;
}

int Push(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  context->memory_mapper->Write(context->cpu->rSP, *opcode.reg1);
  context->cpu->rSP -= 2;
  return instruction_ptr + 1;
}

int Pop(handlers::ExecutorContext* context) {
  int instruction_ptr = *context->instruction_ptr;
  Opcode opcode = *context->opcode;
  MemoryMapper* memory_mapper = context->memory_mapper;
  *opcode.reg1 = (((short)memory_mapper->Read(context->cpu->rSP)) << 8) | memory_mapper->Read(context->cpu->rSP + 1); 
  context->cpu->rSP += 2;
  return instruction_ptr + 1;
}

} // namespace handlers
} // namespace back_end
