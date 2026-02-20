#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <random>

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
    std::vector<Variable> dataStack;

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
        uint8_t op = r8();
        if (debug_mode) std::cout << "[DEBUG] EXEC OPCODE=" << (int)op << " AT PC=" << (pc - 1) << std::endl;
        exec(op);
    }
}

void VM::exec(uint8_t op) {
    switch (op) {
        case PRT_VAR: {
            uint8_t varIndex = r8(); //lit l'index de la variable
            if (vars[varIndex].type == 1) std::cout << vars[varIndex].i;
            else if (vars[varIndex].type == 3) std::cout << vars[varIndex].f;
            else {
                uint32_t strIndex = vars[varIndex].i;
                if (strIndex < strings.size()) std::cout << strings[strIndex];
                else std::cout << "[INVALID_STR]";
            }
            break;
        } case PRT_STR: {
            uint16_t strIndex = r16(); //lit 2 bytes (uint16_t)
            if (strIndex < strings.size()) std::cout << strings[strIndex];
            else std::cout << "[INVALID_STR]";
            break;
        } case SET: {
            uint8_t dst = code[pc++]; //destination
            uint8_t src = code[pc++]; //source

            //copie la valeur selon le type
            vars[dst].type = vars[src].type;
            switch (vars[src].type) {
                case 1: //int
                    vars[dst].i = vars[src].i;
                    break;
                case 3: //float
                    vars[dst].f = vars[src].f;
                    break;
                case 2: //string
                    vars[dst].s = vars[src].s;
                    break;
                case 4: //bool
                    vars[dst].i = vars[src].i;
                    break;
            }
            if (debug_mode) std::cout << "[DEBUG] SET var " << (int)dst << " = var " << (int)src << std::endl;
            break;
        }

        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case POW: {
            Operand a = readOperand();
            Operand b = readOperand();

            if (a.isConst) {
                std::cerr << "Erreur: destination ne peut pas etre constante" << std::endl;
                running = false;
                return;
            }

            uint8_t d = a.var;

            if (d >= vars.size()) {
                std::cerr << "Index variable invalide" << std::endl;
                running = false;
                return;
            }

            float v1 = (vars[d].type == 3) ? vars[d].f : vars[d].i;
            float v2 = b.isConst ? (b.isFloat ? b.f : b.i) : (vars[b.var].type == 3 ? vars[b.var].f : vars[b.var].i);
            float r = 0;

            switch(op) {
                case ADD:
                    r = v1 + v2;
                    if (debug_mode) std::cout << "[DEBUG] ADD: " << v1 << " + " << v2 << " = " << r << std::endl;
                    break;
                case SUB:
                    r = v1 - v2;
                    if (debug_mode) std::cout << "[DEBUG] SUB: " << v1 << " - " << v2 << " = " << r << std::endl;
                    break;
                case MUL:
                    r = v1 * v2;
                    if (debug_mode) std::cout << "[DEBUG] MUL: " << v1 << " * " << v2 << " = " << r << std::endl;
                    break;
                case DIV:
                    r = v1 / v2;
                    if (debug_mode) std::cout << "[DEBUG] DIV: " << v1 << " / " << v2 << " = " << r << std::endl;
                    break;
                case POW:
                    r = std::pow(v1, v2);
                    if (debug_mode) std::cout << "[DEBUG] POW: " << v1 << " ** " << v2 << " = " << r << std::endl;
                    break;
            }

            if (vars[d].type == 3) vars[d].f = r;
            else vars[d].i = (int)r;

            break;
        } case SQR: {
            Operand o = readOperand();
            if (vars[o.var].type == 3) vars[o.var].f = std::sqrt(vars[o.var].f);
            else vars[o.var].i = (int)std::sqrt(vars[o.var].i);
            if (debug_mode) {
                if (vars[o.var].type == 3) std::cout << "[DEBUG] SQR: sqrt(" << vars[o.var].f << ") = " << vars[o.var].f << std::endl;
                else std::cout << "[DEBUG] SQR: sqrt(" << vars[o.var].i << ") = " << vars[o.var].i << std::endl;
            }
            break;
        } case RND: {
            uint8_t varIndex = r8();
            Operand maximum = readOperand();

            int max_val = 0;
            if (maximum.isConst) max_val = maximum.i;
            else {
                if (vars[maximum.var].type != 1) {
                    std::cerr << "RND: second argument doit etre un int" << std::endl;
                    running = false;
                    return;
                }
                max_val = vars[maximum.var].i;
            }

            if (vars[varIndex].type != 1) {
                std::cerr << "RND: variable cible doit etre un int" << std::endl;
                running = false;
                return;
            }

            vars[varIndex].i = std::rand() % max_val;

            if (debug_mode) std::cout << "[DEBUG] RND: rand num generated = " << vars[varIndex].i << " in var " << (int)varIndex << std::endl;
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
                std::cerr << "Erreur: porte logique utilisée sur non-bool" << std::endl;
                running = false;
                return;
            }

            switch(op) {
                case AND:
                    flag = v1 && v2;
                    if (debug_mode) std::cout << "[DEBUG] AND: " << v1 << " && " << v2 << " = " << flag << std::endl;
                    break;
                case OR:
                    flag = v1 || v2;
                    if (debug_mode) std::cout << "[DEBUG] OR: " << v1 << " || " << v2 << " = " << flag << std::endl;
                    break;
                case NND:
                    flag = !(v1 && v2);
                    if (debug_mode) std::cout << "[DEBUG] NND: !(" << v1 << " && " << v2 << ") = " << flag << std::endl;
                    break;
                case NOR:
                    flag = !(v1 || v2);
                    if (debug_mode) std::cout << "[DEBUG] NOR: !(" << v1 << " || " << v2 << ") = " << flag << std::endl;
                    break;
            }
            break;
        } case LIF: {
            Operand o = readOperand();

            float val;
            if (o.isConst) val =o.f;
            else {
                if (vars[o.var].type != 3) {
                    std::cerr << "Erreur: LIF attend un float" << std::endl;
                    running = false;
                    return;
                }
                val = vars[o.var].f;
            }
            flag = (std::fmod(val, 1.0f) == 0.0f); //true si pas de decimale
            if (debug_mode) std::cout << "[DEBUG] LIF: " << val << " = int?: " << flag << std::endl;
            break;
        } case CPR: {
            Operand a = readOperand();
            Operand b = readOperand();

            if (!a.isConst && !b.isConst && vars[a.var].type == 2 && vars[b.var].type == 2) {
                uint32_t idx1 = vars[a.var].i;
                uint32_t idx2 = vars[b.var].i;
                std::string s1 = (idx1 < strings.size()) ? strings[idx1] : "";
                std::string s2 = (idx2 < strings.size()) ? strings[idx2] : "";
                flag = (s1 == s2);

                if (debug_mode) std::cout << "[DEBUG] CPR (str): " << s1 << " == " << s2 << " ? " << flag << std::endl;
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
        } case LCT: {
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

            if (debug_mode) {
                std::cout << "[DEBUG] LCT: var " << i << " = " << input << std::endl;
                std::cout << "[DEBUG INPUT RAW] " << input << std::endl;
            }
            break;
        } case JPT: {
            uint16_t addr = r16();
            if (debug_mode) std::cout << "[DEBUG] JPT: to 0x" << std::hex << addr << " (flag=" << flag << ")" << std::endl;
            if (flag) pc = addr;
            break;
        } case JMP: {
            pc = r16();
            if (debug_mode) std::cout << "[DEBUG] JMP: to 0x" << std::hex << pc << " (flag=" << flag << ")" << std::endl;
            break;
        } case JPF: {
            uint16_t a = r16();
            if (!flag) pc = a;
            if (debug_mode) std::cout << "[DEBUG] JPF: to 0x" << std::hex << a << " (flag=" << flag << ")" << std::endl;
            break;
        } case CAL: {
            uint16_t addr = r16();
            callStack.push_back(pc);
            pc = addr;
            if (debug_mode) std::cout << "[DEBUG] CAL: to 0x" << std::hex << addr << " (flag=" << flag << ")" << std::endl;
            break;
        } case PSH: {
            Operand o = readOperand();
            Variable v;

            if (o.isConst) {
                if (o.isFloat) {
                    v.type = 3;
                    v.f = o.f;
                } else {
                    v.type = 1;
                    v.i = o.i;
                }
            } else v = vars[o.var];

            dataStack.push_back(v);

            if (debug_mode) std::cout << "[DEBUG] PSH: stack size = " << dataStack.size() << std::endl;
            break;
        } case LOD: {
            uint8_t dst = r8();

            if (dataStack.empty()) {
                std::cerr << "Stack underflow" << std::endl;
                running = false;
                return;
            } else if (dataStack.size() > 256) {
                std::cerr << "Data stack overflow" << std::endl;
                running = false;
            }

            vars[dst] = dataStack.back();
            dataStack.pop_back();

            if (debug_mode) std::cout << "[DEBUG] LOD: var " << (int)dst << " loaded, stack size = " << dataStack.size() << std::endl;
            break;
        } case RET: {
            if (callStack.empty()) running = false;
            else {
                pc = callStack.back();
                callStack.pop_back();
            }
            if (debug_mode) std::cout << "[DEBUG] RET: returning to 0x" << std::hex << pc << " (flag=" << flag << ")" << std::endl;
            break;

        } case EXT: {
            running = false;
            if (debug_mode) std::cout << "[DEBUG] EXT: exiting program" << std::endl;
            break;

        } default: {
            std::cerr << "Opcode inconnu: " << (int)op << std::endl;
            running = false;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: casm_vm program.bin" << std::endl;
        return 1;
    }

    bool debug_mode = false;
    std::string filename;

    //verifie si -debug est present
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-debug") {
            debug_mode = true;
        } else {
            filename = argv[i];
        }
    }

    VM vm(debug_mode); //passe le mode debug a la VM
    if (!vm.load(filename)) {
        std::cerr << "Erreur chargement CASM" << std::endl;
        return 1;
    }
    vm.run();
    return 0;
}

//magnus carlsen 2024-06