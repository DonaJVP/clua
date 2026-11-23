#pragma once

#include <asmjit/x86.h>
#include <cstdint>
#include <string>
#include <vector>

namespace clir {

using VReg = uint32_t;
using BlockId = uint32_t;
constexpr uint32_t kNone = UINT32_MAX;

struct Reg {
    uint32_t id = kNone;
    bool phys = false;
    static Reg V(VReg v) { return {v, false}; }
    static Reg P(const asmjit::x86::Gp &g) { return {g.id(), true}; }
    bool valid() const { return id != kNone; }
};

struct MemRef {
    Reg base;
    Reg index;
    int32_t disp = 0;
    uint8_t scale = 1;
};

enum class Kind: uint8_t {None, Reg, Mem, Imm, Block};

struct Opnd {
    Kind kind = Kind::None;
    Reg reg;
    MemRef mem;
    int64_t imm = 0;
    BlockId block = kNone;
    static Opnd R(Reg r) { Opnd o; o.kind = Kind::Reg; o.reg = r; return o; }
    static Opnd V(VReg v) { return R(Reg::V(v)); }
    static Opnd Imm(int64_t i) { Opnd o; o.kind = Kind::Imm; o.imm = i; return o; }
    static Opnd M(MemRef m) { Opnd o; o.kind = Kind::Mem; o.mem = m; return o; }
    static Opnd B(BlockId b) { Opnd o; o.kind = Kind::Block; o.block = b; return o; }
};

enum InstFlag: uint32_t {
    F_JUMP = 1 << 0,
    F_COND = 1 << 1,
    F_RET = 1 << 2,
    F_CALL = 1 << 3,
    F_SIDE = 1 << 4,
};

struct Inst {
    asmjit::InstId id = 0;
    Opnd ops[3];
    uint8_t n = 0;
    uint32_t flags = 0;
    uint16_t clobbers = 0; //
    bool isTerminator() const {return flags & (F_JUMP | F_COND | F_RET);}
};

struct Block {
    BlockId id = kNone;
    std::string name;
    std::vector<Inst> insts;
    BlockId succ[2] = {kNone, kNone}; // [0] = taken-only; [1] = fallthrought-fallback
    std::vector<BlockId> preds;
    std::vector<uint64_t> use;
    std::vector<uint64_t> def;
    std::vector<uint64_t> liveIn;
    std::vector<uint64_t> liveOut;
    bool terminated() const {return !insts.empty() && insts.back().isTerminator(); }
};

class Function {
public:
    // Building up registers/blocks
    VReg newVReg(std::string name = "") {
        vregNames_.push_back(std::move(name));
        return (VReg)vregNames_.size()-1;
    }
    BlockId newBlock(std::string name = "") {
        Block b;
        b.id = (BlockId)blocks_.size();
        b.name = std::move(name);
        blocks_.push_back(std::move(b));
        return blocks_.back().id;
    }
    void setInsert(BlockId b) { cur_ = b; }
    BlockId insert() const { return cur_; }
    // Emmit
    Inst &emit(asmjit::InstId id, Opnd a = {}, Opnd b = {}, uint32_t flags = 0) {
        Block &blk = blocks_[cur_];
        if (blk.terminated())
            abort();
        Inst i;
        i.id = id;
        i.ops[0] = a;
        i.ops[1] = b;
        i.flags = flags;
        i.n = (a.kind != Kind::None) + (b.kind != Kind::None);
        blk.insts.push_back(i);
        return blk.insts.back();
    }
    void mov(Opnd d, Opnd s) { emit(asmjit::x86::Inst::kIdMov, d, s); }
    void jmp(BlockId t) {
        emit(asmjit::x86::Inst::kIdJmp, Opnd::B(t), {}, F_JUMP);
        blocks_[cur_].succ[0] = t;
    }
    void jcc(asmjit::InstId cc, BlockId taken, BlockId fall) {
        emit(cc, Opnd::B(taken), {}, F_COND);
        blocks_[cur_].succ[0] = taken;
        blocks_[cur_].succ[1] = fall;
    }
    void ret() {
        emit(asmjit::x86::Inst::kIdRet, {}, {}, F_RET);
    }
    static constexpr uint16_t kCallerSaved = (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8)|(1<<9)|(1<<10)|(1<<11);
    void call(Opnd target, uint16_t clobbers = kCallerSaved) {
        Inst &i = emit(asmjit::x86::Inst::kIdCall, target, {}, F_CALL | F_SIDE);
        i.clobbers = clobbers;
    }
    // CFG
    std::vector<BlockId> buildCFG() {
        for (Block &b: blocks_)
            b.preds.clear();
        for (Block &b: blocks_) {
            if (!b.terminated())
                abort();
            for (BlockId s: b.succ)
                if (s != kNone)
                    blocks_[s].preds.push_back(b.id);
        }
        std::vector<BlockId> post;
        std::vector<uint8_t> seen(blocks_.size(), 0);
        dfs(0, seen, post);
        return {post.rbegin(), post.rend()};
    }
    Block &blk(BlockId b) { return blocks_[b]; }
    size_t numBlocks() const { return blocks_.size(); }
    size_t numVRegs() const { return vregNames_.size(); }
private:
    void dfs(BlockId b, std::vector<uint8_t> &seen, std::vector<BlockId> &post) {
        if (seen[b])
            return;
        for (BlockId s: blocks_[b].succ)
            if (s != kNone)
                dfs(s, seen, post);
        post.push_back(b);
    }
    std::vector<Block> blocks_;
    std::vector<std::string> vregNames_;
    BlockId cur_ = 0;
};

}
