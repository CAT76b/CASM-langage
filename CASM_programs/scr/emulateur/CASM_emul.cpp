#include <vector>
#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>

enum OpCode {
    CRT = 1,
    SET,
    PRT_VAR,
    PRT_STR,
    EXT,
    ADD,
    SUB,
    MUL,
    DIV,
    POW,
    SQR,
    RND,
    AND,
    OR,
    NND,
    NOR,
    LIF,
    LCT,
    CPR,
    CPG,
    JMP,
    JPT,
    JPF,
    CAL,
    PSH,
    LOD,
    RET
};

struct Program {
    std::vector<uint8_t> code;
    std::vector<int32_t> variables;
    std::vector<std::string> strings;
    uint16_t entryPoint;
};

Program load(const std::string& path) {
    Program p;

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Cannot open file" << std::endl;
        exit(1);
    }

    char magic[4];
    f.read(magic, 4);
    if (std::string(magic, 4) != "CASM") {
        std::cerr << "Invalid CASM file" << std::endl;
        exit(1);
    }

    uint8_t version;
    f.read((char*)&version, 1);
    f.read((char*)&p.entryPoint, 2);
    uint16_t varCount;
    f.read((char*)&varCount, 2);
    p.variables.resize(varCount);

    for (auto& v : p.variables) {
        uint8_t type;
        f.read((char*)&type, 1);
        f.ignore(1);

        int32_t value;
        f.read((char*)&value, 4);
        v = value; //on ignore le type pour l'instant
    }

    uint16_t strCount;
    f.read((char*)&strCount, 2);
    p.strings.resize(strCount);

    for (auto& s : p.strings) {
        uint16_t len;
        f.read((char*)&len, 2);
        s.resize(len);
        f.read(&s[0], len);
    }

    uint16_t funcCount;
    f.read((char*)&funcCount, 2);
    f.ignore(2);
    p.code.assign(std::istreambuf_iterator<char>(f), {});

    return p;
}

std::vector<uint8_t> ram(65536);

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: casm_emul program.bin" << std::endl;
        return 1;
    }

    Program p = load(argv[1]);
    for (size_t i = 0; i < p.code.size(); ++i) ram[i] = p.code[i];

    uint16_t pc = p.entryPoint;
    bool running = true;

    while (running) {
        uint8_t opcode = ram[pc++];
        std::cout << "PC=" << pc - 1 << " OPCODE=" << (int)opcode << std::endl;

        switch (opcode) {
            case PRT_STR: {
                uint16_t index = ram[pc++];
                std::cout << p.strings[index];
                break;
            } case EXT: {
                std::cout << std::endl << "[EXT] Program finished" << std::endl;
                running = false;
                break;
            }
            
            default: {
                std::cout << std::endl << "Unknown opcode" << std::endl;
                running = false;
                break;
            }
        }
    }

    return 0;
}

//magnus carlsen 2024-06
//TCHOUPI