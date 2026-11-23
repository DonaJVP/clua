#include "clregalloc.hpp"
#include "cljit.hpp"
#include "lua.hpp"
#include <asmjit/core/operand.h>
#include <asmjit/x86/x86builder.h>
#include <asmjit/x86/x86operand.h>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Register allocator for the compiler, which maybe will optimize the front outs.
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
using namespace asmjit;

std::unordered_map<std::string, uint64_t> uniqueSave;

CL_RegisterAllocator::CL_RegisterAllocator(void *asm_): m_asm(asm_) {
    // Create registers.
    // rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
    // r12, r13, r14, r15 and rbx shouldnt be touched.
    std::vector<x86::Gp> _regs{
        x86::rax,
        x86::rcx,
        x86::rdx,
        x86::rsi,
        x86::rdi,
        x86::r8,
        x86::r9,
        x86::r10,
        //x86::r11, r11 should be used only as a scratchpad for convertions, and many others.
    };
    for (x86::Gp &GP: _regs) {
        GeneralRegister *reg = new GeneralRegister();
        reg->GR_ID = GP;
        reg->status[""] = GR_Modified;
        reg->savedPtrs[""] = -0;
        m_registers.push_back(reg);
    }
    // end
}

int32_t CL_RegisterAllocator::getNextQwordNupdate() {
    int64_t c = bytes;
    bytes -= 8;
    return bytes;
}

std::vector<std::string> CL_RegisterAllocator::emitCall() {
    x86::Builder *a = (x86::Builder*)m_asm;
    std::vector<std::string> K;
    for (GeneralRegister *reg: m_registers) {
        if (reg->GR_ID == x86::rdi || reg->GR_ID == x86::rsi || reg->GR_ID == x86::rdx || reg->GR_ID == x86::rax) {
            if (reg->GR_ID == x86::rax) {
                // Deoccupy this.
                if (!reg->canOccupy) {
                    if (reg->status[reg->name] == GR_Maintains) {
                        if (uniqueSave[reg->name] > 1)
                            goto _CNT3_;
                        if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                            // Alloc new qword.
                            int32_t level = getNextQwordNupdate();
                            reg->savedPtrs[reg->name] = level;
                            a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                        } else {
                            int32_t level = reg->savedPtrs.at(reg->name);
                            a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                        }
                        if (uniqueSave[reg->name] == 1)
                            uniqueSave[reg->name] = 2;
                        _CNT3_:
                        reg->status[reg->name] = GR_Modified;
                        K.push_back(reg->name);
                    }
                }
                reg->canOccupy = true;
            } else {
                if (reg->contentId == 0) {
                    // Save register..
                    goto _ALWAYSSAVETHISREGISTER;
                } else {
                    reg->contentId = 0; // Reset.
                }
            }
            continue; // Handled by occupyArgumentsRegisters(...)
        }
        if (!reg->canOccupy) {
            _ALWAYSSAVETHISREGISTER:
            // We're playing NOW.
            if (reg->status[reg->name] != GR_Modified) {
                if (uniqueSave[reg->name] > 1)
                    goto _CNT4_;
                if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                    // Alloc new qword.
                    int32_t level = getNextQwordNupdate();
                    reg->savedPtrs[reg->name] = level;
                    a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                } else {
                    int32_t level = reg->savedPtrs.at(reg->name);
                    a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                }
                if (uniqueSave[reg->name] == 1)
                    uniqueSave[reg->name] = 2;
                _CNT4_:
                reg->status[reg->name] = GR_Modified;
                K.push_back(reg->name);
            }
        }
    }
    return K;
}

GeneralRegister *CL_RegisterAllocator::searchRegByNameX(const std::string &name) {
    if (m_registersStatus.find(name) != m_registersStatus.end()) {
        return m_registersStatus.at(name);
    } else {
        return nullptr;
    }
}

GeneralRegister *CL_RegisterAllocator::getFreeRegister() {
    for (GeneralRegister *reg: m_registers) {
        if (reg->canOccupy) {
            return reg;
        }
    }
    return nullptr;
}

