#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <cstring>

//types d'oprds
constexpr uint8_t OPERAND_REGISTER =  0x00;
constexpr uint8_t OPERAND_IMMEDIATE = 0x01;
constexpr uint8_t OPERAND_MEMORY =    0x02;
constexpr uint8_t OPERAND_BUFFER =    0x03;

constexpr uint8_t OPERAND_MEMORY_8 =  0x04;
constexpr uint8_t OPERAND_MEMORY_16 = 0x05;
constexpr uint8_t OPERAND_MEMORY_24 = 0x06;

std::unordered_set<std::string> included_headers;
std::vector<std::string> source_lines;
int lines = 0;

//OPCODES=====
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
    GPU,
    IPF,
    SLP,
    TME
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

enum class section {
    none,
    data,
    buffer,
    code
};
section current_section = section::none;

struct fixup {
    std::string label;
    size_t position;
};

enum class constant_type {
    integer,
    floating,
    string
};
struct constant {
    std::string name;
    constant_type type;
    std::vector<uint8_t> data;
};

struct buffer {
    std::string name;
    uint32_t size_bits;
    uint32_t offset;
};

int register_index(const std::string& name) {
    //registres generaux : ra a rv
    if (name.size() == 2 && name[0] == 'r' && name[1] >= 'a' && name[1] <= 'v') return name[1] - 'a';
    //registres GPU rg1 a rg9
    if (name.size() == 3 && name[0] == 'r' && name[1] == 'g' && name[2] >= '1' && name[2] <= '9') return 22 + (name[2] - '1');

    return -1;
}

//=======UTILS=======
std::string trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
    return s;
}

bool isInt(const std::string& s) {
    if (s.empty()) return false;
    char* end;
    std::strtol(s.c_str(), &end, 10);

    return end != s.c_str() && *end == '\0';
}

bool isFloat(const std::string& s) {
    if (s.empty()) return false;
    char* end;
    std::strtof(s.c_str(), &end);

    return end != s.c_str() && *end == '\0' && s.find('.') != std::string::npos;
}

void append_U32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(value & 0xFF);
    data.push_back((value >> 8) & 0xFF);
    data.push_back((value >> 16) & 0xFF);
    data.push_back((value >> 24) & 0xFF);
}

bool encode_operand(std::vector<uint8_t>& code, std::string operand, const std::vector<constant>& constants, const std::vector<buffer>& buffers, uint8_t memory_type = OPERAND_MEMORY) {
    operand = trim(operand);

    //registre
    int reg = register_index(operand);
    if (reg != -1) {
        code.push_back(OPERAND_REGISTER);
        code.push_back(static_cast<uint8_t>(reg));
        return true;
    }

    //memoire indirecte (entre crochets)
    if (operand.size() >= 3 && operand.front() == '[' && operand.back() == ']') {
        std::string reg_name = trim(operand.substr(1, operand.size() - 2));
        int memory_reg = register_index(reg_name);

        if (memory_reg == -1) return false;

        code.push_back(memory_type);
        code.push_back(static_cast<uint8_t>(memory_reg));
        return true;
    }

    //valeurs immediates
    if (!operand.empty() && (operand.front() == '#' || operand.front() == '$')) {
        char prefix = operand.front();
        operand.erase(0, 1);

        if (prefix == '#') {
            //dec
            if (isInt(operand)) {
                long value = std::strtol(operand.c_str(), nullptr, 10);
                if (value < INT32_MIN || value > INT32_MAX) return false;

                code.push_back(OPERAND_IMMEDIATE);
                append_U32(code, static_cast<uint32_t>(static_cast<int32_t>(value)));
                return true;
            }

            if (isFloat(operand)) {
                float value = std::strtof(operand.c_str(), nullptr);
                uint32_t bits;

                std::memcpy(&bits, &value, sizeof(bits));
                code.push_back(OPERAND_IMMEDIATE);
                append_U32(code, bits);
                return true;
            }
        } else if (prefix == '$') {
            //hex
            char* end;
            unsigned long value = std::strtoul(operand.c_str(), &end, 16);

            if (*end == '\0' && end != operand.c_str() && value <= UINT32_MAX) {
                code.push_back(OPERAND_IMMEDIATE);
                append_U32(code, static_cast<uint32_t>(value));
                return true;
            }
        }

        return false;
    }

    //constante
    for (const auto& c : constants) {
        if (c.name == operand) {
            if (c.type == constant_type::string) return false;
            code.push_back(OPERAND_IMMEDIATE);
            for (uint8_t byte : c.data) code.push_back(byte);

            return true;
        }
    }

    //buffer
    for (const auto& b : buffers) {
        if (b.name == operand) {
            code.push_back(OPERAND_BUFFER);
            append_U32(code, b.offset);
            return true;
        }
    }

    return false;
}

