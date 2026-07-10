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
    GPU_E,
    IPF,
    GTC
};

struct Operand {
    bool isConst = false;
    bool isFloat = false;
    int i = 0;
    float f = 0.0f;
    uint32_t str = 0;
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
    int map_index;
    int s;
};
GPU_registers GPU_reg;

struct Inputs_registers {
    bool a;
    bool b;
    bool c;
    bool d;
    bool e;
    bool f;
    bool g;
    bool h;
    bool i;
    bool j;
    bool k;
    bool l;
    bool m;
    bool n;
    bool o;
    bool p;
    bool q;
    bool r;
    bool s;
    bool t;
    bool u;
    bool v;
    bool w;
    bool x;
    bool y;
    bool z;
    bool n1;
    bool n2;
    bool n3;
    bool n4;
    bool n5;
    bool n6;
    bool n7;
    bool n8;
    bool n9;
    bool n0;
    bool space;
    bool enter;
    bool maj;
    bool left_click;
    bool right_click;
    bool up;
    bool down;
    bool left;
    bool right;
    bool comma;
    bool point;
};
Inputs_registers Inputs_reg;

class CPU {
public:
    Loader loader;
    GPU gpu;

    int ipf = 10;

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
        } else if (b == 0xFD) {
            op.isConst = true;
            op.isFloat = false;
            uint16_t strIndex = r16();
            if (strIndex < strings.size()) op.str = strIndex;
            else {
                std::cerr << "Erreur: index de string hors limites (" << strIndex << " >= " << strings.size() << ")" << std::endl;
                op.str = 0; //valeur par defaut si l'index est hors limites
            }
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
        uint16_t value = loader.code[loader.pc];
        value |= loader.code[loader.pc + 1] << 8;
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
            case 8: GPU_reg.map_index = vars[var].i; break;
        }
    }

    void maj_Inputs_registers() {
        Inputs_reg.a = IsKeyDown(KEY_Q);
        Inputs_reg.b = IsKeyDown(KEY_B);
        Inputs_reg.c = IsKeyDown(KEY_C);
        Inputs_reg.d = IsKeyDown(KEY_D);
        Inputs_reg.e = IsKeyDown(KEY_E);
        Inputs_reg.f = IsKeyDown(KEY_F);
        Inputs_reg.g = IsKeyDown(KEY_G);
        Inputs_reg.h = IsKeyDown(KEY_H);
        Inputs_reg.i = IsKeyDown(KEY_I);
        Inputs_reg.j = IsKeyDown(KEY_J);
        Inputs_reg.k = IsKeyDown(KEY_K);
        Inputs_reg.l = IsKeyDown(KEY_L);
        Inputs_reg.m = IsKeyDown(KEY_COMMA);
        Inputs_reg.n = IsKeyDown(KEY_N);
        Inputs_reg.o = IsKeyDown(KEY_O);
        Inputs_reg.p = IsKeyDown(KEY_P);
        Inputs_reg.q = IsKeyDown(KEY_A);
        Inputs_reg.r = IsKeyDown(KEY_R);
        Inputs_reg.s = IsKeyDown(KEY_S);
        Inputs_reg.t = IsKeyDown(KEY_T);
        Inputs_reg.u = IsKeyDown(KEY_U);
        Inputs_reg.v = IsKeyDown(KEY_V);
        Inputs_reg.w = IsKeyDown(KEY_Z);
        Inputs_reg.x = IsKeyDown(KEY_X);
        Inputs_reg.y = IsKeyDown(KEY_Y);
        Inputs_reg.z = IsKeyDown(KEY_W);
        Inputs_reg.n1 = IsKeyDown(KEY_ONE);
        Inputs_reg.n2 = IsKeyDown(KEY_TWO);
        Inputs_reg.n3 = IsKeyDown(KEY_THREE);
        Inputs_reg.n4 = IsKeyDown(KEY_FOUR);
        Inputs_reg.n5 = IsKeyDown(KEY_FIVE);
        Inputs_reg.n6 = IsKeyDown(KEY_SIX);
        Inputs_reg.n7 = IsKeyDown(KEY_SEVEN);
        Inputs_reg.n8 = IsKeyDown(KEY_EIGHT);
        Inputs_reg.n9 = IsKeyDown(KEY_NINE);
        Inputs_reg.n0 = IsKeyDown(KEY_ZERO);
        Inputs_reg.space = IsKeyDown(KEY_SPACE);
        Inputs_reg.enter = IsKeyDown(KEY_ENTER);
        Inputs_reg.maj = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        Inputs_reg.left_click = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        Inputs_reg.right_click = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
        Inputs_reg.up = IsKeyDown(KEY_UP);
        Inputs_reg.down = IsKeyDown(KEY_DOWN);
        Inputs_reg.left = IsKeyDown(KEY_LEFT);
        Inputs_reg.right = IsKeyDown(KEY_RIGHT);
        Inputs_reg.comma = IsKeyDown(KEY_M);
        Inputs_reg.point = IsKeyDown(KEY_SEMICOLON);
    }

    void exe_instruction() {
        if (loader.pc >= loader.code.size()) {
            std::cerr << "Erreur: PC hors limites (" << loader.pc << " >= " << loader.code.size() << ")" << std::endl;
            running = false;
            return;
        }

        if (vars.size() >= 81) {
            vars[35].i = Inputs_reg.a;
            vars[36].i = Inputs_reg.b;
            vars[37].i = Inputs_reg.c;
            vars[38].i = Inputs_reg.d;
            vars[39].i = Inputs_reg.e;
            vars[40].i = Inputs_reg.f;
            vars[41].i = Inputs_reg.g;
            vars[42].i = Inputs_reg.h;
            vars[43].i = Inputs_reg.i;
            vars[44].i = Inputs_reg.j;
            vars[45].i = Inputs_reg.k;
            vars[46].i = Inputs_reg.l;
            vars[47].i = Inputs_reg.m;
            vars[48].i = Inputs_reg.n;
            vars[49].i = Inputs_reg.o;
            vars[50].i = Inputs_reg.p;
            vars[51].i = Inputs_reg.q;
            vars[52].i = Inputs_reg.r;
            vars[53].i = Inputs_reg.s;
            vars[54].i = Inputs_reg.t;
            vars[55].i = Inputs_reg.u;
            vars[56].i = Inputs_reg.v;
            vars[57].i = Inputs_reg.w;
            vars[58].i = Inputs_reg.x;
            vars[59].i = Inputs_reg.y;
            vars[60].i = Inputs_reg.z;
            vars[61].i = Inputs_reg.n1;
            vars[62].i = Inputs_reg.n2;
            vars[63].i = Inputs_reg.n3;
            vars[64].i = Inputs_reg.n4;
            vars[65].i = Inputs_reg.n5;
            vars[66].i = Inputs_reg.n6;
            vars[67].i = Inputs_reg.n7;
            vars[68].i = Inputs_reg.n8;
            vars[69].i = Inputs_reg.n9;
            vars[70].i = Inputs_reg.n0;
            vars[71].i = Inputs_reg.space;
            vars[72].i = Inputs_reg.enter;
            vars[73].i = Inputs_reg.maj;
            vars[74].i = Inputs_reg.left_click;
            vars[75].i = Inputs_reg.right_click;
            vars[76].i = Inputs_reg.up;
            vars[77].i = Inputs_reg.down;
            vars[78].i = Inputs_reg.left;
            vars[79].i = Inputs_reg.right;
            vars[80].i = Inputs_reg.comma;
            vars[81].i = Inputs_reg.point;
        }

        uint8_t opcode = loader.code[loader.pc++];
        switch (opcode) {
            case IPF: {
                int val = 0;
                for (int i = 0; i < 4; i++) val |= (loader.code[loader.pc++] << (i * 8));
                
                ipf = val;
                if (ipf < 1) ipf = 1;
                break;
            } case SET: {
                uint8_t dst = r8();
                uint8_t src = r8();
                if (src != 0xFF && src != 0xFE && src >= vars.size()) {
                    std::cerr << "Erreur: index source invalide\n";
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
                    if (vars[src].type == 5) {
                        vars[dst].type = 1;
                        vars[dst].i = vars[src].i;
                        maj_GPU_registers(dst);
                    } else {
                        vars[dst] = vars[src];
                        if (vars[dst].type == 1) maj_GPU_registers(dst);
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

                    strings.push_back(strings[idx1] + strings[idx2]);
                    vars[d].i = strings.size() - 1;
                } else {

                    float v1 = (vars[d].type == 3) ? vars[d].f : vars[d].i;

                    float v2;

                    if (b.isConst) v2 = b.isFloat ? b.f : b.i;
                    else v2 = (vars[b.var].type == 3) ? vars[b.var].f : vars[b.var].i;

                    float r = v1 + v2;

                    if (vars[d].type == 3) vars[d].f = r;
                    else vars[d].i = (int)r;

                    if (vars[d].type == 1) maj_GPU_registers(d);
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
                float v2;

                if (b.isConst) v2 = b.isFloat ? b.f : b.i;
                else v2 = (vars[b.var].type == 3) ? vars[b.var].f : vars[b.var].i;
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
                if (vars[d].type == 3) vars[d].f = r;
                else vars[d].i = static_cast<int>(r);
                if (vars[d].type == 1) maj_GPU_registers(d);
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
            } case GTC: {
                uint8_t dst = r8();
                uint8_t strVar = r8();
                uint8_t idxVar = r8();

                if (strVar >= vars.size() || idxVar >= vars.size() || dst >= vars.size()) {
                    std::cerr << "GTC: indice de variable invalide." << std::endl;
                    running = false;
                    return;
                }
                if (vars[strVar].type != 2) {
                    std::cerr << "GTC: la deuxieme variable doit etre une string" << std::endl;
                    running = false;
                    return;
                }

                uint32_t stringIndex = static_cast<uint32_t>(vars[strVar].i);
                if (stringIndex >= strings.size()) {
                    std::cerr << "GTC: index de chaine invalide." << std::endl;
                    running = false;
                    return;
                }

                int charIndex = vars[idxVar].i;
                if (charIndex < 0 || charIndex >= (int)strings[stringIndex].size()) vars[dst].i = 0;
                else vars[dst].i = static_cast<unsigned char>(strings[stringIndex][charIndex]);

                break;
            } case AND:
            case OR:
            case NND:
            case NOR: {
                Operand a = readOperand();
                Operand b = readOperand();

                int v1 = a.isConst ? a.i : vars[a.var].i;
                int v2 = b.isConst ? b.i : vars[b.var].i;

                //verifie que ce sont des bools
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

                            if (GPU_reg.map_index >= 0 && GPU_reg.map_index < loader.maps.size()) {
                                //on passe le vecteur de pixels de la map selectionnee
                                gpu.drawmap(GPU_reg.x, GPU_reg.y, GPU_reg.w, GPU_reg.h, scale, loader.maps[GPU_reg.map_index].pixels.data());
                            } else {
                                std::cerr << "Erreur: Index de map invalide: " << GPU_reg.map_index << std::endl;
                                running = false;
                            }
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