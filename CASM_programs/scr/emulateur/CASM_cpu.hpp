#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <array>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <chrono>
#include "CASM_gpu.hpp"
#include "CASM_bios.hpp"

class CPU {
private:
    enum opcodes : uint8_t {
        SET = 1,
        EXT,

        //ARITHMETIQUE
        //int
        ADI,
        SBI,
        MLI,
        DVI,

        //float
        ADF,
        SBF,
        MLF,
        DVF,

        //COMPARAISON
        CMP,
        CMPF,
        CPG,
        CPGF,

        //SAUTS
        JMP,
        JPT,
        JPF,

        //DATAS
        CALL,
        PUSH,
        LOAD,
        RET,

        //SYSTEM
        GPU_S,
        IPF,
        SLP,
        TME
    };

    enum input_index : uint32_t {
        IN_A, IN_B, IN_C, IN_D, IN_E, IN_F, IN_G, IN_H, IN_I, IN_J, IN_K, IN_L, IN_M, IN_N,
        IN_O, IN_P, IN_Q, IN_R, IN_S, IN_T, IN_U, IN_V, IN_W, IN_X, IN_Y, IN_Z,

        IN_0, IN_1, IN_2, IN_3, IN_4, IN_5, IN_6, IN_7, IN_8, IN_9,

        IN_COMMA, IN_PERIOD,

        IN_SPACE, IN_ENTER, IN_BACKSPACE,

        IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT,

        INPUT_COUNT
    };

    enum gpu_operations : uint8_t {
        GPU_PIXEL = 1,
        GPU_RECT,
        GPU_VOIDRECT,
        GPU_CIRCLE,
        GPU_VOIDCIRCLE,
        GPU_CLEAR,
        GPU_DRAWMAP
    };

    static constexpr uint32_t INPUT_BASE = 0xFFFF0000u;
    static constexpr uint32_t INPUT_SLOT_SIZE = 4;
    static constexpr uint32_t input_address(input_index key) { return INPUT_BASE + static_cast<uint32_t>(key) * INPUT_SLOT_SIZE; }
    std::array<uint32_t, INPUT_COUNT> input_state{};

public:
    Bios loader;
    GPU gpu;

    std::array<uint32_t, 31> registers{}; //22 regs + 9 regs GPU

    //piles de valeurs brutes 32bits
    std::vector<uint32_t> data_stack;
    std::vector<uint32_t> call_stack;

    bool running = true;
    bool flag = false;
    uint32_t ipf = 10;
    std::chrono::steady_clock::time_point sleep_until{};
    bool is_sleeping = false;

    bool load(const std::string& file) {
        if (!loader.load(file)) return false;

        registers.fill(0);
        data_stack.clear();
        call_stack.clear();

        running = true;
        flag = false;
        is_sleeping = false;

        return true;
    }

    uint8_t r8() {
        if (loader.pc >= loader.code.size()) {
            running = false;
            throw std::runtime_error("Error: unexpected end of code");
        }

        return loader.code[loader.pc++];
    }

    uint32_t r32() {
        uint32_t value = 0;
        for (int i = 0; i < 4; i++) value |= static_cast<uint32_t>(r8()) << (i * 8);

        return value;
    }

    float to_float(uint32_t value) {
        float result;
        std::memcpy(&result, &value, sizeof(float));
        return result;
    }

    uint32_t to_bits(float value) {
        uint32_t result;
        std::memcpy(&result, &value, sizeof(float));
        return result;
    }

    struct decoded_operand {
        uint8_t type;
        uint32_t value;
    };
    decoded_operand read_operand() {
        decoded_operand op{};

        op.type = r8();

        switch (op.type) {
            case 0x00:   //reg
            case 0x02: { //memoire
                op.value = r8();
                if (op.type == 0x00 && op.value >= registers.size()) throw std::runtime_error("Error: invalid register index");
                if (op.type == 0x02 && op.value >= registers.size()) throw std::runtime_error("Error: invalid memory address register");

                break;
            } case 0x04:
            case 0x05:
            case 0x06: {
                op.value = r8();
                if (op.type == 0x00 && op.value >= registers.size()) throw std::runtime_error("Error: invalid register index");
                if (op.type != 0x00 && op.value >= registers.size()) throw std::runtime_error("Error: invalid memory address register");

                break;
            } case 0x01: //immediat
            case 0x03: { //buffer
                op.value = r32();
                break;
            } default: throw std::runtime_error("Error: invalid operand type");
        }

        return op;
    }