GeneralRegister *CL_RegisterAllocator::searchRegByName(const std::string &name, bool onlyReadIfMemory) { 
    if (m_registersStatus.find(name) != m_registersStatus.end()) {
        // Check their status.
        GeneralRegister *reg = m_registersStatus.at(name);
        if (reg->status[name] == GR_Modified) {
            // Maybe search for another register which is not used..
            GeneralRegister *unusedReg = getFreeRegister();
            if (unusedReg != nullptr) {
                // Interleave registers.
                unusedReg->name = name;
                unusedReg->canOccupy = false;
                unusedReg->uses = 0;
                unusedReg->status[name] = GR_Maintains;
                unusedReg->contentId = 0;
                m_registersStatus[name] = unusedReg;
                a->mov(unusedReg->GR_ID, x86::qword_ptr(x86::rbp, reg->savedPtrs[name]));
                // Plus..
                unusedReg->savedPtrs[name] = reg->savedPtrs[name];
                return unusedReg;
            }
            // If onlyReadIfMemory is set, just return a reinterpret_cast object.
            if (onlyReadIfMemory) {
                
                return nullptr;
            }
            // See if other valued already occupied this register so we can save theirs.
            if (reg->name != name) {
                if (reg->status.at(reg->name) == GR_Maintains) {
                    if (uniqueSave[reg->name] > 1)
                        goto _CNT2_;
                    if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                        // Alloc new qword.
                        int32_t level = getNextQwordNupdate();
                        reg->savedPtrs[reg->name] = level;
                        a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                    } else {
                        int32_t level = reg->savedPtrs[reg->name];
                        a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                    }
                    if (uniqueSave[reg->name] == 1)
                        uniqueSave[reg->name] = 2;
                    _CNT2_:
                    __asm__("nop");
                } else { // Destroyed register and holds other data, so..
                    
                }
            }
            // Restore it.
            if (reg->savedPtrs[name] == 0) {
                m_LuaErrorHandler->reportError(_lua_es_Illegal, 0, "Unknown ptr offset for "+name+" register, supposed to had more than 0!");
                m_LuaErrorHandler->setFatal(true);
            }
            a->mov(reg->GR_ID, x86::qword_ptr(x86::rbp, reg->savedPtrs[name]));
            reg->name = name;
        }
        reg->uses = reg->uses + 1;
        return reg; 
    } else {
        return nullptr;
    }
}

// Used only if register are already known
void CL_RegisterAllocator::pushToKnownRegister(const std::string &name) {
    if (m_registersStatus.find(name) != m_registersStatus.end()) {
        GeneralRegister *reg = m_registersStatus.at(name);
        reg->status[name] = GR_Maintains;
    }
}

// DEBUG LANE.
const std::string _getRegisterName(x86::Gp K) {
    switch (K.id()) {
        case x86::Gp::kIdAx: return "<>AX";
        case x86::Gp::kIdBp: return "<>BP";
        case x86::Gp::kIdBx: return "<>BX";
        case x86::Gp::kIdCx: return "<>CX";
        case x86::Gp::kIdDi: return "<>DI";
        case x86::Gp::kIdDx: return "<>DX";
        case x86::Gp::kIdR10: return "<>10";
        case x86::Gp::kIdR11: return "<>11";
        case x86::Gp::kIdR12: return "<>12";
        case x86::Gp::kIdR13: return "<>13";
        case x86::Gp::kIdR14: return "<>14";
        case x86::Gp::kIdR15: return "<>15";
        case x86::Gp::kIdSi: return "<>SI";
        case x86::Gp::kIdSp: return "<>SP";
        case x86::Gp::kIdR8: return "<>8";
        case x86::Gp::kIdR9: return "<>9";
    }
    return "<!><!>";
}

