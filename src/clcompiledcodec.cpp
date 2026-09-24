#include "clcompiledcodec.hpp"
#include <asmjit/core/builder.h>
#include <asmjit/core/inst.h>
#include <asmjit/core/operand.h>
#include <asmjit/x86/x86builder.h>
#include <asmjit/x86/x86globals.h>
#include <cstdint>
#include "lua.hpp"
#include <iostream>
#include <unordered_map>

using namespace asmjit;

std::unordered_map<int64_t, BaseNode*> toDel;
std::unordered_map<int64_t, bool> toDelV;
uint64_t COUNTER00 = 0;

void prepareFinalCode(x86::Builder *OBJ) {
    BaseNode *node = OBJ->first_node();
    BaseNode *lastNode = nullptr;
    while (node) {
        InstNode *IN = node->as<InstNode>();
        if (lastNode == nullptr) {
            lastNode = IN;
            goto _END;
        }
        // Delete some instructions which does nothing for the next 5 instructions (Or nil)
        // Like qword ptr [rbp-64], rdi; which [rbp-64] is not used anymore.
        if (IN->inst_id() == x86::Inst::kIdMov) {
            Operand op0 = IN->op(0);
            if (op0.is_mem()) {
                // Huh..
                x86::Mem X = op0.as<x86::Mem>();
                if (X.base_reg() == x86::rbp) {
                    if (toDelV.find(X.offset()) == toDelV.end()) {
                        toDel[X.offset()] = IN;
                        COUNTER00++;
                        toDelV[X.offset()] = true;
                    }
                }
            } else {
                Operand op1 = IN->op(1);
                if (op1.is_mem()) {
                    x86::Mem X = op1.as<x86::Mem>();
                    if (X.base_reg() == x86::rbp) {
                        int64_t Y = X.offset();
                        if (toDel.find(Y) != toDel.end()) {
                            toDel.erase(Y);
                            COUNTER00--;
                            toDelV[Y] = false;
                        } else {
                            // Optimization method for regAlloc if register are deleted BUT their info keeps.
                        }
                    }
                }
            }
        }
        if ((lastNode->as<InstNode>())->is_inst() && IN->is_inst()) {
            InstId lni = (lastNode->as<InstNode>())->inst_id();
            InstId ani = IN->inst_id();
            bool fCase = lni == ani;
            ///
            
            // Delete some instructions like mov rdi, rdi
            if (IN->op_count() > 1) {
                Operand op00 = IN->op(0);
                Operand op01 = IN->op(1);
                ///
                if (op00.equals(op01)) {
                    // Delete this node.
                    OBJ->remove_node(IN);
                }
            }
            
            
            
            // Delete some instructions which extends the use of registers.
            if ((lastNode->as<InstNode>())->op_count() > 1 && IN->op_count() > 1) {
                Operand op00 = (lastNode->as<InstNode>())->op(0);
                Operand op01 = (lastNode->as<InstNode>())->op(1);
                Operand op10 = IN->op(0);
                Operand op11 = IN->op(1);
                // Chain wave.
                
            }
            // FIRST PHASE COMPARATION.
            
        }
        _END:
        node = node->next();
    }
    std::cout << "REQUESTED TO DELETE " << COUNTER00 << " NODES!" << std::endl;
    // Delete requested nodes..
    for (auto X = toDel.begin(); X != toDel.end(); X++) {
        std::cout << "REMOVING INST 'mov' NODE WHICH IS UNUSED ON CODE: " << X->first << std::endl;
        OBJ->remove_node(X->second);
    }
    toDel.clear();
    toDelV.clear();
}
