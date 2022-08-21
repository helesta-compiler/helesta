#include "ir/ir.hpp"

namespace IR {
void SIMDInstr::compute(typeless_scalar_t simd_regs[][4]) {
#define at_(j, type) simd_regs[regs[j]][i].type##_value()
  switch (type) {
  case VCVT_S32_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, float);
    }
    break;
  case VCVT_F32_S32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, int);
    }
    break;
  case VADD_I32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, int) + at_(2, int);
    }
    break;
  case VADD_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, float) + at_(2, float);
    }
    break;
  case VSUB_I32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, int) - at_(2, int);
    }
    break;
  case VSUB_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, float) - at_(2, float);
    }
    break;
  case VMUL_S32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) = at_(1, int) * at_(2, int);
    }
    break;
  case VMUL_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) = at_(1, float) * at_(2, float);
    }
    break;
  case VMLA_S32:
    for (int i = 0; i < 4; ++i) {
      at_(0, int) += at_(1, int) * at_(2, int);
    }
    break;
  case VMLA_F32:
    for (int i = 0; i < 4; ++i) {
      at_(0, float) += at_(1, float) * at_(2, float);
    }
    break;
  default:
    assert(0);
  }
#undef case_
}
} // namespace IR