//gestion des caracteres d'echappement dans les chaines litterales
std::string parse_escape_sequences(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n':  result += '\n'; break; //nouvelle ligne
                case 't':  result += '\t'; break; //tabulation
                case 'r':  result += '\r'; break; //retour chariot
                case '\\': result += '\\'; break; //antislash
                case '"':  result += '"';  break; //guillemet literal
                default:   result += s[i]; //caractere inconnu: on garde le '\'
            }
            ++i; //saute le caractere suivant
        } else result += s[i];
    }
    return result;
}

std::filesystem::path find_headers_dir(const std::filesystem::path& input_path, const std::filesystem::path& executable_path) {
    std::vector<std::filesystem::path> candidates;

    auto add_candidate = [&](const std::filesystem::path& p) {
        if (!p.empty()) candidates.push_back(p);
    };

    add_candidate(std::filesystem::current_path() / "scr" / "headers");

    if (!input_path.empty()) {
        std::filesystem::path current = input_path.has_parent_path() ? input_path.parent_path() : std::filesystem::current_path();
        while (true) {
            add_candidate(current / "scr" / "headers");
            add_candidate(current / "headers");
            if (current == current.parent_path()) break;
            current = current.parent_path();
        }
    }

    if (!executable_path.empty()) {
        std::filesystem::path exe_dir = executable_path.has_parent_path() ? executable_path.parent_path() : std::filesystem::current_path();
        add_candidate(exe_dir / ".." / "headers");
        add_candidate(exe_dir / ".." / "scr" / "headers");
    }

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            return candidate.lexically_normal();
        }
    }

    return {};
}

void include_header(const std::string& name, const std::filesystem::path& headers_dir) {
    if (included_headers.count(name)) return;
    included_headers.insert(name);

    std::filesystem::path header_path = std::filesystem::path(name);
    if (!header_path.is_absolute()) header_path = headers_dir / header_path;

    std::filesystem::path deps_path = header_path;
    deps_path = deps_path.parent_path() / (deps_path.filename().string() + ".deps.txt");
    std::ifstream deps(deps_path);
    if (deps) {
        std::string dep;
        while (std::getline(deps, dep)) {
            dep = trim(dep);
            if (!dep.empty()) include_header(dep, headers_dir);
        }
    }

    std::ifstream file(header_path);
    if (!file) {
        std::cerr << "\033[31m" << "Error: header not found: " << header_path.string() << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;

        //supprime les commentaires de fin de ligne
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) line = trim(line.substr(0, comment_pos));
        if (line.empty()) continue;

        source_lines.push_back(line);
    }
}

