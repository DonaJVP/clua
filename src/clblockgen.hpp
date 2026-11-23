// Per block generation.

#include <asmjit/core.h>
#include <asmjit/core/operand.h>
#include <asmjit/x86.h>
#include <asmjit/host.h>

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <deque>


namespace clb {
    
struct CL_CodeBlock;

/*
 * CL_CodeBlockInst type reference
 * 0 = normal bindings
 * 1 = Block
 * 2 = register name allocation
 * 3 = save register 'name'
 */

struct CL_CodeBlockInst {
    uint64_t type;
    CL_CodeBlock *block;
    std::string toSave0;
    asmjit::InstId INSTRUCTION;
    asmjit::Operand op0;
    asmjit::Operand op1;
    std::pair<bool, std::string> op0_R{false, ""};
    std::pair<bool, std::string> op1_R{false, ""};
};

struct CL_registerState {
    uint64_t state;
    uint64_t usage;
    std::string name;
};

enum BlockFlags: uint64_t {
    BF_CALL_SITE = 1ull << 0,
};

enum class StreamTarget: uint8_t {Raw, Entrance, Exit};

struct CL_CodeBlock {
    std::string codeBlockName; // Example: get(varname0)
    std::vector<CL_CodeBlockInst> rawInst; // Pack of instructions, which their register value should be allocated
    std::deque<CL_CodeBlockInst> entranceInstructionSet;
    std::deque<CL_CodeBlockInst> outInstructionSet;
    std::unordered_map<std::string, CL_registerState> regId; // Per register identification usage, just for evaluate and then alloc to their respective spaces.
    std::unordered_map<std::string, uint64_t> outerRegIdUsage; // Outside blocks
    CL_CodeBlock *back;
    uint64_t flags = 0;
    std::vector<CL_CodeBlock*> upperBlocks;
};

void CL_NewBlock();
void CL_EndBlock();
void declareRegisterName(const std::string &name);
void CL_InstSetParts(bool mode);

// Instructions.

std::string TGR(); // Return temporary general register {r11}

using RegResolver = std::function<asmjit::x86::Gp(const std::string &)>;
void compile(asmjit::x86::Builder &a, const RegResolver &resolve);

} // namespace clb
