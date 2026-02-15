#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>

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
    RET
};

struct Variable {
    uint8_t type; // 1=int, 2=string, 3=float, 4=bool
    int i = 0;
    float f = 0.0f;
    std::string s;
};

struct Operand {
    bool isConst = false;
    bool isFloat = false;
    int i = 0;
    float f = 0.0f;
    uint8_t var = 0;
};

class VM {
public:
    VM(bool debug_mode = false) : debug_mode(debug_mode) {}
    bool load(const std::string& file);
    void run();

private:

    bool debug_mode;

    std::vector<Variable> vars;
    std::vector<std::string> strings;
    std::vector<uint8_t> code;

    size_t pc = 0;
    bool running = true;
    bool flag = false;

    std::vector<uint16_t> callStack;

    uint8_t r8();
    uint16_t r16();
    int32_t r32();
    float rFloat();

    Operand readOperand();
    void exec(uint8_t op);
};

uint8_t VM::r8() {
    return code[pc++];
}

uint16_t VM::r16() {
    uint16_t v = code[pc] | (code[pc + 1] << 8);
    pc += 2;
    return v;
}

int32_t VM::r32() {
    int32_t v = 0;
    v |= code[pc++];
    v |= code[pc++] << 8;
    v |= code[pc++] << 16;
    v |= code[pc++] << 24;
    return v;
}

float VM::rFloat() {
    float v;
    uint8_t* p = (uint8_t*)&v;
    p[0] = code[pc++];
    p[1] = code[pc++];
    p[2] = code[pc++];
    p[3] = code[pc++];
    return v;
}

Operand VM::readOperand() {
    Operand o;
    uint8_t m = r8();

    if (m == 0xFF) {
        o.isConst = true;
        o.i = r32();
    } else if (m == 0xFE) {
        o.isConst = true;
        o.isFloat = true;
        o.f = rFloat();
    } else {
        o.var = m;
    }
    return o;
}

