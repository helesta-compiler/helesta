#pragma once

#include <array>
#include <cstdint>

#include "common/common.hpp"

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

enum class RegisterUsage { caller_save, callee_save, special };

template <ScalarType scalar> struct RegConvention;

template <> struct RegConvention<ScalarType::Int> {
  static constexpr int Count = 16;
  static constexpr int ALLOCABLE_REGISTER_COUNT = 14;
  static constexpr int ARGUMENT_REGISTER_COUNT = 4;
  static constexpr int ARGUMENT_REGISTERS[ARGUMENT_REGISTER_COUNT] = {r0, r1,
                                                                      r2, r3};
  static constexpr RegisterUsage REGISTER_USAGE[Count] = {
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save, // r0...r3
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save,                             // r4...r8
      RegisterUsage::callee_save,                             // r9
      RegisterUsage::callee_save, RegisterUsage::callee_save, // r10, r11
      RegisterUsage::caller_save, RegisterUsage::special,
      RegisterUsage::callee_save, RegisterUsage::special // r12...r15
  };
  static constexpr bool allocable(int reg_id) {
    return REGISTER_USAGE[reg_id] == RegisterUsage::caller_save ||
           REGISTER_USAGE[reg_id] == RegisterUsage::callee_save;
  }
};

template <> struct RegConvention<ScalarType::Float> {
  static constexpr int Count = 32;
  static constexpr int ALLOCABLE_REGISTER_COUNT = 28;
  static constexpr int ARGUMENT_REGISTER_COUNT = 4;
  static constexpr int ARGUMENT_REGISTERS[ARGUMENT_REGISTER_COUNT] = {0, 1, 2,
                                                                      3};
  static constexpr RegisterUsage REGISTER_USAGE[Count] = {
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save, // s0..s3
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save, RegisterUsage::callee_save,
      RegisterUsage::callee_save, RegisterUsage::callee_save, // s4..s15
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save,
      RegisterUsage::caller_save, RegisterUsage::caller_save, // s16..s27
      RegisterUsage::special,     RegisterUsage::special,
      RegisterUsage::special,     RegisterUsage::special, // s28..s31
  };
  static constexpr bool allocable(int reg_id) {
    return REGISTER_USAGE[reg_id] == RegisterUsage::caller_save ||
           REGISTER_USAGE[reg_id] == RegisterUsage::callee_save;
  }
};

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
