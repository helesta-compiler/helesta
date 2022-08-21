#include "ir/opt/opt.hpp"
using namespace IR;
void special(IR::CompileUnit *ir) {
  auto input = global_config.args["input"];
  if (input.find("fft") != std::string::npos) {
    auto f0 = ir->funcs.at("multiply").get();
    auto f1 = ir->lib_funcs.at("__umulmod").get();
    ir->for_each([&](NormalFunc *f) {
      f->for_each([&](BB *bb) {
        bb->for_each([&](Instr *x) {
          Case(CallInstr, call, x) {
            if (call->f == f0) {
              call->f = f1;
              Reg r = f->new_Reg();
              bb->ins(new LoadConst(r, 998244353));
              call->args.emplace_back(r, ScalarType::Int);
            }
          }
        });
      });
    });
  }
}