GeneralRegister *CL_RegisterAllocator::createGR(const std::string &name, bool nonExpansible, x86::Gp _preferGp, bool singleTimeSave) {
    // Log
    qlog0._log2("<+> Registering ");
    qlog0._log2(name.c_str());
    qlog0._log2(" as ");
    x86::Builder *a = (x86::Builder*)m_asm;
    if (_preferGp != x86::noReg) {
        // Maybe select ONE register.
        GeneralRegister *reg_ = nullptr;
        for (GeneralRegister *reg: m_registers) {
            if (reg->GR_ID == _preferGp) {
                reg_ = reg;
            }
        }
        if (reg_ != nullptr) {
            // It is occupied, so first we save their data and then alloc this new name.
            if (!reg_->canOccupy) {
                reg_->status[reg_->name] = GR_Modified;
                if (uniqueSave[reg_->name] > 1)
                    goto _CNT0_;
                // Save to pointer.
                if (reg_->savedPtrs.find(reg_->name) != reg_->savedPtrs.end()) {
                    // Do not allocate new but save.
                    int32_t level = reg_->savedPtrs.at(reg_->name);
                    a->mov(x86::qword_ptr(x86::rbp, level), reg_->GR_ID);
                } else {
                    int32_t level = getNextQwordNupdate();
                    reg_->savedPtrs[reg_->name] = level;
                    a->mov(x86::qword_ptr(x86::rbp, level), reg_->GR_ID);
                }
                if (uniqueSave[reg_->name] == 1) {
                    uniqueSave[reg_->name] = uniqueSave[reg_->name] + 1;
                }
            }
            _CNT0_:
            reg_->canOccupy = false;
            reg_->name = name;
            reg_->status[name] = GR_Maintains;
            reg_->uses = 0;
            reg_->contentId = 1; // Manually set register.
            uniqueSave[name] = singleTimeSave;
            m_registersStatus[name] = reg_;
            qlog0._log2(_getRegisterName(reg_->GR_ID).c_str());
            qlog0._log2("\n");
            return reg_;
        } else {
            return nullptr; // Requested unknown register!
        }
    }
    // Search for instances if that register already exists.
    // Search for registers which has canOccupy flag to true
    for (GeneralRegister *reg: m_registers) {
        if (reg->canOccupy) {
            reg->name = name;
            reg->canOccupy = false;
            reg->uses = 0;
            reg->status[name] = GR_Maintains;
            uniqueSave[name] = singleTimeSave;
            m_registersStatus[name] = reg;
            qlog0._log2(_getRegisterName(reg->GR_ID).c_str());
            qlog0._log2("\n");
            return reg;
        }
    }
    // Check if name are saved.
    if (!nonExpansible) { // Do not overwrite other register if we want only the original register.
        x86::Assembler *a = (x86::Assembler*)m_asm;
        if (m_registersStatus.find(name) != m_registersStatus.end()) {
            // It are saved.
            GeneralRegister *r = m_registersStatus.at(name);
            try {
                if (r->status.at(name) != GR_Maintains) {
                    // Oof, it is saved!..
                    GeneralRegister *rNew = createGR(name, true);
                    if (rNew != nullptr) {
                        if (rNew->GR_ID != r->GR_ID) {
                            // Seek if this are saved...
                            a->mov(rNew->GR_ID, x86::qword_ptr(x86::rbp, r->savedPtrs[name]));
                            // Set new possibilities.
                            rNew->name = name;
                            rNew->canOccupy = false;
                            rNew->uses = 0;
                            rNew->status[name] = GR_Maintains;
                            uniqueSave[name] = singleTimeSave;
                            m_registersStatus[name] = rNew;
                            return rNew;
                        }
                    }
                }
            } catch (std::out_of_range &e) {
                // No need to get panic
            }
        }
        // Every register is used. So we might sacrifice others.
        GeneralRegister *reg = getLeastUsedRegister();
        if (reg->status.at(reg->name) == GR_Maintains) {
            reg->status[reg->name] = GR_Modified;
            // Save data.
            {
                if (uniqueSave[reg->name] > 1)
                    goto _CNT1_;
                if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                    int32_t level = getNextQwordNupdate();
                    reg->savedPtrs[reg->name] = level;
                    a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                } else {
                    int32_t level = reg->savedPtrs.at(reg->name);
                    a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                }
                if (uniqueSave[reg->name] == 1) {
                    uniqueSave[reg->name] = uniqueSave[reg->name] + 1;
                }
            }
        }
        _CNT1_:
        reg->name = name;
        reg->canOccupy = false;
        reg->uses = 0;
        reg->status[name] = GR_Maintains;
        uniqueSave[name] = singleTimeSave;
        qlog0._log2(_getRegisterName(reg->GR_ID).c_str());
        qlog0._log2("\n");
        m_registersStatus[name] = reg;
        return reg;
    } else {
        return nullptr;
    }
}