bool VM::load(const std::string& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;

    char magic[4];
    f.read(magic, 4);
    if (std::string(magic, 4) != "CASM") return false;

    uint8_t version;
    uint16_t entry;
    f.read((char*)&version, 1);
    f.read((char*)&entry, 2);
    pc = entry;

    uint16_t varCount;
    f.read((char*)&varCount, 2);
    vars.resize(varCount);

    for (auto& v : vars) {
        f.read((char*)&v.type, 1);
        f.ignore(1);
        if (v.type == 3) f.read((char*)&v.f, 4);
        else f.read((char*)&v.i, 4);
    }

    uint16_t strCount;
    f.read((char*)&strCount, 2);
    strings.resize(strCount);

    for (auto& s : strings) {
        uint16_t len;
        f.read((char*)&len, 2);
        s.resize(len);
        f.read(&s[0], len);
    }

    uint16_t funcCount;
    f.read((char*)&funcCount, 2);
    f.ignore(2);

    code.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

void VM::run() {
    while (running && pc < code.size()) {
        exec(r8());
    }
}

void VM::exec(uint8_t op) {
    switch (op) {

        case PRT_VAR: {
            uint8_t varIndex = r8();  // Lit l'index de la variable
            if (vars[varIndex].type == 1) std::cout << vars[varIndex].i;
            else if (vars[varIndex].type == 3) std::cout << vars[varIndex].f;
            else {
                uint32_t strIndex = vars[varIndex].i;
                if (strIndex < strings.size()) std::cout << strings[strIndex];
                else std::cout << "[INVALID_STR]";
            }
            break;
        } case PRT_STR: {
            uint16_t strIndex = r16();  // Lit 2 bytes (uint16_t)
            if (strIndex < strings.size()) std::cout << strings[strIndex];
            else std::cout << "[INVALID_STR]";
            break;
        }

        case SET: {
            uint8_t i = r8();
            if (vars[i].type == 3) vars[i].f = rFloat();
            else vars[i].i = r32();
            break;
        }

        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case POW: {
            Operand a = readOperand();
            Operand b = readOperand();

            if (a.isConst && b.isConst) { running = false; return; }

            uint8_t d = a.isConst ? b.var : a.var;
            bool fl = a.isFloat || b.isFloat || vars[d].type == 3;

            float v1 = a.isConst ? (a.isFloat ? a.f : a.i)
                                : (vars[a.var].type == 3 ? vars[a.var].f : vars[a.var].i);
            float v2 = b.isConst ? (b.isFloat ? b.f : b.i)
                                : (vars[b.var].type == 3 ? vars[b.var].f : vars[b.var].i);

            float r = 0;
            if (op == ADD) r = v1 + v2;
            if (op == SUB) r = v1 - v2;
            if (op == MUL) r = v1 * v2;
            if (op == DIV) r = v1 / v2;
            if (op == POW) r = std::pow(v1, v2);

            if (fl) vars[d].f = r;
            else vars[d].i = (int)r;
            break;
        }

        case SQR: {
            Operand o = readOperand();
            if (vars[o.var].type == 3)
                vars[o.var].f = std::sqrt(vars[o.var].f);
            else
                vars[o.var].i = (int)std::sqrt(vars[o.var].i);
            break;
        }

        case AND:
        case OR:
        case NND:
        case NOR: {
            Operand a = readOperand();
            Operand b = readOperand();

            int v1 = a.isConst ? a.i : vars[a.var].i;
            int v2 = b.isConst ? b.i : vars[b.var].i;

            //verifie que ce sont des bools (0 ou 1)
            if ((v1 != 0 && v1 != 1) || (v2 != 0 && v2 != 1)) {
                std::cerr << "Erreur: porte logique utilisée sur non-bool\n";
                running = false;
                return;
            }

            switch(op) {
                case AND: flag = v1 && v2; break;
                case OR:  flag = v1 || v2; break;
                case NND: flag = !(v1 && v2); break;
                case NOR: flag = !(v1 || v2); break;
            }
            break;
        }

        case LIF: {
            Operand o = readOperand();

            float val;
            if (o.isConst) val =o.f;
            else {
                if (vars[o.var].type != 3) {
                    std::cerr << "Erreur: LIF attend un float\n";
                    running = false;
                    return;
                }
                val = vars[o.var].f;
            }
            flag = (std::fmod(val, 1.0f) == 0.0f); //true si pas de decimale
            break;
        }

        case CPR: {
            Operand a = readOperand();
            Operand b = readOperand();

            if (!a.isConst && !b.isConst && vars[a.var].type == 2 && vars[b.var].type == 2) {
                uint32_t idx1 = vars[a.var].i;
                uint32_t idx2 = vars[b.var].i;
                std::string s1 = (idx1 < strings.size()) ? strings[idx1] : "";
                std::string s2 = (idx2 < strings.size()) ? strings[idx2] : "";
                flag = (s1 == s2);

                if (debug_mode) std::cout << "[DEBUG] CPR (str): " << s1 << " == " << s2
                                << " ? " << flag << std::endl;
                break;
            }

            float v1 = a.isConst ? (a.isFloat ? a.f : a.i) : (vars[a.var].type == 3 ? vars[a.var].f : vars[a.var].i);
            float v2 = b.isConst ? (b.isFloat ? b.f : b.i) : (vars[b.var].type == 3 ? vars[b.var].f : vars[b.var].i);

            flag = (v1 == v2);

            if (debug_mode) std::cout << "[DEBUG] CPR: " << v1 << " == " << v2 << " ? " << flag << std::endl;
            break;
        } case CPG: {
            Operand a = readOperand();
            Operand b = readOperand();

            if (!a.isConst && !b.isConst && vars[a.var].type == 2 && vars[b.var].type == 2) {
                uint32_t idx1 = vars[a.var].i;
                uint32_t idx2 = vars[b.var].i;
                std::string s1 = (idx1 < strings.size()) ? strings[idx1] : "";
                std::string s2 = (idx2 < strings.size()) ? strings[idx2] : "";
                flag = (s1 > s2);

                if (debug_mode) std::cout << "[DEBUG] CPG (str): " << vars[a.var].s << " > "
                                << vars[b.var].s << " ? " << flag << std::endl;
                break;
            }

            float v1 = a.isConst ? (a.isFloat ? a.f : a.i) : (vars[a.var].type == 3 ? vars[a.var].f : vars[a.var].i);
            float v2 = b.isConst ? (b.isFloat ? b.f : b.i) : (vars[b.var].type == 3 ? vars[b.var].f : vars[b.var].i);
            flag = (v1 > v2);

            if (debug_mode) std::cout << "[DEBUG] CPG: " << v1 << " > " << v2 << " ? " << flag << std::endl;
            break;
        }

        case LCT: {
            uint8_t i = r8();
            std::string input;
            std::cin >> std::ws; 
            
            if (vars[i].type == 2) std::getline(std::cin, input);
            else std::cin >> input;

            if (vars[i].type == 3) vars[i].f = std::stof(input);
            else if (vars[i].type == 1) vars[i].i = std::stoi(input);
            else {
                strings.push_back(input);
                vars[i].i = strings.size() - 1;
            }
            break;
        } case JPT: {
            uint16_t addr = r16();
            if (debug_mode) std::cout << "[DEBUG] JPT to 0x" << std::hex << addr << " (flag=" << flag << ")" << std::endl;
            if (flag) pc = addr;
            break;
        } case JMP: {
            pc = r16(); //saute toujours a l'adresse lue
            break;
        } case JPF: { uint16_t a = r16(); if (!flag) pc = a; break; }

        case CAL: {
            callStack.push_back(pc);
            pc = r16();
            break;

        } case RET: {
            if (callStack.empty()) running = false;
            else { pc = callStack.back(); callStack.pop_back(); }
            break;

        } case EXT: {
            running = false;
            break;

        } default: {
            std::cerr << "Opcode inconnu: " << (int)op << "\n";
            running = false;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: casm_vm program.bin\n";
        return 1;
    }

    bool debug_mode = false;
    std::string filename;

    //verifie si -debug est présent
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-debug") {
            debug_mode = true;
        } else {
            filename = argv[i];
        }
    }

    VM vm(debug_mode); // Passe le mode debug à la VM
    if (!vm.load(filename)) {
        std::cerr << "Erreur chargement CASM\n";
        return 1;
    }
    vm.run();
    return 0;
}

//magnus carlsen 2024-06