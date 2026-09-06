#include "clregalloc.hpp"
#include <cstdint>
#include <vector>

// Register allocator for the compiler, which maybe will optimize the front outs.
#include <asmjit/core.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
using namespace asmjit;
using namespace asmjit::x86;

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
        m_registers.push_back(reg);
    }
    // end
}

int32_t CL_RegisterAllocator::getNextQwordNupdate() {
    int64_t c = bytes;
    bytes -= 8;
    return c;
}

void CL_RegisterAllocator::emitCall() {
    x86::Assembler *a = (x86::Assembler*)m_asm;
    for (GeneralRegister *reg: m_registers) {
        if (reg->GR_ID == x86::rdi || reg->GR_ID == x86::rsi || reg->GR_ID == x86::rdx || reg->GR_ID == x86::rax) {
            if (reg->GR_ID == x86::rax) {
                // Deoccupy this.
                if (reg->status[reg->name] == GR_Maintains) {
                    if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                        // Alloc new qword.
                        int32_t level = getNextQwordNupdate();
                        reg->savedPtrs[reg->name] = level;
                        a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                    } else {
                        int32_t level = reg->savedPtrs.at(reg->name);
                        a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                    }
                }
                reg->canOccupy = true;
            }
            continue; // Handled by occupyArgumentsRegisters(...)
        }
        if (!reg->canOccupy) {
            // We're playing NOW.
            if (reg->status[reg->name] != GR_Modified) {
                if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                    // Alloc new qword.
                    int32_t level = getNextQwordNupdate();
                    reg->savedPtrs[reg->name] = level;
                    a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                } else {
                    int32_t level = reg->savedPtrs.at(reg->name);
                    a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                }
            }
        }
    }
}

GeneralRegister *CL_RegisterAllocator::searchRegByName(const std::string &name) { 
    if (m_registersStatus.find(name) != m_registersStatus.end()) {
        // Check their status.
        GeneralRegister *reg = m_registersStatus.at(name);
        if (reg->status[name] == GR_Modified) {
            x86::Assembler *a = (x86::Assembler*)m_asm;
            // See if other valued already occupied this register so we can save theirs.
            if (reg->name != name) {
                if (reg->status.at(reg->name) == GR_Maintains) {
                    if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                        // Alloc new qword.
                        int32_t level = getNextQwordNupdate();
                        reg->savedPtrs[reg->name] = level;
                        a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                    } else {
                        int32_t level = reg->savedPtrs[reg->name];
                        a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
                    }
                } else { // Destroyed register and holds other data, so..
                    
                }
            }
            // Restore it.
            a->mov(reg->GR_ID, x86::qword_ptr(x86::rbp, reg->savedPtrs[name]));
        }
        reg->uses = reg->uses + 1;
        return reg; 
    } else {
        return nullptr;
    }
}

GeneralRegister *CL_RegisterAllocator::createGR(const std::string &name, bool nonExpansible, x86::Gp _preferGp) {
    x86::Assembler *a = (x86::Assembler*)m_asm;
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
                // Save to pointer.
                if (reg_->savedPtrs.find(reg_->name) != reg_->savedPtrs.end()) {
                    // Do not allocate new but save.
                    int32_t level = reg_->savedPtrs.at(reg_->name);
                    a->mov(x86::qword_ptr(x86::rbp, level), reg_->GR_ID);
                } else {
                    int32_t level = getNextQwordNupdate();
                    a->mov(x86::qword_ptr(x86::rbp, level), reg_->GR_ID);
                }
            }
            reg_->canOccupy = false;
            reg_->name = name;
            reg_->status[name] = GR_Maintains;
            reg_->uses = 0;
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
            m_registersStatus[name] = reg;
            return reg;
        }
    }
    // Check if name are saved.
    if (!nonExpansible) { // Do not overwrite other register if we want only the original register.
        x86::Assembler *a = (x86::Assembler*)m_asm;
        if (m_registersStatus.find(name) != m_registersStatus.end()) {
            // It are saved.
            GeneralRegister *r = m_registersStatus.at(name);
            if (r->status[name] != GR_Maintains) {
                // Oof, it is saved!..
                GeneralRegister *rNew = createGR(name, true);
                if (rNew != nullptr) {
                    if (rNew->GR_ID != r->GR_ID) {
                        // Perfect.
                        a->mov(rNew->GR_ID, x86::qword_ptr(x86::rbp, r->savedPtrs[name]));
                        // Set new possibilities.
                        rNew->name = name;
                        rNew->canOccupy = false;
                        rNew->uses = 0;
                        m_registersStatus[name] = rNew;
                        return rNew;
                    }
                }
            }
        }
        // Every register is used. So we might sacrifice others.
        GeneralRegister *reg = getLeastUsedRegister();
        reg->status[reg->name] = GR_Modified;
        // Save data.
        {
            if (reg->savedPtrs.find(reg->name) == reg->savedPtrs.end()) {
                int32_t level = getNextQwordNupdate();
                reg->savedPtrs[reg->name] = level;
                a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
            } else {
                int32_t level = reg->savedPtrs.at(reg->name);
                a->mov(x86::qword_ptr(x86::rbp, level), reg->GR_ID);
            }
        }
        reg->name = name;
        reg->canOccupy = false;
        reg->uses = 0;
        reg->status[name] = GR_Maintains;
        return reg;
    } else {
        return nullptr;
    }
}

bool CL_RegisterAllocator::destroyGR(const std::string &name) {
    if (m_registersStatus.find(name) == m_registersStatus.end())
        return false;
    m_registersStatus.at(name)->canOccupy = true;
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
    std::vector<x86::Gp> _regs_{x86::rdi, x86::rsi, x86::rdx};
    x86::Assembler *a = (x86::Assembler*)m_asm;
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

// Global api...
x86::Assembler *a = nullptr;

static void _CLRA__setAssemblerObject(x86::Assembler *_a) {
    a = _a;
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

x86::Gp S(const std::string &name) {
    // Yeet.
    GeneralRegister *reg = R->searchRegByName(name);
    reg->uses = reg->uses + 1;
    return reg->GR_ID;
}

GeneralRegister *X(x86::Gp &reg) {
    return R->getRegisterRAW(reg);
}