bool CL_RegisterAllocator::destroyGR(const std::string &name, bool purge) {
    if (m_registersStatus.find(name) == m_registersStatus.end())
        return false;
    m_registersStatus.at(name)->canOccupy = true;
    x86::Gp Gr = m_registersStatus.at(name)->GR_ID;
    if (purge) {
        // Delete their references..
        m_registersStatus.at(name)->name = "";
        if (m_registersStatus.at(name)->status.find(name) != m_registersStatus.at(name)->status.end())
            m_registersStatus.at(name)->status.at(name) = GR_Modified;
        m_registersStatus.erase(name);
    }
    qlog0._log2("<-> Destroyed ");
    qlog0._log2(name.c_str());
    qlog0._log2(" register; ");
    qlog0._log2(_getRegisterName(Gr).c_str());
    qlog0._log2("\n");
    return true;
}

void CL_RegisterAllocator::destroyGR(GeneralRegister *gp) {
    gp->canOccupy = true;
}

bool CL_RegisterAllocator::existsAtIndex(x86::Gp &reg) {
    std::vector<x86::Gp> _regs{
        x86::rax,
        x86::rcx,
        x86::rdx,
        x86::rsi,
        x86::rdi,
        x86::r8,
        x86::r9,
        x86::r10,
    };
    for (x86::Gp &GP: _regs) {
        if (GP == reg) {
            return true;
        }
    }
    return false;
}

#include <deque>
#include <set>
GeneralRegister *CL_RegisterAllocator::getLeastUsedRegister() {
    std::deque<GeneralRegister*> clf;
    uint8_t c = m_registers.size();
    std::set<std::string> alreadyset;
    while (c) {
        uint64_t mp = 0;
        GeneralRegister *cache;
        for (GeneralRegister *i: m_registers) {
            if (i->name == "" || alreadyset.find(i->name) != alreadyset.end()) {
                continue;
            }
            if (i->uses >= mp) {
                cache = i;
                mp = i->uses;
            }
            alreadyset.insert(cache->name);
            clf.push_front(cache);
        }        
        c--;
    }
    // Get the last node ever.
    return clf.back();
}

GeneralRegister *CL_RegisterAllocator::getRegisterRAW(x86::Gp gp) {
    for (GeneralRegister *reg: m_registers) {
        if (reg->GR_ID == gp) {
            return reg;
        }
    }
    return nullptr;
}

// Disable rdi, rsi, rdx just for a moment, and then clear.
void CL_RegisterAllocator::occupyArgumentsRegisters(GeneralRegister *gp1, GeneralRegister *gp2, GeneralRegister *gp3) {
    //OBSOLETE
    std::vector<x86::Gp> _regs_{x86::rdi, x86::rsi, x86::rdx};
    x86::Builder *a = (x86::Builder*)m_asm;
    for (x86::Gp &gp: _regs_) {
        GeneralRegister *reg = getRegisterRAW(gp);
        // Save those registers if they we're populated.
        if (reg->GR_ID == gp1->GR_ID || reg->GR_ID == gp2->GR_ID || reg->GR_ID == gp3->GR_ID) {
            // Skip.
        } else {
            if (reg->canOccupy) {
                a->mov(gp, gp1->GR_ID);
            } else {
                // Save before being overwritten
                // Check if previously saved..
                std::string name = reg->name;
                if (reg->savedPtrs.find(name) != reg->savedPtrs.end()) {
                    // Proceed.
                    reg->status[name] = GR_Modified;
                    a->mov(x86::qword_ptr(x86::rbp, reg->savedPtrs.at(name)), gp);
                } else {
                    // Create new slot.
                    int32_t level = getNextQwordNupdate();
                    reg->status[name] = GR_Modified;
                    reg->savedPtrs[name] = level;
                    a->mov(x86::qword_ptr(x86::rbp, level), gp);
                }
                // After saved. We can proceed.
                a->mov(gp, gp1->GR_ID);
            }
        }
    }
}


static void _CLRA__setAssemblerObject(x86::Builder *_a) {
}

static void _CLRA__movWhenNeeded(GeneralRegister *gp0, GeneralRegister *gp1) {
    if (gp0 == gp1) { // Theyre same address?
        return;
    } else {
        if (gp1 == nullptr) {
            return;
        }
        if (gp0 == nullptr) {
            return;
        }
        // Proceed.
        a->mov(gp0->GR_ID, gp1->GR_ID);
    }
}

