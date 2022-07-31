#include "ir/opt/opt.hpp"
#include <iostream>

// inline son to fa
void move_func(IR::NormalFunc *fa, IR::CallInstr *son_call, 
    IR::BB *son_bb) {
    using namespace IR;
    struct InlineState {
        std::unordered_map<MemObject *, MemObject *> map_mm;
        int cnt = 0;
    };
    static std::unordered_map<NormalFunc *, InlineState> is;

    Case(NormalFunc, son_func, son_call->f) {
        // map local var to fa
           // move all local vars to fa
        if (!is.count(son_func)) {
            son_func->scope.for_each([&](MemObject *x0, MemObject *x1) {
                assert(!x1->global);
                if (x1->arg) {
                    delete x1;
                } else {
                    // alloc local vars
                    is[son_func].map_mm[x0] = x1;
                    fa->scope.add(x1);
                }
            });
        }
        std::unordered_map<BB *, BB *> map_bb;
        std::unordered_map<Reg, Reg> map_reg;
        std::unordered_map<MemObject *, MemObject *> &map_mm = is[son_func].map_mm;
        son_func->for_each([&](BB *bb) {
            map_bb[bb] = fa->new_BB();
            bb->for_each([&](Instr *instr) {
                Case(RegWriteInstr, rwinstr, instr) {
                    Reg r = rwinstr->d1;
                    assert(r.id);
                    if (!map_reg.count(r)){
                        // map_reg[r] = fa->new_Reg(son->get_name(r) + "_" + fa->name); }
                    }
                }
            });
        });

        BB *nxt = fa->new_BB();
    // nxt->instrs.splice(nxt->instrs.begin(), son_bb->instrs, ++son_instr_,
                    //    son->instrs.end());

        auto map_reg_f = [&](Reg &reg){ reg = map_reg.at(reg); };
        auto map_bb_f = [&](BB *&bb){ bb = map_bb.at(bb); };
        auto map_mem_f = [&](MemObject *&mm){ mm = map_mm.at(mm); };

        son_func->for_each([&](BB *bb) {
            // copy all BBs
            BB *bb1 = map_bb.at(bb);
            bb->for_each([&](Instr *instr) {
                Instr *instr1 = nullptr;
                Case(LocalVarDef, local_val_def, instr) {
                    // Todo ?
                    if (local_val_def->data->arg) return ;
                }
                Case(LoadArg, load_arg, instr) {
                    UnaryOpInstr *uo_instr = new UnaryOpInstr(load_arg->d1,
                        son_call->args.at(load_arg->id), UnaryOp::ID);
                    map_reg_f(uo_instr->d1);
                    instr1 = uo_instr;
                }
                /*else Case(ReturnInstr return_instr, instr) {
                    UnaryOpInstr *uo_instr = new UnaryOpInstr(return_instr->d1,
                        son_call->args.at(return_instr->id), UnaryOp::ID);
                    map_reg_f(uo_instr->d1);
                    bb1->push(uo_instr);
                    instr1 = new JumpInstr(nxt);
                }*/
                else {
                    instr1 = instr->map(map_reg_f, map_bb_f, map_mem_f);
                }
                bb1->push(instr1);
            });
            
        });
        son_bb->pop();
        son_bb->push(new JumpInstr(map_bb.at(son_func->entry)));
        son_bb = nxt;
        return ;
    }
}

void search_call_instr(IR::NormalFunc *func) {
    func->for_each([&](IR::BB *bb) {
        bb->for_each([&](IR::Instr *instr) {
            Case(IR::CallInstr, call, instr) {
                Case(IR::NormalFunc, func_t, call->f) {
                    if (func_t != func) {
                        move_func(func, call, bb);
                    }
                }
            }
        });
    });
}

void func_inline(IR::CompileUnit *ir){
    struct State {
        std::vector<IR::NormalFunc *> in_nodes;
        int count;
    };
    std::unordered_map<IR::NormalFunc *, State> map;
    ir->for_each([&](IR::NormalFunc *func) {
        std::ostream &os = std::cout;
        os << func->scope.name << ":\n";
        func->for_each([&](IR::BB *bb) {
            bb->for_each([&](IR::Instr *instr) {
                Case(IR::CallInstr, call_instr, instr){
                    Case(IR::NormalFunc, func_t, call_instr->f) {
                        map[func].count++;
                        map[func_t].in_nodes.push_back(func);
                    }
                }
            });
            bb->print(os);
        });
    });  
    /* 1. 拓扑排序，所有非循环函数进行内联，这样可以保证所有函数都被内联，但是会导致代码过于冗余；
       2. 迭代若干次，根据 call graph 的边的权值进行内联，目前是想直接根据指令数来决定。
    */
    std::vector<IR::NormalFunc *> order;
    for (auto &[func, state] : map) {
        if (state.count == 0) {
            order.push_back(func);
        }
    }  
    while(!order.empty()) {
        IR::NormalFunc *func = order.back();
        order.pop_back();

        search_call_instr(func);

        for (auto &in_node : map[func].in_nodes) {
            map[in_node].count--;
            if (map[in_node].count == 0) {
                order.push_back(in_node);
            }
        }
    }
}