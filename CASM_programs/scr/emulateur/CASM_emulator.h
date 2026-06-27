#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include "CASM_gpu.h"
#include "CASM_loader.h"

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
    RET,
    SLP,
    TME,
    GPU_E
};

struct Operand {
    bool isConst = false;
    bool isFloat = false;
    int i = 0;
    float f = 0.0f;
    uint8_t var = 0;
};

struct Var {
    uint8_t type = 0;
    int32_t i = 0;
    float f = 0.0f;
    std::string name;
};

struct GPU_registers {
    int x;
    int y;
    int w;
    int h;
    int t;
    int color;
    int r;
    uint8_t* map;
};
GPU_registers GPU_reg;

class CPU {
public:
    Loader loader;
    GPU gpu;

    std::vector<Var> vars;
    std::vector<Var> dataStack;
    std::vector<std::string> strings;
    std::vector<uint16_t> callStack;
    bool running = true;
    bool flag = false;

    bool load(const std::string& file) {
        running = true;
        return loader.load(file);
    }

    Operand readOperand() {
        if (loader.pc >= loader.code.size()) {
            Operand empty{};
            empty.isConst = false;
            empty.var = 0;
            return empty;
        }

        uint8_t operand = loader.code[loader.pc++];
        bool isConst = (operand & 0x80) != 0;
        uint8_t varIndex = operand & 0x7F;

        Operand result{};
        result.isConst = isConst;
        result.var = varIndex;
        return result;
    }

    //lit un entier 8 bits (1 octet) a partir du code charge
    uint8_t r8() {
        if (loader.pc >= loader.code.size()) return 0;
        return loader.code[loader.pc++];
    }

    //lit un entier 16 bits (2 octets) a partir du code charge
    uint16_t r16() {
        if (loader.pc + 1 >= loader.code.size()) return 0;
        uint16_t value = (loader.code[loader.pc] << 8) | loader.code[loader.pc + 1];
        loader.pc += 2;
        return value;
    }

    //lecture des registres GPU a partir des variables
    void maj_GPU_registers(uint16_t var) {
        if (vars[var].name == "i_GPU_x") GPU_reg.x = vars[var].i;
        else if (vars[var].name == "i_GPU_y") GPU_reg.y = vars[var].i;
        else if (vars[var].name == "i_GPU_w") GPU_reg.w = vars[var].i;
        else if (vars[var].name == "i_GPU_h") GPU_reg.h = vars[var].i;
        else if (vars[var].name == "i_GPU_t") GPU_reg.t = vars[var].i;
        else if (vars[var].name == "i_GPU_color") GPU_reg.color = vars[var].i;
        else if (vars[var].name == "i_GPU_r") GPU_reg.r = vars[var].i;
        else return;
    }

    void exe_instruction() {
        uint8_t opcode = loader.code[loader.pc++];
        switch(opcode) {
            case SET: {
                uint8_t dst = loader.code[loader.pc++]; //destination
                uint8_t src = loader.code[loader.pc++]; //source

                vars[dst].type = vars[src].type;
                switch (vars[src].type) {
                    case 1: { //int
                        maj_GPU_registers(src);
                        vars[dst].i = vars[src].i;
                        break;
                    } case 3: { //float
                        vars[dst].f = vars[src].f;
                        break;
                    } case 2: { //string
                        vars[dst].i = vars[src].i;
                        break;
                    } case 4: { //bool
                        vars[dst].i = vars[src].i;
                        break;
                    } case 5: { //map
                        std::cerr << "Erreur: impossible de modifier un tableau" << std::endl;
                        running = false;
                        break;
                    }
                }
                break;
            } case EXT: {
                running = false;
                break;
            } case ADD: {
                Operand a = readOperand();
                Operand b = readOperand();

                if (a.isConst) {
                    std::cerr << "Erreur: la destination ne peut pas etre une constante" << std::endl;
                    running = false;
                    return;
                }

                uint8_t d = a.var;

                if (d >= vars.size()) {
                    std::cerr << "Index variable invalide" << std::endl;
                    running = false;
                    return;
                }

                if (vars[d].type == 2) {
                    if (b.isConst) {
                        std::cerr << "Erreur: impossible d'utiliser une constante dans une operation string" << std::endl;
                        running = false;
                        return;
                    }
                    if (vars[b.var].type != 2) {
                        std::cerr << "Erreur ADD string: operande doit etre une string" << std::endl;
                        running = false;
                        return;
                    }

                    uint32_t idx1 = vars[d].i;
                    uint32_t idx2 = vars[b.var].i;

                    if (idx1 >= strings.size() || idx2 >= strings.size()) {
                        std::cerr << "Index string invalide" << std::endl;
                        running = false;
                        return;
                    }

                    std::string newStr = strings[idx1] + strings[idx2];
                    strings.push_back(newStr);
                    vars[d].i = strings.size() - 1;
                }
                break;
            } case SUB:
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
                float v2 = (vars[b.var].type == 3) ? vars[b.var].f : vars[b.var].i;
                float r = 0;

                switch(opcode) {
                    case SUB: {
                        r = v1 - v2;
                        break;
                    } case MUL: {
                        r = v1 * v2;
                        break;
                    } case DIV: {
                        r = v1 / v2;
                        break;
                    } case POW: {
                        r = std::pow(v1, v2);
                        break;
                    }
                }
            } case SQR: {
                Operand o = readOperand();
                if (vars[o.var].type == 3) vars[o.var].f = std::sqrt(vars[o.var].f);
                else vars[o.var].i = (int)std::sqrt(vars[o.var].i);
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
                break;
            } case AND:
            case OR:
            case NND:
            case NOR: {
                Operand a = readOperand();
                Operand b = readOperand();

                int v1 = a.isConst ? a.i : vars[a.var].i;
                int v2 = b.isConst ? b.i : vars[b.var].i;

                //verifie que ce sont des bools (0 ou 1)
                if ((v1 != 0 && v1 != 1) || (v2 != 0 && v2 != 1)) {
                    std::cerr << "Erreur: porte logique utilisee sur non-bool" << std::endl;
                    running = false;
                    return;
                }

                switch(opcode) {
                    case AND: {
                        flag = v1 && v2;
                        break;
                    } case OR: {
                        flag = v1 || v2;
                        break;
                    } case NND: {
                        flag = !(v1 && v2);
                    } case NOR: {
                        flag = !(v1 || v2);
                        break;
                    }
                }
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
                    break;
                }

                float v1 = a.isConst ? (a.isFloat ? a.f : a.i) : (vars[a.var].type == 3 ? vars[a.var].f : vars[a.var].i);
                float v2 = b.isConst ? (b.isFloat ? b.f : b.i) : (vars[b.var].type == 3 ? vars[b.var].f : vars[b.var].i);
                flag = (v1 == v2);
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
                    break;
                }