//=======MAIN=======

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "\033[31m" << "Usage: CASM <input.casm> <output.bin>" << std::endl;
        return 1;
    }

    std::filesystem::path input_path = argv[1];
    std::filesystem::path executable_path = argc > 0 ? argv[0] : "";
    std::filesystem::path headers_dir = find_headers_dir(input_path, executable_path);
    if (headers_dir.empty()) {
        std::cerr << "\033[31m" << "Error: header's repository unfindable" << std::endl;
        return 1;
    }

    std::ifstream in(input_path);
    if (!in) {
        std::cerr << "\033[31m" << "Error in source opening" << std::endl;
        return 1;
    }

    std::ofstream out(argv[2], std::ios::binary);
    if (!out) {
        std::cerr << "\033[31m" <<  "Error in binary opening" << std::endl;
        return 1;
    }

    //variables detection opcodes
    uint8_t arithmetic_opcode = 0;
    uint8_t comparison_opcode = 0;
    uint8_t jump_opcode = 0;
    uint8_t gpu_opcode = 0;

    //variables programme
    std::vector<uint8_t> code;
    std::unordered_map<std::string, uint32_t> labels;
    std::vector<fixup> fixups;
    std::vector<constant> constants;
    std::vector<buffer> buffers;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) lines++;
        if (line.empty() || line[0] == ';') continue;

        //supprime les commentaires
        size_t inline_comment = line.find(';');
        if (inline_comment != std::string::npos) line = trim(line.substr(0, inline_comment));

        std::istringstream iss(line);
        std::string cmd, file;
        iss >> cmd;
        if (cmd == "header") {
            iss >> file;
            file.erase(std::remove(file.begin(), file.end(), '"'), file.end());
            include_header(file, headers_dir);
            continue;
        }
        source_lines.push_back(line);
    }

    for (size_t i = 0; i < source_lines.size(); ++i) {
        bool protected_instruction = false;
        lines++;

        std::string line = source_lines[i];
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "section.data") {
            current_section = section::data;
            continue;
        } else if (cmd == "section.buffer") {
            current_section = section::buffer;
            continue;
        } else if (cmd == "section.code") {
            current_section = section::code;
            continue;
        }

        //=====SECTION DATA=====
        if (current_section == section::data) {
            if (cmd != "crt") {
                std::cerr << "[" << lines << "] Error: expected crt" << std::endl;
                return 1;
            }

            std::string declaration;
            std::getline(iss, declaration);
            declaration = trim(declaration);

            size_t comma = declaration.find(',');
            if (comma == std::string::npos) {
                std::cerr << "[" << lines << "] Error: invalid crt syntax" << std::endl;
                return 1;
            }

            std::string name = trim(declaration.substr(0, comma));
            std::string value = trim(declaration.substr(comma + 1));

            if (name.empty() || value.empty()) {
                std::cerr << "[" << lines << "] Error: invalid crt declaration" << std::endl;
                return 1;
            }

            for (const auto& c : constants) {
                if (c.name == name) {
                    std::cerr << "[" << lines << "] Error: duplicate constant" << std::endl;
                    return 1;
                }
            }

            constant c;
            c.name = name;

            if (value.front() == '"' && value.back() == '"') {
                c.type = constant_type::string;

                std::string str = parse_escape_sequences(value.substr(1, value.size() - 2));
                for (unsigned char ch : str) c.data.push_back(ch);
                c.data.push_back(0);
            } else if (isInt(value)) {
                c.type = constant_type::integer;
                long parsed = std::strtol(value.c_str(), nullptr, 10);

                if (parsed < INT32_MIN || parsed > INT32_MAX) {
                    std::cerr << "[" << lines << "] Error: integer out of range" << std::endl;
                    return 1;
                }

                append_U32(c.data, static_cast<uint32_t>(static_cast<int32_t>(parsed)));
            } else if (isFloat(value)) {
                c.type = constant_type::floating;
                float parsed = std::strtof(value.c_str(), nullptr);
                uint32_t bits;

                std::memcpy(&bits, &parsed, sizeof(bits));
                append_U32(c.data, bits);
            } else {
                std::cerr << "[" << lines << "] Error: invalid constant value" << std::endl;
                return 1;
            }

            constants.push_back(c);
            continue;
        }

        //=====SECTION BUFFER=====
        if (current_section == section::buffer) {
            if (cmd != "bset") {
                std::cerr << "[" << lines << "] Error: expected bset" << std::endl;
                return 1;
            }

            std::string declaration;
            std::getline(iss, declaration);
            declaration = trim(declaration);

            size_t comma = declaration.find(',');
            if (comma == std::string::npos) {
                std::cerr << "[" << lines << "] Error: invalid bset syntax" << std::endl;
                return 1;
            }

            std::string name = trim(declaration.substr(0, comma));
            std::string size = trim(declaration.substr(comma + 1));

            if (name.empty() || !isInt(size)) {
                std::cerr << "[" << lines << "] Error: invalid bset declaration" << std::endl;
                return 1;
            }

            long parsed = std::strtol(size.c_str(), nullptr, 10);

            if (parsed <= 0 || parsed > UINT32_MAX) {
                std::cerr << "[" << lines << "] Error: invalid buffer size" << std::endl;
                return 1;
            }

            for (const auto& b : buffers) {
                if (b.name == name) {
                    std::cerr << "[" << lines << "] Error: duplicate buffer" << std::endl;
                    return 1;
                }
            }

            //calcul de l'offset en octets
            uint32_t offset = 0;

            for (const auto& b : buffers) {
                uint32_t end = b.offset + (b.size_bits + 7) / 8;
                if (end > offset) offset = end;
            }

            buffers.push_back({ name, static_cast<uint32_t>(parsed), offset });

            continue;
        }

        //=====SECTION CODE=====
        if (current_section != section::code) continue;

        //1. labels
        if (!cmd.empty() && cmd.back() == ':') {
            std::string label = cmd.substr(0, cmd.size() - 1);
            if (labels.contains(label)) {
                std::cerr << "\033[31m" << "[" << lines << "] Error: the label <<" << label << ">> already exist" << std::endl;
                exit(1);
            }
            labels[label] = (uint32_t)code.size();
            continue;
        }

        if (cmd == "ext") {
            code.push_back(EXT);
            continue;
        }

        if (cmd == "ret") {
            code.push_back(RET);
            continue;
        }

        if (cmd == "set") {
            std::string operands;
            std::getline(iss, operands);
            operands = trim(operands);
            uint8_t memory_type = OPERAND_MEMORY;

            //detection du specifieur vgl
            std::istringstream spec_stream(operands);
            std::string specifier;
            spec_stream >> specifier;

            if (specifier == "vgl" || specifier == "vgl2" || specifier == "vgl3") {
                if (specifier == "vgl")  memory_type = OPERAND_MEMORY_8;
                if (specifier == "vgl2") memory_type = OPERAND_MEMORY_16;
                if (specifier == "vgl3") memory_type = OPERAND_MEMORY_24;
                std::getline(spec_stream, operands);
                operands = trim(operands);
            }

            //recherche de la virgule separant les deux operandes
            size_t comma = operands.find(',');

            if (comma == std::string::npos || operands.find(',', comma + 1) != std::string::npos) {
                std::cerr << "[" << lines << "] Error: invalid SET syntax" << std::endl;
                return 1;
            }

            std::string destination = trim(operands.substr(0, comma));
            std::string source = trim(operands.substr(comma + 1));

            auto is_indirect_memory = [](const std::string& operand) {
                return operand.size() >= 3 &&
                operand.front() == '[' &&
                operand.back() == ']';
            };

            if (memory_type != OPERAND_MEMORY) {
                bool dest_memory = is_indirect_memory(destination);
                bool src_memory  = is_indirect_memory(source);

                if (dest_memory == src_memory) {
                    std::cerr << "[" << lines << "] Error: Vogel requires exactly one memory operand" << std::endl;
                    return 1;
                }
            }

            if (destination.empty() || source.empty()) {
                std::cerr << "[" << lines << "] Error: missing SET operand" << std::endl;
                return 1;
            }

            //la destination doit etre registre, adresse memoire ou buffer
            bool valid_destination = false;
            if (register_index(destination) != -1) valid_destination = true;
            else if (destination.size() >= 3 && destination.front() == '[' && destination.back() == ']') {
                std::string address_reg = trim(destination.substr(1, destination.size() - 2));
                if (register_index(address_reg) != -1) valid_destination = true;
            } else {
                for (const auto& b : buffers) {
                    if (b.name == destination) {
                        valid_destination = true;
                        break;
                    }
                }
            }

            if (!valid_destination) {
                std::cerr << "[" << lines << "] Error: invalid SET destination" << std::endl;
                return 1;
            }

            //encodage temporaire des operandes
            std::vector<uint8_t> encoded;
            uint8_t dest_type = is_indirect_memory(destination) ? memory_type : OPERAND_MEMORY;
            uint8_t src_type = is_indirect_memory(source) ? memory_type : OPERAND_MEMORY;

            if (!encode_operand(encoded, destination, constants, buffers, dest_type)) {
                std::cerr << "[" << lines << "] Error: invalid SET destination operand" << std::endl;
                return 1;
            }

            if (!encode_operand(encoded, source, constants, buffers, src_type)) {
                std::cerr << "[" << lines << "] Error: invalid SET source operand" << std::endl;
                return 1;
            }

            //ecriture de l'instruction
            code.push_back(SET);
            code.insert(code.end(), encoded.begin(), encoded.end());
            continue;
        }

        //arithmetique
        arithmetic_opcode = 0;
        if      (cmd == "adi") arithmetic_opcode = ADI;
        else if (cmd == "sbi") arithmetic_opcode = SBI;
        else if (cmd == "mli") arithmetic_opcode = MLI;
        else if (cmd == "dvi") arithmetic_opcode = DVI;
        else if (cmd == "adf") arithmetic_opcode = ADF;
        else if (cmd == "sbf") arithmetic_opcode = SBF;
        else if (cmd == "mlf") arithmetic_opcode = MLF;
        else if (cmd == "dvf") arithmetic_opcode = DVF;
        if (arithmetic_opcode != 0) {
            std::string operands;
            std::getline(iss, operands);
            operands = trim(operands);

            size_t comma = operands.find(',');

            if (comma == std::string::npos ||
                operands.find(',', comma + 1) != std::string::npos) {
                std::cerr << "[" << lines << "] Error: invalid arithmetic syntax" << std::endl;
                return 1;
            }

                std::string destination = trim(operands.substr(0, comma));
                std::string source = trim(operands.substr(comma + 1));

                if (register_index(destination) == -1 || source.empty()) {
                    std::cerr << "[" << lines << "] Error: invalid arithmetic operands" << std::endl;
                    return 1;
                }

                code.push_back(arithmetic_opcode);

                if (!encode_operand(code, destination, constants, buffers) || !encode_operand(code, source, constants, buffers)) {
                    std::cerr << "[" << lines << "] Error: invalid arithmetic operand" << std::endl;
                    return 1;
                }

            continue;
        }

        //comparaison
        comparison_opcode = 0;
        if      (cmd == "cmp")  comparison_opcode = CMP;
        else if (cmd == "cmpf") comparison_opcode = CMPF;
        else if (cmd == "cpg")  comparison_opcode = CPG;
        else if (cmd == "cpgf") comparison_opcode = CPGF;
        if (comparison_opcode != 0) {
            std::string operands;
            std::getline(iss, operands);
            operands = trim(operands);
            size_t comma = operands.find(',');

            if (comma == std::string::npos || operands.find(',', comma + 1) != std::string::npos) {
                std::cerr << "[" << lines << "] Error: invalid comparison syntax" << std::endl;
                return 1;
            }

            std::string arg1 = trim(operands.substr(0, comma));
            std::string arg2 = trim(operands.substr(comma + 1));

            if (register_index(arg1) == -1 || arg2.empty()) {
                std::cerr << "[" << lines << "] Error: invalid comparison operands" << std::endl;
                return 1;
            }

            code.push_back(comparison_opcode);

            if (!encode_operand(code, arg1, constants, buffers) || !encode_operand(code, arg2, constants, buffers)) {
                std::cerr << "[" << lines << "] Error: invalid comparison operand" << std::endl;
                return 1;
            }

            continue;
        }

        //sauts
        jump_opcode = 0;
        if      (cmd == "jmp")  jump_opcode = JMP;
        else if (cmd == "jpt")  jump_opcode = JPT;
        else if (cmd == "jpf")  jump_opcode = JPF;
        else if (cmd == "call") jump_opcode = CALL;
        if (jump_opcode != 0) {
            std::string label;
            iss >> label;

            if (label.empty()) {
                std::cerr << "[" << lines << "] Error: missing label" << std::endl;
                return 1;
            }

            std::string extra;
            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: too many operands" << std::endl;
                return 1;
            }

            code.push_back(jump_opcode);

            //reserve 4 octets pour addr du label
            fixups.push_back({ label, code.size() });
            append_U32(code, 0);

            continue;
        }

        if (cmd == "push") {
            std::string operand;
            iss >> operand;

            if (operand.empty()) {
                std::cerr << "[" << lines << "] Error: missing PUSH operand" << std::endl;
                return 1;
            }

            std::string extra;
            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: too many PUSH operands" << std::endl;
                return 1;
            }

            bool valid = register_index(operand) != -1 || (operand.size() >= 3 && operand.front() == '[' && operand.back() == ']');
            for (const auto& b : buffers) if (b.name == operand) valid = true;

            if (!valid) {
                std::cerr << "[" << lines << "] Error: invalid PUSH operand" << std::endl;
                return 1;
            }

            code.push_back(PUSH);
            if (!encode_operand(code, operand, constants, buffers)) {
                std::cerr << "[" << lines << "] Error: invalid PUSH operand" << std::endl;
                return 1;
            }

            continue;
        }

        if (cmd == "load") {
            std::string destination;
            iss >> destination;

            if (destination.empty()) {
                std::cerr << "[" << lines << "] Error: missing LOAD destination" << std::endl;
                return 1;
            }

            std::string extra;
            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: too many LOAD operands" << std::endl;
                return 1;
            }

            if (register_index(destination) == -1) {
                std::cerr << "[" << lines << "] Error: LOAD destination must be a register" << std::endl;
                return 1;
            }

            code.push_back(LOAD);
            if (!encode_operand(code, destination, constants, buffers)) {
                std::cerr << "[" << lines << "] Error: invalid LOAD destination" << std::endl;
                return 1;
            }

            continue;
        }

        //gpu
        gpu_opcode = 0;
        if      (cmd == "pixel")     gpu_opcode = GPU_PIXEL;
        else if (cmd == "rect")      gpu_opcode = GPU_RECT;
        else if (cmd == "voidrect")  gpu_opcode = GPU_VOIDRECT;
        else if (cmd == "circle")    gpu_opcode = GPU_CIRCLE;
        else if (cmd == "voidcircle") gpu_opcode = GPU_VOIDCIRCLE;
        else if (cmd == "clear")     gpu_opcode = GPU_CLEAR;
        else if (cmd == "drawmap")   gpu_opcode = GPU_DRAWMAP;
        if (gpu_opcode != 0) {
            std::string extra;

            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: GPU instructions take no operands" << std::endl;
                return 1;
            }

            code.push_back(GPU);
            code.push_back(gpu_opcode);

            continue;
        }

        if (cmd == "slp") {
            std::string operand;
            iss >> operand;

            if (operand.empty()) {
                std::cerr << "[" << lines << "] Error: missing SLP operand" << std::endl;
                return 1;
            }

            std::string extra;
            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: too many SLP operands" << std::endl;
                return 1;
            }

            std::vector<uint8_t> encoded;

            if (!encode_operand(encoded, operand, constants, buffers)) {
                std::cerr << "[" << lines << "] Error: invalid SLP operand" << std::endl;
                return 1;
            }

            code.push_back(SLP);
            code.insert(code.end(), encoded.begin(), encoded.end());

            continue;
        }

        if (cmd == "ipf") {
            std::string operand;
            iss >> operand;

            if (operand.empty()) {
                std::cerr << "[" << lines << "] Error: missing IPF operand" << std::endl;
                return 1;
            }

            std::string extra;
            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: too many IPF operands" << std::endl;
                return 1;
            }

            bool valid = false;
            if (operand.front() == '#') {
                std::string value = operand.substr(1);

                if (isInt(value)) {
                    long parsed = std::strtol(value.c_str(), nullptr, 10);
                    valid = parsed > 0 && parsed <= INT32_MAX;
                }
            } else if (operand.front() == '$') {
                std::string value = operand.substr(1);
                char* end;
                unsigned long parsed = std::strtoul(value.c_str(), &end, 16);

                valid = end != value.c_str() && *end == '\0' && parsed > 0 && parsed <= UINT32_MAX;
            } else {
                for (const auto& c : constants) {
                    if (c.name == operand && c.type == constant_type::integer) {
                        uint32_t value = 0;
                        for (int i = 0; i < 4; i++) value |= static_cast<uint32_t>(c.data[i]) << (i * 8);

                        valid = static_cast<int32_t>(value) > 0;
                        break;
                    }
                }
            }

            if (!valid) {
                std::cerr << "[" << lines << "] Error: IPF requires a positive integer" << std::endl;
                return 1;
            }

            std::vector<uint8_t> encoded;

            if (!encode_operand(encoded, operand, constants, buffers)) {
                std::cerr << "[" << lines << "] Error: invalid IPF operand" << std::endl;
                return 1;
            }

            code.push_back(IPF);
            code.insert(code.end(), encoded.begin(), encoded.end());

            continue;
        }

        if (cmd == "tme") {
            std::string destination;
            iss >> destination;

            if (destination.empty()) {
                std::cerr << "[" << lines << "] Error: missing TME destination" << std::endl;
                return 1;
            }

            std::string extra;
            if (iss >> extra) {
                std::cerr << "[" << lines << "] Error: too many TME operands" << std::endl;
                return 1;
            }

            bool valid = register_index(destination) != -1;
            if (!valid && destination.size() >= 3 && destination.front() == '[' && destination.back() == ']') {
                std::string reg_name = trim(destination.substr(1, destination.size() - 2));
                valid = register_index(reg_name) != -1;
            }

            //buffer
            if (!valid) {
                for (const auto& b : buffers) {
                    if (b.name == destination) {
                        valid = true;
                        break;
                    }
                }
            }

            if (!valid) {
                std::cerr << "[" << lines << "] Error: invalid TME destination" << std::endl;
                return 1;
            }

            std::vector<uint8_t> encoded;
            if (!encode_operand(encoded, destination, constants, buffers)) {
                std::cerr << "[" << lines << "] Error: invalid TME operand" << std::endl;
                return 1;
            }

            code.push_back(TME);
            code.insert(code.end(), encoded.begin(), encoded.end());

            continue;
        }

        std::cout << "[" << lines << "] Error: unknown op code: " << cmd << std::endl;
        exit(1);
    }

    //=====ECRITURE DU BINAIRE=====
    //resolution des labels
    for (auto& f : fixups) {
        if (labels.find(f.label) == labels.end()) {
            std::cerr << "\033[31m" << "[" << lines << "] Error: label '" << f.label << "' not found" << std::endl;
            return 1;
        }

        uint32_t addr = labels[f.label];
        for (int i = 0; i < 4; i++) code[f.position + i] = (addr >> (i * 8)) & 0xFF;
    }

    //calcul de la taille totale des buffers
    uint64_t total_buffer_size = 0;
    for (const auto& b : buffers) {
        uint64_t end = static_cast<uint64_t>(b.offset) + (static_cast<uint64_t>(b.size_bits) + 7) / 8;

        if (end > total_buffer_size) total_buffer_size = end;
    }

    if (total_buffer_size > UINT32_MAX || code.size() > UINT32_MAX) {
        std::cerr << "\033[31mError: binary too large" << std::endl;
        return 1;
    }

    std::vector<uint8_t> header; //creation de l'en-tete

    //magic word
    header.push_back('C');
    header.push_back('A');
    header.push_back('S');
    header.push_back('M');
    header.push_back(8); //version

    uint32_t entry = labels.count("main") ? labels["main"] : 0;

    append_U32(header, entry);
    append_U32(header, static_cast<uint32_t>(total_buffer_size));
    append_U32(header, static_cast<uint32_t>(code.size()));

    //ecriture du fichier
    out.write(reinterpret_cast<const char*>(header.data()), header.size());
    out.write(reinterpret_cast<const char*>(code.data()), code.size());

    if (!out) {
        std::cerr << "\033[31mError: binary writing failed" << std::endl;
        return 1;
    }

    std::cout << "\033[0m" << "Compilation finished: " << argv[2] << std::endl;
    return 0;
}

//@KOT76b; magnus carlasen 2024-06 for ГПСД и XS проект
//v8