    uint32_t read_memory(uint32_t address, uint8_t size) {
        constexpr uint32_t INPUT_END =
        INPUT_BASE + INPUT_COUNT * INPUT_SLOT_SIZE;
        uint32_t value = 0;

        for (uint8_t i = 0; i < size; i++) {
            uint64_t current = static_cast<uint64_t>(address) + i;

            if (current > UINT32_MAX) throw std::runtime_error("Error: memory address overflow");

            uint32_t addr = static_cast<uint32_t>(current);
            uint8_t byte;

            //zone inputs clavier
            if (addr >= INPUT_BASE && addr < INPUT_END) {
                uint32_t offset = addr - INPUT_BASE;
                uint32_t slot = offset / INPUT_SLOT_SIZE;
                uint32_t byte_offset = offset % INPUT_SLOT_SIZE;
                byte = (input_state[slot] >> (byte_offset * 8)) & 0xFF;
            } else {
                if (addr >= loader.memory.size()) throw std::runtime_error("Error: memory read out of bounds");
                byte = loader.memory[addr];
            }

            value |= static_cast<uint32_t>(byte) << (i * 8);
        }

        return value;
    }

    void write_memory(uint32_t address, uint32_t value, uint8_t size) {
        constexpr uint32_t INPUT_END =
        INPUT_BASE + INPUT_COUNT * INPUT_SLOT_SIZE;

        for (uint8_t i = 0; i < size; i++) {
            uint64_t current = static_cast<uint64_t>(address) + i;
            if (current > UINT32_MAX) throw std::runtime_error("Error: memory address overflow");
            uint32_t addr = static_cast<uint32_t>(current);

            //interdiction d'ecrire dans les inputs clavier
            if (addr >= INPUT_BASE && addr < INPUT_END) throw std::runtime_error("Error: cannot write to input memory");
            if (addr >= loader.memory.size()) throw std::runtime_error("Error: memory write out of bounds");
            loader.memory[addr] = (value >> (i * 8)) & 0xFF;
        }
    }

    uint32_t read_value(decoded_operand op) {
        switch (op.type) {
            case 0x00: return registers[op.value];
            case 0x01: return op.value;
            case 0x02: return read_memory(registers[op.value], 4);
            case 0x04: return read_memory(registers[op.value], 1);
            case 0x05: return read_memory(registers[op.value], 2);
            case 0x06: return read_memory(registers[op.value], 3);
            case 0x03: return read_memory(op.value, 4);
            default: throw std::runtime_error("Error: invalid operand type");
        }
    }

    void write_value(decoded_operand op, uint32_t value) {
        switch (op.type) {
            case 0x00: {
                registers[op.value] = value;
                break;
            } case 0x02: {
                write_memory(registers[op.value], value, 4);
                break;
            } case 0x04: {
                write_memory(registers[op.value], value, 1);
                break;
            } case 0x05: {
                write_memory(registers[op.value], value, 2);
                break;
            } case 0x06: {
                write_memory(registers[op.value], value, 3);
                break;
            } case 0x03: {
                uint32_t address = op.value;
                if (address > loader.memory.size() || loader.memory.size() - address < 4) throw std::runtime_error("Error: buffer write out of bounds");
                for (int i = 0; i < 4; i++) loader.memory[address + i] = (value >> (i * 8)) & 0xFF;

                break;
            } default: throw std::runtime_error("Error: cannot write to an immediate operand");
        }
    }