                float v1 = a.isConst ? (a.isFloat ? a.f : a.i) : (vars[a.var].type == 3 ? vars[a.var].f : vars[a.var].i);
                float v2 = b.isConst ? (b.isFloat ? b.f : b.i) : (vars[b.var].type == 3 ? vars[b.var].f : vars[b.var].i);
                flag = (v1 > v2);
                break;
            } case JMP: {
                loader.pc = r16();
                break;
            } case JPT: {
                uint16_t addr = r16();
                if (flag) loader.pc = addr;
                break;
            } case JPF: {
                uint16_t addr = r16();
                if (!flag) loader.pc = addr;
                break;
            } case CAL: {
                uint16_t addr = r16();
                callStack.push_back(loader.pc);
                loader.pc = addr;
                break;
            } case PSH: {
                Operand o = readOperand();
                Var v;

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
                break;
            } case RET: {
                if (callStack.empty()) running = false;
                else {
                    loader.pc = callStack.back();
                    callStack.pop_back();
                }
                break;
            } case SLP: {
                Operand o = readOperand();
                int ms = o.isConst ? o.i : (vars[o.var].type == 3 ? (int)vars[o.var].f : vars[o.var].i);
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                break;
            } case TME: {
                uint8_t dst = r8();
                int timestamp = static_cast<int>(std::time(nullptr));
                
                if (vars[dst].type == 1) vars[dst].i = timestamp;
                else if (vars[dst].type == 3) vars[dst].f = (float)timestamp;
                else {
                    std::cerr << "TME: La variable destination doit etre un int ou un float" << std::endl;
                    running = false;
                }
                break;
            } case GPU_E: {
                uint8_t action = r8();
                switch(action) {
                    case 0: {
                        gpu.pixel(GPU_reg.x, GPU_reg.y, gpu.palette[GPU_reg.color]);
                        break;
                    } case 1: {
                        gpu.rect(GPU_reg.x, GPU_reg.y, GPU_reg.w, GPU_reg.h, gpu.palette[GPU_reg.color]);
                        break;
                    } case 2: {
                        gpu.voidrect(GPU_reg.x, GPU_reg.y, GPU_reg.w, GPU_reg.h, GPU_reg.t, gpu.palette[GPU_reg.color]);
                        break;
                    } case 3: {
                        gpu.circle(GPU_reg.x, GPU_reg.y, GPU_reg.r, gpu.palette[GPU_reg.color]);
                        break;
                    } case 4: {
                        gpu.voidcircle(GPU_reg.x, GPU_reg.y, GPU_reg.r, GPU_reg.t, gpu.palette[GPU_reg.color]);
                        break;
                    } case 5: {
                        gpu.drawmap(GPU_reg.x, GPU_reg.y, GPU_reg.w, GPU_reg.h, GPU_reg.map);
                        break;
                    }
                }
            } default: {
                std::cerr << "Unknown opcode: " << (int)opcode << std::endl;
                break;
            }
        }
    }
};

//magnus carlsen 2024-06 for ГПСД, XS проект