#include <asmjit/core.h>
#include <asmjit/core/operand.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>
#include <cstdint>

#include "clblockgen.hpp"

using namespace asmjit;

namespace clb {

namespace {
std::string logBuffer;
CL_CodeBlock *now = nullptr;
CL_CodeBlock *base = nullptr;
StreamTarget stream = StreamTarget::Raw;

void L(const std::string &S) {
    logBuffer.append(S);
}

std::deque<CL_CodeBlockInst> &entrance() {
    return now->entranceInstructionSet;
}

void push(CL_CodeBlockInst inst) {
    switch (stream) {
        case StreamTarget::Raw: {
            now->rawInst.push_back(std::move(inst));
            break;
        }
        case StreamTarget::Entrance: {
            now->entranceInstructionSet.push_back(std::move(inst));
            break;
        }
        case StreamTarget::Exit: {
            now->outInstructionSet.push_back(std::move(inst));
            break;
        }
    }
}

void declareRegisterName(const std::string &name) {
    if (!now)
        CL_NewBlock();
    CL_registerState st;
    st.name = name;
    now->regId[name] = st;
}

CL_registerState *findReg(const std::string &name, bool *outer = nullptr) {
    bool isOuter = false;
    for (CL_CodeBlock *b = now; b!= nullptr; b = b->back) {
        auto it = b->regId.find(name);
        if (it != b->regId.end()) {
            if (outer)
                *outer = isOuter;
            return &it->second;
        }
        isOuter = true;
    }
    return nullptr;
}


CL_registerState *touchReg(const std::string &name, bool &outer) {
    outer = false;
    CL_registerState *s = findReg(name, &outer);
    if (!s) {
        declareRegisterName(name);
        outer = false;
        s = findReg(name);
    }
    return s;
}

void bump(const std::string &name) {
    bool outer = false;
    CL_registerState *s = touchReg(name, outer);
    s->usage++;
    if (outer)
        now->outerRegIdUsage[name]++;
}

} // namespace

void CL_NewBlock() {
    CL_CodeBlock *n0 = new CL_CodeBlock();
    n0->back = now;
    now = n0;
    if (now) {
        CL_CodeBlockInst ref;
        ref.type = 1;
        ref.block = n0;
        now->rawInst.push_back(ref);
    }
    if (base == nullptr)
        base = n0;
    stream = StreamTarget::Raw;
    L("{");
}

void CL_EndBlock() {
    if (now->back != nullptr && now->back) {
        L("}");
        now->back->upperBlocks.push_back(now);
        now = now->back;
        stream = StreamTarget::Raw;
    }
}

// EMMITER
static void emitBlock(x86::Builder &a, CL_CodeBlock *b, const RegResolver &resolve);
static void emitInst(x86::Builder &a, const CL_CodeBlockInst &i, const RegResolver &resolve) {
    if (i.type != 0)
        return; // 2,3 are handled by the allocator.
    Operand o0 = i.op0_R.first ? Operand(resolve(i.op0_R.second)) : i.op0;
    Operand o1 = i.op1_R.first ? Operand(resolve(i.op1_R.second)) : i.op1;
    a.emit(i.INSTRUCTION, o0, o1);
}

static void emitBlock(x86::Builder &a, CL_CodeBlock *b, const RegResolver &resolve) {
    for (auto &i: b->entranceInstructionSet)
        emitInst(a, i, resolve);
    for (auto &i: b->rawInst) {
        if (i.type == 1)
            emitBlock(a, i.block, resolve);
        else
            emitInst(a, i, resolve);
    }
    for (auto &i: b->outInstructionSet)
        emitInst(a, i, resolve);
}

void compile(x86::Builder &a, const RegResolver &resolve) {
    if (base)
        emitBlock(a, base, resolve);
}

} //namespace clb
