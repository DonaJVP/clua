#pragma once

// Include libraries for terminal manipulation inside CLua
#include <cstdint>
#include <string>
#include <vector>
struct lua_Table;
void initializeTBuffer(lua_Table *t);
/*
 * tbuf = {
 *   switchCleanUp = function(Values::Bool)
 * }
 */

typedef void(*_CL_TB_CLBK_String)(const std::vector<char>);

struct _CL_TB_COORDS {
    int64_t x;
    int64_t y; // Not used after all.
};

// A buffer for stdout/stdin.
class CL_TerminalBuffer {
public:
    CL_TerminalBuffer() {};
    char get_bch();
    //void moveC(int64_t x, int64_t y); // Non relative
    void saveToH(const std::string line);
    void loop();
    void emitMod(int64_t relY, int64_t relX);
    _CL_TB_CLBK_String _callback;
private:
    // Coordinates
    _CL_TB_COORDS coords;
    // Should we clean terminal every command we sent?
    bool m_cleanup = false;
    // History
    std::vector<std::vector<char>> m_history;
    uint64_t m_hpos;
};