    int32_t to_int(uint32_t value) {
        int32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    uint32_t to_bits_int(int32_t value) {
        uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    inline decoded_operand read_register_destination() {
        decoded_operand dst = read_operand();
        if (dst.type != 0x00) throw std::runtime_error("Error: destination must be a register");

        return dst;
    }

    inline void integer_arithmetic(uint8_t opcode) {
        decoded_operand dst = read_register_destination();
        decoded_operand src = read_operand();
        uint32_t a = registers[dst.value];
        uint32_t b = read_value(src);
        uint32_t result = 0;

        switch (opcode) {
            case ADI: {
                result = a + b;
                break;
            } case SBI: {
                result = a - b;
                break;
            } case MLI: {
                result = a * b;
                break;
            } case DVI: {
                int32_t ia = to_int(a);
                int32_t ib = to_int(b);

                if (ib == 0) throw std::runtime_error("[DVI] Error: division by zero");
                if (ia == std::numeric_limits<int32_t>::min() && ib == -1) throw std::runtime_error("[DVI] Error: integer overflow");
                result = to_bits_int(ia / ib);

                break;
            } default: throw std::runtime_error("Error: invalid integer arithmetic opcode");
        }

        registers[dst.value] = result;
    }

    inline void float_arithmetic(uint8_t opcode) {
        decoded_operand dst = read_register_destination();
        decoded_operand src = read_operand();
        float a = to_float(registers[dst.value]);
        float b = to_float(read_value(src));
        float result = 0.0f;

        switch (opcode) {
            case ADF: {
                result = a + b;
                break;
            } case SBF: {
                result = a - b;
                break;
            } case MLF: {
                result = a * b;
                break;
            } case DVF: {
                result = a / b;
                break;
            } default: throw std::runtime_error("Error: invalid float arithmetic opcode");
        }

        registers[dst.value] = to_bits(result);
    }

    inline void comparison(uint8_t opcode) {
        decoded_operand a = read_register_destination();
        decoded_operand b = read_operand();
        uint32_t va = registers[a.value];
        uint32_t vb = read_value(b);

        switch (opcode) {
            case CMP: {
                flag = (va == vb);
                break;
            } case CPG: {
                flag = (to_int(va) > to_int(vb));
                break;
            } case CMPF: {
                flag = (to_float(va) == to_float(vb));
                break;
            } case CPGF: {
                flag = (to_float(va) > to_float(vb));
                break;
            } default: throw std::runtime_error("Invalid comparison opcode");
        }
    }

    void update_inputs() {
        input_state = {
            //lettres
            IsKeyDown(KEY_Q),          //A
            IsKeyDown(KEY_B),          //B
            IsKeyDown(KEY_C),          //C
            IsKeyDown(KEY_D),          //D
            IsKeyDown(KEY_E),          //E
            IsKeyDown(KEY_F),          //F
            IsKeyDown(KEY_G),          //G
            IsKeyDown(KEY_H),          //H
            IsKeyDown(KEY_I),          //I
            IsKeyDown(KEY_J),          //J
            IsKeyDown(KEY_K),          //K
            IsKeyDown(KEY_L),          //L
            IsKeyDown(KEY_SEMICOLON),  //M
            IsKeyDown(KEY_N),          //N
            IsKeyDown(KEY_O),          //O
            IsKeyDown(KEY_P),          //P
            IsKeyDown(KEY_A),          //Q
            IsKeyDown(KEY_R),          //R
            IsKeyDown(KEY_S),          //S
            IsKeyDown(KEY_T),          //T
            IsKeyDown(KEY_U),          //U
            IsKeyDown(KEY_V),          //V
            IsKeyDown(KEY_Z),          //W
            IsKeyDown(KEY_X),          //X
            IsKeyDown(KEY_Y),          //Y
            IsKeyDown(KEY_W),          //Z

            //chiffres
            IsKeyDown(KEY_ZERO),
            IsKeyDown(KEY_ONE),
            IsKeyDown(KEY_TWO),
            IsKeyDown(KEY_THREE),
            IsKeyDown(KEY_FOUR),
            IsKeyDown(KEY_FIVE),
            IsKeyDown(KEY_SIX),
            IsKeyDown(KEY_SEVEN),
            IsKeyDown(KEY_EIGHT),
            IsKeyDown(KEY_NINE),

            //ponct
            IsKeyDown(KEY_M),          //,
            IsKeyDown(KEY_COMMA),      //.

            //touches speciales
            IsKeyDown(KEY_SPACE),
            IsKeyDown(KEY_ENTER),
            IsKeyDown(KEY_BACKSPACE),

            //fleches
            IsKeyDown(KEY_UP),
            IsKeyDown(KEY_DOWN),
            IsKeyDown(KEY_LEFT),
            IsKeyDown(KEY_RIGHT)
        };
    }

    void exe_instruction() {
        if (!running) return;

        try {
            uint8_t opcode = r8();

            switch(opcode) {
                case SET: {
                    decoded_operand dst = read_operand();
                    decoded_operand src = read_operand();
                    write_value(dst, read_value(src));

                    break;
                } case EXT: {
                    running = false;
                    return;
                } case ADI:
                case SBI:
                case MLI:
                case DVI: {
                    integer_arithmetic(opcode);
                    break;
                } case ADF:
                case SBF:
                case MLF:
                case DVF: {
                    float_arithmetic(opcode);
                    break;
                } case CMP:
                case CPG:
                case CMPF:
                case CPGF: {
                    comparison(opcode);
                    break;
                } case JMP: {
                    uint32_t addr = r32();
                    if (addr >= loader.code.size()) throw std::runtime_error("[JMP] Error: invalid jump address");
                    loader.pc = addr;
                    break;
                } case JPT: {
                    uint32_t addr = r32();
                    if (flag) {
                        if (addr >= loader.code.size()) throw std::runtime_error("[JPT] Error: invalid jump address");
                        loader.pc = addr;
                    }

                    break;
                } case JPF: {
                    uint32_t addr = r32();
                    if (!flag) {
                        if (addr >= loader.code.size()) throw std::runtime_error("[JPF] Error: invalid jump address");
                        loader.pc = addr;
                    }

                    break;
                } case CALL: {
                    uint32_t addr = r32();
                    if (addr >= loader.code.size()) throw std::runtime_error("[CALL] Error: invalid call address");
                    call_stack.push_back(loader.pc);
                    loader.pc = addr;

                    break;
                } case RET: {
                    if (call_stack.empty()) {
                        running = false;
                        break;
                    }
                    loader.pc = call_stack.back();
                    call_stack.pop_back();

                    break;
                } case PUSH: {
                    decoded_operand src = read_operand();
                    if (src.type == 0x01) throw std::runtime_error("Error: PUSH cannot use immediate values");
                    data_stack.push_back(read_value(src));

                    break;
                } case LOAD: {
                    uint8_t dst = r8();
                    if (dst >= registers.size()) throw std::runtime_error("[LOAD] Error: invalid register");
                    if (data_stack.empty()) throw std::runtime_error("[LOAD] Error: stack underflow");
                    registers[dst] = data_stack.back();
                    data_stack.pop_back();

                    break;
                } case GPU_S: {
                    uint8_t operation = r8();
                    int x = to_int(registers[22]);
                    int y = to_int(registers[23]);
                    int w = to_int(registers[24]);
                    int h = to_int(registers[25]);
                    int t = to_int(registers[26]);
                    uint32_t color_index = registers[27];
                    int radius = to_int(registers[28]);
                    int scale = to_int(registers[29]);
                    uint32_t map_offset = registers[30];

                    //verif couleur
                    if (color_index >= 26) throw std::runtime_error("GPU: invalid color index");
                    Color color = gpu.palette[color_index];

                    switch (operation) {
                        case GPU_PIXEL: {
                            gpu.pixel(x, y, color);
                            break;
                        } case GPU_RECT: {
                            gpu.rect(x, y, w, h, color);
                            break;
                        } case GPU_VOIDRECT: {
                            gpu.voidrect(x, y, w, h, t, color);
                            break;
                        } case GPU_CIRCLE: {
                            gpu.circle(x, y, radius, color);
                            break;
                        } case GPU_VOIDCIRCLE: {
                            gpu.voidcircle(x, y, radius, t, color);
                            break;
                        } case GPU_CLEAR: {
                            gpu.clear(color);
                            break;
                        } case GPU_DRAWMAP: {
                            if (w <= 0 || h <= 0) throw std::runtime_error("GPU: invalid map dimensions");
                            if (scale <= 1) scale = 1;

                            size_t map_size = static_cast<size_t>(w) * h;
                            if (map_offset > loader.memory.size() || map_size > loader.memory.size() - map_offset) throw std::runtime_error("GPU: map out of memory bounds");
                            uint8_t* map = loader.memory.data() + map_offset;

                            //verif indices de palette avant rendu
                            for (size_t i = 0; i < map_size; i++) {
                                if (map[i] >= 26) throw std::runtime_error("GPU: invalid color in map");
                            }
                            gpu.drawmap(x, y, w, h, scale, map);

                            break;
                        } default: throw std::runtime_error("GPU: unknown operation");
                    }

                    break;
                } case IPF: {
                    uint32_t value = r32();
                    ipf = (value == 0) ? 1 : value;

                    break;
                } case SLP: {
                    decoded_operand src = read_operand();
                    int32_t duration = to_int(read_value(src));

                    if (duration < 0) throw std::runtime_error("[SLP] Error: negative duration");
                    if (duration > 0) {
                        sleep_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration);
                        is_sleeping = true;
                    }

                    break;
                } case TME: {
                    uint8_t dst = r8();
                    if (dst >= registers.size()) throw std::runtime_error("[TME} Error: invalid register");

                    std::time_t timestamp = std::time(nullptr);
                    if (timestamp == static_cast<std::time_t>(-1)) throw std::runtime_error("[TME] Error: unable to get system time");
                    registers[dst] = static_cast<uint32_t>(timestamp);

                    break;
                } default: {
                    std::cerr << "\033[31m" << "Error: unknown opcode: " << std::to_string(opcode) << std::endl;
                    running = false;
                    return;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "\033[31m[CPU] Error: " << e.what() << "\033[0m" << std::endl;
            running = false;
        }
    }
};

//@KOT76b; magnus carlasen 2024-06 for ГПСД и XS проект
