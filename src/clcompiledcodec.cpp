#include "clcompiledcodec.hpp"
#include "cljit.hpp"
#include <asmjit/core/builder.h>
#include <asmjit/core/inst.h>
#include <asmjit/x86/x86builder.h>

using namespace asmjit;

void prepareFinalCode(x86::Builder *OBJ) {
    BaseNode *node = OBJ->first_node();
    BaseNode *lastNode = nullptr;
    while (node) {
        InstNode *IN = node->as<InstNode>();
        if (lastNode == nullptr) {
            lastNode = IN;
            goto _END;
        }
        if ((lastNode->as<InstNode>())->is_inst() && IN->is_inst()) {
            InstId lni = (lastNode->as<InstNode>())->inst_id();
            InstId ani = IN->inst_id();
            bool fCase = lni == ani;
            ///
            
        }
        _END:
        node = node->next();
    }
}
