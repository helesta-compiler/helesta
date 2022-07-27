#pragma once

#include <array>
#include <cstdint>

namespace ARMv7 {

constexpr int r0 = 0;
constexpr int r1 = 1;
constexpr int r2 = 2;
constexpr int r3 = 3;
constexpr int r4 = 4;
constexpr int r5 = 5;
constexpr int r6 = 6;
constexpr int r7 = 7;
constexpr int r8 = 8;
constexpr int r9 = 9;
constexpr int r10 = 10;
constexpr int r11 = 11;
constexpr int r12 = 12;
constexpr int r13 = 13;
constexpr int r14 = 14;
constexpr int r15 = 15;
constexpr int ip = r12;
constexpr int sp = r13;
constexpr int lr = r14;
constexpr int pc = r15;
constexpr int s0 = 0;
constexpr int s1 = 1;
constexpr int s2 = 2;
constexpr int s3 = 3;
constexpr int s4 = 4;
constexpr int s5 = 5;
constexpr int s6 = 6;
constexpr int s7 = 7;
constexpr int s8 = 8;
constexpr int s9 = 9;
constexpr int s10 = 10;
constexpr int s11 = 11;
constexpr int s12 = 12;
constexpr int s13 = 13;
constexpr int s14 = 14;
constexpr int s15 = 15;

constexpr int RegCount = 16;
constexpr int FloatRegCount = 16;
// machine register in 0...RegCount-1
// if an operand register >= RegCount, it's a pseudoregister

enum RegisterUsage { caller_save, callee_save, special };

constexpr RegisterUsage REGISTER_USAGE[RegCount] = {
    caller_save, caller_save, caller_save, caller_save,              // r0...r3
    callee_save, callee_save, callee_save, callee_save, callee_save, // r4...r8
    callee_save,                                                     // r9
    callee_save, callee_save,                                        // r10, r11
    caller_save, special,     callee_save, special // r12...r15
};

constexpr RegisterUsage FLOAT_REGISTER_USAGE[FloatRegCount] = {
    caller_save, caller_save, caller_save, caller_save,              // s0...s3
    callee_save, callee_save, callee_save, callee_save, callee_save, // s4...s8
    callee_save,                                                     // s9
    callee_save, callee_save,                                        // s10, s11
    caller_save, callee_save, callee_save, callee_save // s12...s15
};                                                     // TODO: double check

constexpr bool allocable(int reg_id, bool is_float = 0) {
  if (is_float) {
    return true; // TODO: double check
  } else {
    return REGISTER_USAGE[reg_id] == caller_save ||
           REGISTER_USAGE[reg_id] == callee_save;
  }
}

constexpr int ALLOCABLE_REGISTER_COUNT = []() constexpr {
  int cnt = 0;
  for (int i = 0; i < RegCount; ++i)
    if (allocable(i))
      ++cnt;
  return cnt;
}
();

constexpr int ALLOCABLE_FLOAT_REGISTER_COUNT = []() constexpr {
  int cnt = 0;
  for (int i = 0; i < FloatRegCount; ++i)
    if (allocable(i, true))
      ++cnt;
  return cnt;
}
();

constexpr std::array<int, ALLOCABLE_REGISTER_COUNT> ALLOCABLE_REGISTERS =
    []() constexpr {
  std::array<int, ALLOCABLE_REGISTER_COUNT> ret{};
  int cnt = 0;
  for (int i = 0; i < RegCount; ++i)
    if (allocable(i))
      ret[cnt++] = i;
  return ret;
}
();

constexpr std::array<int, ALLOCABLE_FLOAT_REGISTER_COUNT>
    ALLOCABLE_FLOAT_REGISTERS = []() constexpr {
  std::array<int, ALLOCABLE_FLOAT_REGISTER_COUNT> ret{};
  int cnt = 0;
  for (int i = 0; i < FloatRegCount; ++i)
    if (allocable(i, true))
      ret[cnt++] = i;
  return ret;
}
();

constexpr int ARGUMENT_REGISTER_COUNT = 4;

constexpr int ARGUMENT_REGISTERS[ARGUMENT_REGISTER_COUNT] = {r0, r1, r2, r3};

constexpr bool is_legal_immediate(int32_t value) {
  uint32_t u = static_cast<uint32_t>(value);
  if (u <= 0xffu)
    return true;
  for (int i = 1; i < 16; ++i) {
    uint32_t cur = (u << (2 * i)) | (u >> (32 - 2 * i));
    if (cur <= 0xffu)
      return true;
  }
  return false;
}

} // namespace ARMv7