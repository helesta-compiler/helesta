#include "ir/opt/opt.hpp"
#include <iostream>
#include <unordered_map>
#include <vector>

// inline son to fa
void move_func(IR::NormalFunc *fa, IR::NormalFunc *son_func, 
    IR::BB *son_bb) {
    // To Do
    using namespace IR;
    // move all local vars to fa
    son_func->scope.for_each([&](MemObject *x0, MemObject *x1) {
        assert(!x1->global);
        if (x1->arg) {
        delete x1;
        } else {
        // alloc local vars
        fa->scope.add(x1);
        }
    });

    // alloc BBs
    std::unordered_map<BB *, BB *> map_bb;
    std::unordered_map<Reg, Reg> map_reg;
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

    // move all bb to fa
    std::vector<IR::BB *> _bbs;
    son_func->for_each([&](IR::BB *_bb) { _bbs.push_back(_bb); });
    std::for_each(_bbs.cbegin(), _bbs.cend(), [&](IR::BB *_bb) {
        for (IR::BB *bb = _bb;;) {
        for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
            Case(IR::CallInstr, call, it->get()) {
            Case(IR::NormalFunc, g, call->f) {
                if (g != son) {
                fa->add(bb);
                break;
                }
            }
            }
        }
        if (bb->succ.empty()) {
            break;
        }
        bb = bb->succ.front();
        }
    });
}

void search_call_instr(IR::NormalFunc *func) {
    func->for_each([&](IR::BB *bb) {
        bb->for_each([&](IR::Instr *instr) {
            Case(IR::CallInstr, call, instr) {
                Case(IR::NormalFunc, func_t, call->f) {
                    if (func_t != func) {
                        move_func(func, func_t, bb);
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
    /* 1. 拓扑排序，所有非循环函数进行内联，这样可以保证所有函数都被内联，但是会导致函数的调用次数增加；
       2. 迭代若干次，根据 call graph 的边的权值进行内联。
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