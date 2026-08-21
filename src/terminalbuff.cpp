#include "terminalbuff.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <termios.h>
#include <unistd.h>

std::string toBuf;

void _writeBuf(const char *str, size_t len) {
    std::cout.write(str, len);
}

// CL_TerminalBuffer
char CL_TerminalBuffer::get_bch() {
    char b = 0;
    struct termios old = {};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ~ICANON");
    if (read(0, &b, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ICANON");
    return b;
}

const char *sLine = "CLua> ";
#define offsetLensLine 6

void CL_TerminalBuffer::loop() {
    std::vector<char> buff;
    _writeBuf(sLine, offsetLensLine+1);
    while (true) {
        char c = get_bch();
        if (c == '\033') {
            get_bch();
            char sC = get_bch();
            switch (sC) {
                case 'A': {
                    // Moved up
                    emitMod(1, 0);
                    break;
                }
                case 'B': {
                    // Moved down
                    emitMod(-1, 0);
                    break;
                }
                case 'C': {
                    // Right
                    emitMod(0, 1);
                }
                case 'D': {
                    // Left
                    emitMod(0, -1);
                }
            }
        } else {
            if (c == 0xA) { // New line.
                m_history.push_back(buff);
                _callback(buff);
                m_hpos = 0; // Reset pos relative to m_history
                coords.x = offsetLensLine;
                std::cout << '\n';
                toBuf.append(sLine, offsetLensLine);
                std::cout << '\r';
                std::cout << toBuf;
                buff.clear();
            } else {
                if (c == 0x7F) {
                    coords.x = coords.x-1;
                    toBuf.erase(toBuf.end());
                    std::cout << '\r';
                    std::cout << toBuf;
                } else {
                    toBuf.push_back(c);
                    std::cout << '\r';
                    std::cout << toBuf;
                }
                coords.x = coords.x+1;
                buff.push_back(c);
            }
        }
    }
}

void CL_TerminalBuffer::emitMod(int64_t relY, int64_t relX) {
    std::string s;
    s.push_back('\033');
    s.push_back('[');
    if (relX > 0) {
        s.push_back('1');
        s.push_back('C');
    } else if (relX < 0) {
        s.push_back('1');
        s.push_back('D');
    } else {
        // Once at a time.
    }
    if (relY > 0) {
        s.push_back('1');
        s.push_back('A');
    } else if (relX < 0) {
        s.push_back('1');
        s.push_back('B');
        
    }
    _writeBuf(s.c_str(), s.size());
}