x86::Gp T(GeneralRegister *gp) {
    gp->uses = gp->uses+1;
    return gp->GR_ID;
}

x86::Gp SX(const std::string &name) {
    GeneralRegister *reg = R->searchRegByNameX(name);
    reg->uses = reg->uses + 1;
    reg->status[reg->name] = GR_Modified;
    reg->status[name] = GR_Maintains;
    reg->name = name;
    reg->canOccupy = false;
    return reg->GR_ID;
}

x86::Gp S(const std::string &name, bool onlyReadIfMemory) {
    // Yeet.
    GeneralRegister *reg = R->searchRegByName(name, onlyReadIfMemory);
    qlog0._log2("<##> ");
    qlog0._log2((name+" register used as "+_getRegisterName(reg->GR_ID)).c_str());
    qlog0._log2("\n");
    reg->uses = reg->uses + 1;
    return reg->GR_ID;
}

GeneralRegister *X(x86::Gp &reg) {
    return R->getRegisterRAW(reg);
}

void mov(Operand X, Operand Y) {
    a->emit(x86::Inst::kIdMov, X, Y);
}

void mov(const std::string &X, Operand Y) {
    a->emit(x86::Inst::kIdMov, SX(X), Y);
}

/// CL_LocalsAllocator

CL_LocalsAllocator::CL_LocalsAllocator() {
    for (uint32_t i = 12; i < 16; i++) {
        m_allocSymbols[i] = nullptr;
    }
}
CL_LocalsAllocator::~CL_LocalsAllocator() {}

std::pair<bool, lua_localSymbol*> searchSymbolOnlyZone(lua_Scope *scp, const std::string &id) {
    if (scp->symbols.find(id) == scp->symbols.end()) {
        return {false,nullptr};
    } else {
        return {true, &scp->symbols.at(id)};
    }
}

std::vector<uint8_t> CL_LocalsAllocator::getFreeRegisters(lua_Scope *toNew) {
    // First get those nullptr registers (TMP)
    std::vector<uint8_t> vct;
    std::vector<uint8_t> vctSkipped;
    for (auto &i: m_allocSymbols) {
        if (i.second == nullptr) {
            vct.push_back(i.first);
        } else {
            vctSkipped.push_back(i.first);
        }
    }
    // Recalc it.
    // now scope: now*
    for (uint8_t &i: vctSkipped) {
        // If not found in toNew, maybe erase.
        bool r = searchSymbolOnlyZone(toNew, m_allocSymbols[i]->id).first;
        if (!r) {
            x86::Gp HSV = x86::Gp::make_r64(i);
            vct.push_back(i);
            // Restore it.
            // Must be a local.
            if (m_allocSymbols[i]->qID == 2) {
                std::string K = "cacheGp::"+std::to_string(i)+"::"+m_allocSymbols[i]->id;
                R->createGR(K);
                // Add type
                LuaType T = m_allocSymbols[i]->type;
                uint64_t X = NAN_BASE | (T << 48);
                a->mov(x86::r11, X);
                a->or_(HSV, x86::r11);
                // Save
                a->emit(x86::Inst::kIdMov, x86::qword_ptr(_0_0_0_CMPTIME_ASM_isScript ? S("f_mem_scr") : x86::rbp, (int32_t)(_0_0_0_CMPTIME_ASM_isScript ? 1 : -1) * m_allocSymbols[i]->slot), HSV);
            }
        }
    }
    return vct;
}

lua_localSymbol *_HELPER__recursiveSearchForSymbol(const std::string &x, lua_Scope *s) {
    // null check
    if (s == nullptr)
        return nullptr;
    // First check
    if (s->symbols.find(x) != s->symbols.end())
        return &s->symbols.at(x);
    // Second checks (recursive)
    return _HELPER__recursiveSearchForSymbol(x, s->rSCOPE);
}

bool _HELPER__searchForSymbol(lua_Scope *toS, const std::string &X) {
    return toS->symbols.find(X) != toS->symbols.end();
}

