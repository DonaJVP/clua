#pragma once
#include <asmjit/x86.h>
#include "clir.hpp"
#include <algorithm>
#include <cstdio>

namespace clir {
    using Bits = std::vector<uint64_t>;
    inline void bset(Bits &b, uint32_t i) {
        b[i >> 6] |=  (1ull << (i & 63));
    }
    inline void bclr(Bits &b, uint32_t i) { 
        b[i >> 6] &= ~(1ull << (i & 63));
    }
    inline bool btest(const Bits &b, uint32_t i) { 
        return b[i >> 6] & (1ull << (i & 63));
    }
    
    struct DefUse {
        std::vector<VReg> defs, uses;
        bool memWrite = false; // writes through a memory operand
    };
    
    namespace detail {
        inline void useReg(DefUse &d, const Reg &r) {
            if (r.valid() && !r.phys) 
                d.uses.push_back(r.id); 
        }
        inline void useMem(DefUse &d, const MemRef &m) { 
            useReg(d, m.base); 
            useReg(d, m.index); 
        }
        inline void useOpnd(DefUse &d, const Opnd &o) {
            if (o.kind == Kind::Reg) 
                useReg(d, o.reg);
            else if (o.kind == Kind::Mem) 
                useMem(d, o.mem);
        }
        // Instructions which op0 is written but NOT read.
        inline bool isPureWrite(asmjit::InstId id) {
            switch (id) {
                case asmjit::x86::Inst::kIdMov: 
                case asmjit::x86::Inst::kIdMovabs: 
                case asmjit::x86::Inst::kIdLea:
                case asmjit::x86::Inst::kIdMovzx:
                case asmjit::x86::Inst::kIdMovsx:
                case asmjit::x86::Inst::kIdMovsxd:
                case asmjit::x86::Inst::kIdMovq:
                case asmjit::x86::Inst::kIdMovd:
                case asmjit::x86::Inst::kIdCvttsd2si:
                case asmjit::x86::Inst::kIdCvtsi2sd:
                    return true;
                default: return false;
            }
        }
        // Only read operands, write nothing.
        inline bool isReadOnly(asmjit::InstId id) {
            return id == asmjit::x86::Inst::kIdCmp || id == asmjit::x86::Inst::kIdTest;
        }
    } // namespace detail
    
    inline DefUse defsUses(const Inst &in) {
        using namespace detail;
        DefUse d;
        if (in.flags & (F_JUMP | F_COND | F_RET)) 
            return d; // block operands only
        if (in.id == asmjit::x86::Inst::kIdCall) { 
            useOpnd(d, in.ops[0]); 
            return d; 
        }
        const Opnd &o0 = in.ops[0];
        // The zeroing `xor v, v` defines v without reading it.
        if (in.id == asmjit::x86::Inst::kIdXor && o0.kind == Kind::Reg && in.ops[1].kind == Kind::Reg && !o0.reg.phys && !in.ops[1].reg.phys && o0.reg.id == in.ops[1].reg.id) {
            d.defs.push_back(o0.reg.id);
            return d;
        }
        for (int k = 1; k < in.n; k++) 
            useOpnd(d, in.ops[k]);
            
        if (isReadOnly(in.id)) { 
            useOpnd(d, o0); 
            return d; 
        }
        if (o0.kind == Kind::Reg) {
            if (!o0.reg.phys && o0.reg.valid()) {
                d.defs.push_back(o0.reg.id);
                if (!isPureWrite(in.id))
                    d.uses.push_back(o0.reg.id); // read-modify-write (add, sub, setz...)
                }
        } else if (o0.kind == Kind::Mem) {
            useMem(d, o0.mem);
            d.memWrite = true;
        }
        return d;
    }
    
    // `rpo` is the vector returned by Function::buildCFG().
    inline void computeLiveness(Function &F, const std::vector<BlockId> &rpo) {
        const size_t words = (F.numVRegs() + 63) / 64 + 1;
        for (BlockId id : rpo) {
            Block &b = F.blk(id);
            b.use.assign(words, 0); 
            b.def.assign(words, 0);
            b.liveIn.assign(words, 0); 
            b.liveOut.assign(words, 0);
            for (const Inst &in : b.insts) {
                DefUse du = defsUses(in);
                for (VReg u : du.uses) 
                    if (!btest(b.def, u)) 
                        bset(b.use, u); // upward-exposed
                for (VReg d : du.defs) 
                    bset(b.def, d);
            }
        }
        // Backward problem: walking reverse RPO converges quickly.
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto it = rpo.rbegin(); it != rpo.rend(); ++it) {
                Block &b = F.blk(*it);
                Bits out(words, 0);
                for (BlockId s : b.succ)
                    if (s != kNone)
                        for (size_t w = 0; w < words; w++) 
                            out[w] |= F.blk(s).liveIn[w];
                Bits in(words);
                for (size_t w = 0; w < words; w++) 
                    in[w] = b.use[w] | (out[w] & ~b.def[w]);
                if (out != b.liveOut || in != b.liveIn) {
                    b.liveOut = std::move(out);
                    b.liveIn = std::move(in);
                    changed = true;
                }
            }
        }
    }
    
    // Removes `mov/lea/movzx... v, x` when v is not live afterwards.
    inline size_t eliminateDeadMoves(Function &F, const std::vector<BlockId> &rpo) {
        size_t removed = 0;
        for (BlockId id : rpo) {
            Block &b = F.blk(id);
            Bits live = b.liveOut;
            std::vector<Inst> kept;
            for (auto it = b.insts.rbegin(); it != b.insts.rend(); ++it) {
                DefUse du = defsUses(*it);
                bool removable = !(it->flags & (F_SIDE | F_JUMP | F_COND | F_RET | F_CALL)) && detail::isPureWrite(it->id) && !du.memWrite && !du.defs.empty() &&
                std::none_of(du.defs.begin(), du.defs.end(), [&](VReg v) { return btest(live, v); });
                if (removable) { 
                    removed++; 
                    continue; 
                }
                for (VReg d : du.defs) 
                    bclr(live, d);
                for (VReg u : du.uses) 
                    bset(live, u);
                kept.push_back(*it);
            }
            std::reverse(kept.begin(), kept.end());
            b.insts = std::move(kept);
        }
        return removed;
    }
    
    inline void dumpLiveness(Function &F, const std::vector<BlockId> &rpo) {
        auto show = [&](const Bits &b) {
            std::string s = "{";
            for (uint32_t v = 0; v < F.numVRegs(); v++)
                if (btest(b, v)) { s += " v" + std::to_string(v); }
            return s + " }";
        };
        for (BlockId id : rpo) {
            Block &b = F.blk(id);
            printf("%s (b%u) livein=%s; liveout=%s\n", b.name.c_str(), id, show(b.liveIn).c_str(), show(b.liveOut).c_str());
        }
    }
    
} // namespace clir
