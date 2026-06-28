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
    int s;
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
    uint64_t sleep_until = 0;
    bool is_sleeping = false;

    bool load(const std::string& file) {
        running = true;
        if (!loader.load(file)) return false;

        vars.clear();
        vars.resize(loader.vars.size());
        for (size_t i = 0; i < loader.vars.size(); ++i) {
            const Variable& lv = loader.vars[i];
            Var v{};
            v.type = lv.type;
            switch (lv.type) {
                case 1: {
                    v.i = lv.i;
                    break;
                } case 3: {
                    v.f = lv.f;
                    break;
                } default: {
                    v.i = static_cast<int32_t>(lv.index);
                    break;
                }
            }
            vars[i] = v;
        }

        strings = loader.strings;
        return true;
    }

    Operand readOperand() {
        Operand op{};
        uint8_t b = r8();

        if (b == 0xFF) {
            op.isConst = true;
            op.isFloat = false;
            op.i = r32();
        } else if (b == 0xFE) {
            op.isConst = true;
            op.isFloat = true;
            op.f = rFloat();
        } else {
            op.isConst = false;
            op.var = b;
        }

        return op;
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

    int32_t r32() {
        int32_t v = 0;
        v |= loader.code[loader.pc++];
        v |= loader.code[loader.pc++] << 8;
        v |= loader.code[loader.pc++] << 16;
        v |= loader.code[loader.pc++] << 24;
        return v;
    }

    float rFloat() {
        float v;
        uint8_t* p = (uint8_t*)&v;
        p[0] = loader.code[loader.pc++];
        p[1] = loader.code[loader.pc++];
        p[2] = loader.code[loader.pc++];
        p[3] = loader.code[loader.pc++];
        return v;
    }

    //lecture des registres GPU a partir des variables
    void maj_GPU_registers(uint8_t var) {
        switch (var) {
            case 0: GPU_reg.x = vars[var].i; break;
            case 1: GPU_reg.y = vars[var].i; break;
            case 2: GPU_reg.w = vars[var].i; break;
            case 3: GPU_reg.h = vars[var].i; break;
            case 4: GPU_reg.t = vars[var].i; break;
            case 5: GPU_reg.color = vars[var].i; break;
            case 6: GPU_reg.r = vars[var].i; break;
            case 7: GPU_reg.s = vars[var].i; break;
        }
    }

    void exe_instruction() {
        if (loader.pc >= loader.code.size()) {
            std::cerr << "Erreur: PC hors limites (" << loader.pc << " >= " << loader.code.size() << ")" << std::endl;
            running = false;
            return;
        }
        uint8_t opcode = loader.code[loader.pc++];
        switch (opcode) {
            case SET: {
                uint8_t dst = r8();
                uint8_t src = r8();
                if (dst >= vars.size()) {
                    std::cerr << "Erreur: index de variable invalide: " << dst << std::endl;
                    running = false;
                    break;
                }

                if (src == 0xFF) {
                    vars[dst].type = 1;
                    vars[dst].i = r32();
                    maj_GPU_registers(dst);
                } else if (src == 0xFE) {
                    vars[dst].type = 3;
                    vars[dst].f = rFloat();
                } else {
                    vars[dst] = vars[src];

                    if(vars[src].type == 1) maj_GPU_registers(dst);
                    if(vars[src].type == 5) {
                        std::cerr << "Erreur: impossible de modifier un tableau" << std::endl;
                        running = false;
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
                break;
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
                        break;
                    } case NOR: {
                        flag = !(v1 || v2);
                        break;
                    }
                    break;
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

                //active slp pendant le temps specifie
                sleep_until = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + ms;
                is_sleeping = true;
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
                if (GPU_reg.color >= 0 && GPU_reg.color < 25) {
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
                            int scale;
                            if (GPU_reg.s <= 1) scale = 1;
                            else scale = GPU_reg.s;
                            gpu.drawmap(GPU_reg.x, GPU_reg.y, GPU_reg.w, GPU_reg.h, scale, GPU_reg.map);
                            break;
                        } default: {
                            std::cerr << "Erreur: action GPU inconnue: " << action << std::endl;
                            running = false;
                            break;
                        }
                    }
                } else {
                    std::cerr << "Erreur: couleur GPU invalide: " << GPU_reg.color << std::endl;
                    running = false;
                }
                break;
            } default: {
                std::cerr << "Unknown opcode: " << (int)opcode << std::endl;
                running = false;
                break;
            }
        }
    }
};

//magnus carlsen 2024-06 for ГПСД, XS проект