void CL_LocalsAllocator::nextScope(lua_Scope *scope) {
    // Reorganize pointers and registers.
    // Keep an eye, should not allow global variables around this place.
    // Request next registers.
    std::vector<uint8_t> freed = getFreeRegisters(scope);
    // Now scope = now
    // Register possible candidates to HSV -> vars.
    for (uint8_t C = 0; C < 5; ++C) {
        std::string selVar = "";
        try {
            selVar = scope->HVtoCompiler.at(C).first;
        } catch (std::out_of_range &e) {
            goto _END;
        }
        /// Check
        if (_HELPER__searchForSymbol(now, selVar)) 
            goto _END;
        // Alloc.
        // Get symbol
        lua_localSymbol *sym = _HELPER__recursiveSearchForSymbol(selVar, scope);
        if (sym == nullptr) {
            // Are a general table symbol
            // ...
        } else {
            // Get C register
            uint8_t regN = freed.back();
            freed.pop_back();
            if (m_allocSymbols.at(regN) != nullptr) {
                m_allocSymbols[regN]->cacheReg = 0;
                m_allocSymbols.erase(regN);
                m_allocSymbols[regN] = sym;
            }
            sym->cacheReg = regN;
            /// ASM
            a->emit(x86::Inst::kIdMov, x86::Gp::make_r64(regN), x86::qword_ptr(_0_0_0_CMPTIME_ASM_isScript ? S("f_mem_scr") : x86::rbp, (int32_t)(_0_0_0_CMPTIME_ASM_isScript ? 1 : -1) * sym->slot));
            a->emit(x86::Inst::kIdMov, x86::r11, PTR_MASK);
            a->emit(x86::Inst::kIdAnd, x86::Gp::make_r64(regN), x86::r11);
        }
    }
    _END:
    now = scope;
    __asm__("nop");
}

void CL_LocalsAllocator::downscaleScope() {
    // Get back to now->rSCOPE
    // Delete those vars which are used ONLY in now scope and maintain those variables that rSCOPE uses
    std::vector<uint8_t> freeRegisters = {};
    for (auto &i: m_allocSymbols) {
        std::string symName = i.second == nullptr ? "" : i.second->id;
        if (i.second == nullptr) {
            freeRegisters.push_back(i.first);
        } else if (!_HELPER__searchForSymbol(now->rSCOPE, symName)) {
            freeRegisters.push_back(searchSymbolOnlyZone(now->rSCOPE, symName).second->cacheReg);
            m_allocSymbols[i.first] = nullptr;
        }
    }
    // Reattach needed variables.
    for (uint8_t C = 0; C < 5; ++C) {
        std::string selVar = "";
        try {
            selVar = now->rSCOPE->HVtoCompiler.at(C).first;
        } catch (std::out_of_range &e) {
            goto _reached_end;
        }
        // Alloc those what are needed
        lua_localSymbol *sym = _HELPER__recursiveSearchForSymbol(selVar, now->rSCOPE);
        if (sym->cacheReg == 0) {
            uint8_t regC = freeRegisters.back();
            freeRegisters.pop_back();
            //
            m_allocSymbols[regC]->cacheReg = 0;
            m_allocSymbols.erase(regC);
            m_allocSymbols[regC] = sym;
            sym->cacheReg = regC;
        }
    }
    _reached_end:
    if (now->rSCOPE != nullptr)
        now = now->rSCOPE;
    __asm__("nop");
}

Operand CL_LocalsAllocator::getSym(const std::string &name, bool alwaysAddress) {
    lua_localSymbol *SYM = _HELPER__recursiveSearchForSymbol(name, now);
    if (SYM) {
        if (alwaysAddress) {
            goto _getADDR;
        }
        if (SYM->cacheReg > 0) {
            // Are on HSV
            return x86::Gp::make_r64(SYM->cacheReg);
        } else {
            _getADDR:
            return x86::qword_ptr(_0_0_0_CMPTIME_ASM_isScript ? S("f_mem_scr") : x86::rbp, (int32_t)(_0_0_0_CMPTIME_ASM_isScript ? 1 : -1) * SYM->slot);
        }
    } else {
        return Imm(0);
    } 
}

bool CL_LocalsAllocator::existsSymbolBetweenNow(const std::string &name) {
    return _HELPER__recursiveSearchForSymbol(name, now) != nullptr;
}
