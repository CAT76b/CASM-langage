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
std::unordered_set<std::string> included_headers;
std::vector<std::string> source_lines;
int lines = 0;
constexpr uint8_t ERROR_OFFSET = 160;

//============OPCODES============

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
    GTC,
    ITS,
    DISK,
    WRT,
    RAD,
    ERROR
};

struct Fixup {
    std::string label;
    size_t position;
};

struct Map {
    uint16_t width;
    uint16_t height;
    std::vector<uint8_t> pixels;
};

//=======UTILS=======

std::string trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
    return s;
}

std::string stripComma(std::string s) {
    if (!s.empty() && s.back() == ',') s.pop_back();
    return s;
}

bool isInt(const std::string& s) {
    if (s.empty()) return false;
    char* end;
    std::strtol(s.c_str(), &end, 10);
    return *end == '\0';
}

bool isFloat(const std::string& s) {
    if (s.empty()) return false;
    char* end;
    std::strtof(s.c_str(), &end);
    return *end == '\0' && s.find('.') != std::string::npos;
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
        std::cerr << "Error: Header not found: " << header_path.string() << std::endl;
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

        //gestion multi-lignes pour assembler les accolades { } des maps dans les headers
        if (line.find('{') != std::string::npos && line.find('}') == std::string::npos) {
            std::string next_line;
            while (line.find('}') == std::string::npos && std::getline(file, next_line)) {
                size_t next_comment = next_line.find(';');
                if (next_comment != std::string::npos) next_line = next_line.substr(0, next_comment);
                line += " " + trim(next_line);
            }
        }

        source_lines.push_back(line);
    }
}

//=======MAIN=======

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: CASM <input.casm> <output.bin>" << std::endl;
        return 1;
    }

    std::filesystem::path input_path = argv[1];
    std::filesystem::path executable_path = argc > 0 ? argv[0] : "";
    std::filesystem::path headers_dir = find_headers_dir(input_path, executable_path);
    if (headers_dir.empty()) {
        std::cerr << "Error: header's repository unfindable" << std::endl;
        return 1;
    }

    std::ifstream in(input_path);
    if (!in) { std::cerr << "Error in source opening" << std::endl; return 1; }

    std::ofstream out(argv[2], std::ios::binary);
    if (!out) { std::cerr << "Error in binary opening" << std::endl; return 1; }

    std::vector<uint8_t> code;
    std::unordered_map<std::string, uint16_t> labels;
    std::vector<Fixup> fixups;

    //variables
    std::vector<std::string> varOrder;
    std::unordered_map<std::string, uint8_t> varType; //1=int 2=str 3=float 4=bool
    std::unordered_map<std::string, int> intVars;
    std::unordered_map<std::string, float> floatVars;
    std::unordered_map<std::string, std::string> strVars;
    std::unordered_map<std::string, Map> mapVars;
    std::vector<std::string> stringOrder;
    std::vector<std::string> mapOrder;

    bool inData = true, inCode = false;

    auto ensureVar = [&](const std::string& v) -> uint8_t {
        auto it = std::find(varOrder.begin(), varOrder.end(), v);
        if (it != varOrder.end()) {
            return (uint8_t)std::distance(varOrder.begin(), it);
        }

        varOrder.push_back(v);
        if (v.rfind("f_", 0) == 0) {
            varType[v] = 3;
            floatVars[v] = 0.0f;
        } else if (v.rfind("i_", 0) == 0) {
            varType[v] = 1;
            intVars[v] = 0;
        } else if (v.rfind("s_", 0) == 0) {
            varType[v] = 2;
            strVars[v] = "";
            stringOrder.push_back(v);
        } else if (v.rfind("b_", 0) == 0) {
            varType[v] = 4;
            intVars[v] = 0;
        } else if (v.rfind("m_", 0) == 0) {
            varType[v] = 5;
            mapOrder.push_back(v);
            mapVars[v] = Map{};
        } else {
            varType[v] = 1;
            intVars[v] = 0;
        }

        return (uint8_t)(varOrder.size() - 1);
    };

    auto varIndex = [&](const std::string& v) -> uint8_t {
        return ensureVar(v);
    };

    auto encodeOperand = [&](std::string t, bool& hasVar, bool forceVar = false) {
        // constante explicite
        if (!t.empty() && t[0] == '#') t.erase(0, 1);
        if (!isInt(t) && !isFloat(t) || forceVar) {
            hasVar = true;
            code.push_back(varIndex(t));
        } else if (isFloat(t)) {
            float v = std::stof(t);
            code.push_back(0xFE);
            uint8_t* p = (uint8_t*)&v;
            for (int i = 0; i < 4; i++) code.push_back(p[i]);
        } else {
            int v = std::stoi(t, nullptr, 0); //dec et hex acceptes
            code.push_back(0xFF);
            for (int i = 0; i < 4; i++) code.push_back((v >> (i * 8)) & 0xFF);
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) lines++;
        if (line.empty() || line[0] == ';') continue;

        //supprime les commentaires
        size_t inline_comment = line.find(';');
        if (inline_comment != std::string::npos) line = trim(line.substr(0, inline_comment));

        //===GESTION MULTI-LIGNES POUR LES MAPS===
        if (line.front() == '{') {
            if (line.back() != '}') {
                std::cerr << "\033[31m" << "[" << lines << "][syntax] Error in line: \"" << line << "\" -> end curly bracket '}' missing" << std::endl;
                exit(1);
            }
            line = line.substr(1, line.size() - 2);
            std::istringstream mapIss(line);
            std::string widthStr, heightStr;
            std::getline(mapIss, widthStr, ',');
            std::getline(mapIss, heightStr, ',');

            Map m;
            try {
                m.width = std::stoi(trim(widthStr));
                m.height = std::stoi(trim(heightStr));
            } catch (const std::invalid_argument&) {
                std::cerr << "\033[31m" << "[" << lines << "] Error: invalid map width/height in line: \"" << line << "\"" << std::endl;
                exit(1);
            }

            std::string pixelStr;
            while (std::getline(mapIss, pixelStr, ',')) { //AJOUTER SYSTEME DE VERIFICATION DE VALIDITE DES NOMBRES
                pixelStr = trim(pixelStr);
                if (!pixelStr.empty()) {
                    try {
                        m.pixels.push_back((uint8_t)std::stoi(pixelStr));
                    } catch (const std::invalid_argument&) {
                        std::cerr << "\033[31m" << "[" << lines << "] Error: pixel '" << pixelStr << "' isn't a valid number in line: \"" << line << "\"" << std::endl;
                        exit(1);
                    }
                }
            }
        }
        //=======================================

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

        if (cmd == "section.data") { inData = true; inCode = false; continue; }
        if (cmd == "section.code") { inCode = true; inData = false; continue; }

        //=====SECTION DATA=====
        if (inData && cmd == "crt") {
            std::string name, val;
            iss >> name >> val;
            name = stripComma(name);
            varOrder.push_back(name);

            //=====TYPE==================
            if (name.rfind("f_", 0) == 0) {
                varType[name] = 3; //float
                floatVars[name] = isFloat(val) ? std::stof(val) : (float)std::stoi(val);
            } else if (name.rfind("i_", 0) == 0) {
                varType[name] = 1; //int
                intVars[name] = isInt(val) ? std::stoi(val) : (int)std::stof(val);
            } else if (name.rfind("s_", 0) == 0) {
                varType[name] = 2; //string

                size_t first = line.find('"');
                size_t last = line.find_last_of('"');
                std::string content = "";

                if (first != std::string::npos && last != std::string::npos && last > first) content = parse_escape_sequences(line.substr(first + 1, last - first - 1));

                strVars[name] = content;
                stringOrder.push_back(name);
            } else if (name.rfind("b_", 0) == 0) {
                varType[name] = 4; //bool
                intVars[name] = std::stoi(val) != 0;
            } else if (name.rfind("m_", 0) == 0) {
                mapOrder.push_back(name);
                varType[name] = 5; //map

                std::string map_line = line;
                size_t first = map_line.find('{');
                size_t last = map_line.find_last_of('}');
                if (first != std::string::npos && last == std::string::npos) {
                    while (i + 1 < source_lines.size()) {
                        std::string next_line = source_lines[++i];
                        map_line += " " + trim(next_line);
                        last = map_line.find_last_of('}');
                        if (last != std::string::npos) break;
                    }
                    first = map_line.find('{');
                    last = map_line.find_last_of('}');
                }

                std::string content = "";
                if (first != std::string::npos && last != std::string::npos && last > first)
                    content = map_line.substr(first + 1, last - first - 1);

                std::istringstream mapStream(content);
                std::string widthStr, heightStr;
                std::getline(mapStream, widthStr, ',');
                std::getline(mapStream, heightStr, ',');

                Map map;
                try {
                    map.width = (uint16_t)std::stoi(trim(widthStr), nullptr, 0);
                    map.height = (uint16_t)std::stoi(trim(heightStr), nullptr, 0);
                } catch (const std::exception&) {
                    std::cerr << "\033[31m" << "[" << lines << "] Error: invalid map's dimensions for '" << name << "'" << std::endl;
                    exit(1);
                }

                size_t expectedPixels = static_cast<size_t>(map.width) * static_cast<size_t>(map.height);
                std::string pixelLine;
                while (std::getline(mapStream, pixelLine)) {
                    std::istringstream pixelStream(pixelLine);
                    std::string pixel;
                    while (std::getline(pixelStream, pixel, ',')) {
                        pixel = trim(pixel);
                        if (pixel.empty()) continue;
                        size_t commentPos = pixel.find(';');
                        if (commentPos != std::string::npos) pixel = trim(pixel.substr(0, commentPos));
                        if (pixel.empty()) continue;
                        if (!isInt(pixel)) continue;
                        if (map.pixels.size() < expectedPixels) {
                            map.pixels.push_back((uint8_t)std::stoi(pixel, nullptr, 0));
                        }
                    }
                }

                if (map.pixels.size() < expectedPixels) map.pixels.resize(expectedPixels, 0);

                mapVars[name] = map;

            } else {
                std::cerr << "\033[31m" << "[" << lines << "] Error: unknown type for variable '" << name << "' (use i_, f_, s_, b_, m_)" << std::endl;
                exit(1);
            }

            continue;
        }

        //=====SECTION CODE=====
        if (!inCode) continue;

        if (cmd.ends_with(".E")) {
            protected_instruction = true;
            cmd.erase(cmd.size() - 2); //retire .E
        }

        //1. labels
        if (cmd.back() == ':') {
            std::string label = cmd.substr(0, cmd.size() - 1);
            if (labels.contains(label)) {
                std::cerr << "\033[31m" << "[" << lines << "] Error: the label <<" << label << ">> already exist" << std::endl;
                exit(1);
            }
            labels[label] = (uint16_t)code.size();
            continue;
        }

        //2. instructions sans arguments
        if (cmd == "ext") { code.push_back(EXT); continue; }
        if (cmd == "ret") { code.push_back(RET); continue; }

        //3. instructions a un argument (PRT, LCT, CAL, JMP, etc.)
        if (cmd == "prt") {
            std::string arg;
            //on recupere tout ce qui reste apres "prt " pour gerer les espaces dans les strings
            size_t pos = line.find("prt");
            arg = trim(line.substr(pos + 3));

            if (arg.front() == '"') {
                std::string content = parse_escape_sequences(arg.substr(1, arg.size() - 2));
                uint16_t strIndex = 0;
                bool found = false;
                for (size_t i = 0; i < stringOrder.size(); ++i) {
                    if (strVars[stringOrder[i]] == content) {
                        strIndex = (uint16_t)i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::string litName = "_lit" + std::to_string(stringOrder.size());
                    strVars[litName] = content;
                    stringOrder.push_back(litName);
                    strIndex = (uint16_t)stringOrder.size() - 1;
                }
                code.push_back(PRT_STR);
                code.push_back(strIndex & 0xFF);
                code.push_back((strIndex >> 8) & 0xFF);
            } else {
                code.push_back(PRT_VAR);
                code.push_back(varIndex(arg));
            }
            continue;
        }

        if (cmd == "lct") { //AJOUTER SYSTEM DE DEBUG
            std::string arg;
            iss >> arg;
            code.push_back(LCT);
            code.push_back(varIndex(stripComma(arg)));
            continue;
        }

        if (cmd == "lif") {
            std::string arg;
            iss >> arg;
            code.push_back(LIF);
            bool hv = false;
            encodeOperand(stripComma(arg), hv);
            continue;
        }

        if (cmd == "cal" || cmd == "jmp" || cmd == "jpt" || cmd == "jpf") {
            uint8_t op = (cmd == "cal") ? CAL : (cmd == "jmp") ? JMP : (cmd == "jpt") ? JPT : JPF;
            std::string lbl;
            iss >> lbl;

            if (lbl.empty()) {
                std::cerr << "\033[31m" << "[" << lines << "][error] JUMPS: invalid label: " << lbl << std::endl;
                exit(1);
            }

            code.push_back(op);
            if (!lbl.empty() && lbl[0] == '$') { //accepte les constantes hex et dec
                lbl.erase(0,1);
                uint16_t addr = (uint16_t)std::stoi(lbl, nullptr, 0);

                code.push_back(addr & 0xFF);
                code.push_back(addr >> 8);
            } else {
                fixups.push_back({lbl, code.size()});
                code.push_back(0);
                code.push_back(0);
            }
            continue;
        }

        if (cmd == "sqr") {
            std::string a;
            iss >> a;

            code.push_back(protected_instruction ? SQR + ERROR_OFFSET : SQR);
            bool hv = false;
            encodeOperand(stripComma(a), hv);
            std::cout << "\033[33m" << "[" << lines << "][warning]SQR (square root): verify if the division by zero may take place" << std::endl;
            continue;
        }

        if (cmd == "psh") {
            std::string arg;
            iss >> arg;
            code.push_back(PSH);
            bool hasVar = false;
            encodeOperand(stripComma(arg), hasVar);
            continue;
        }

        if (cmd == "lod") {
            std::string arg;
            iss >> arg;
            code.push_back(LOD);
            code.push_back(varIndex(stripComma(arg)));
            std::cout << "\033[33m" << "[" << lines << "][warning]LOD (loading): verify if the loaded variable is the good type" << std::endl;
            continue;
        }

        if (cmd == "slp") {
            std::string arg;
            iss >> arg;
            bool hv = false;
            code.push_back(SLP);
            encodeOperand(stripComma(arg), hv);
            continue;
        }

        if (cmd == "tme") {
            std::string arg;
            iss >> arg;
            code.push_back(TME);
            code.push_back(varIndex(stripComma(arg)));
            continue;
        }

        if (cmd == "gpu") {
            std::string arg;
            iss >> arg;
            code.push_back(GPU_E);
            if (arg == "pixel") code.push_back(0);
            else if (arg == "rect") code.push_back(1);
            else if (arg == "voidrect") code.push_back(2);
            else if (arg == "circle") code.push_back(3);
            else if (arg == "voidcircle") code.push_back(4);
            else if (arg == "clear") code.push_back(5);
            else if (arg == "drawmap") code.push_back(6);
            else std::cerr << "\033[31m" << "[" << lines << "] Error: the specified gpu call type doesn't exist: " << arg << std::endl;
            continue;
        } 
        
        if (cmd == "ipf") {
            std::string arg;
            iss >> arg;
            code.push_back(IPF);
            arg = stripComma(arg);
            if (!isInt(arg)) {
                std::cerr << "\033[31m" << "]Error: 'ipf' wait an integer constant without # (ex: ipf 5000)" << std::endl;
                exit(1);
            }

            int val = std::stoi(arg, nullptr, 0);
            for (int i = 0; i < 4; i++) code.push_back((val >> (i * 8)) & 0xFF);
            continue;
        }

        if (cmd == "disk") {
            std::string arg;
            iss >> arg;

            if (arg.front() != '"' || arg.back() != '"') {
                std::cerr << "\033[31m" << "[" << lines << "] Error: disk wait a chain: " << arg << std::endl;
                exit(1);
            }

            std::string content = parse_escape_sequences(arg.substr(1, arg.size() - 2));
            uint16_t strIndex = 0;
            bool found = false;
            for (size_t i = 0; i < stringOrder.size(); i++) {
                if (strVars[stringOrder[i]] == content) {
                    strIndex = i;
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::string litName = "_lit" + std::to_string(stringOrder.size());
                strVars[litName] = content;
                stringOrder.push_back(litName);
                strIndex = stringOrder.size() - 1;
            }

            code.push_back(protected_instruction ? DISK + ERROR_OFFSET : DISK);
            code.push_back(0xFD);
            code.push_back(strIndex & 0xFF);
            code.push_back((strIndex >> 8) & 0xFF);

            continue;
        }

        if (cmd == "error") {
            std::string lbl;
            iss >> lbl;

            if (lbl.empty()) {
                std::cerr << "\033[31m" << "[" << lines << "] Error: invalid label: " << lbl << std::endl;
                exit(1);
            }

            code.push_back(ERROR);
            if (!lbl.empty() && lbl[0] == '$') { //accepte les constantes hex et dec
                lbl.erase(0,1);
                uint16_t addr = (uint16_t)std::stoi(lbl, nullptr, 0);

                code.push_back(addr & 0xFF);
                code.push_back(addr >> 8);
            } else {
                fixups.push_back({lbl, code.size()});
                code.push_back(0);
                code.push_back(0);
            }
            continue;
        }

        //instruction a trois arguments
        if (cmd == "gtc") {
            std::string a, b, c;
            iss >> a >> b >> c;

            a = stripComma(a);
            b = stripComma(b);
            c = stripComma(c);

            std::cout << "\033[33m" << "[" << lines << "][warning]GTC (get char): gtc use in this order [gtc int, str, int]" << std::endl;

            code.push_back(protected_instruction ? GTC + ERROR_OFFSET : GTC);
            code.push_back(varIndex(a));
            code.push_back(varIndex(b));
            code.push_back(varIndex(c));
            continue;
        }

        //4. instructions a deux arguments (ADD, SUB, CPR, etc.)
        if (cmd == "set") {
            std::string a, b;
            iss >> a >> b;
            a = stripComma(a);
            b = stripComma(b);

            code.push_back(protected_instruction ? SET + ERROR_OFFSET : SET);
            code.push_back(varIndex(a)); //destination
            bool hasVar = false;
            encodeOperand(b, hasVar);
            continue;
        }

        if (cmd == "its") {
            std::string a, b;
            iss >> a >> b;
            a = stripComma(a);
            b = stripComma(b);

            code.push_back(protected_instruction ? ITS + ERROR_OFFSET : ITS);
            code.push_back(varIndex(a)); //index de la variable destination
            code.push_back(varIndex(b)); //index de la variable source
            continue;
        }

        if (cmd == "wrt") {
            std::string src, addr;
            iss >> src >> addr;
            code.push_back(protected_instruction ? WRT + ERROR_OFFSET : WRT);
            src = stripComma(src);
            addr = stripComma(addr);

            //variable à ecrire
            code.push_back(varIndex(src));

            //adresse
            bool hasVar = false;
            if (!addr.empty() && addr[0] == '$') addr.erase(0, 1);
            encodeOperand(addr, hasVar);
            continue;
        }

        if (cmd == "rad") {
            std::string dst, addr;
            iss >> dst >> addr;
            code.push_back(protected_instruction ? RAD + ERROR_OFFSET : RAD);
            dst = stripComma(dst);
            addr = stripComma(addr);

            //variable destination
            code.push_back(varIndex(dst));

            //adresse
            bool hasVar = false;
            if (!addr.empty() && addr[0] == '$') addr.erase(0, 1);
            encodeOperand(addr, hasVar);

            continue;
        }

        //instructions a deux arguments classiques
        std::string a, b;
        if (iss >> a >> b) {
            a = stripComma(a);
            b = stripComma(b);
            bool hasVar = false;

            if (cmd == "add")      code.push_back(protected_instruction ? ADD + ERROR_OFFSET : ADD);
            else if (cmd == "sub") code.push_back(protected_instruction ? SUB + ERROR_OFFSET : SUB);
            else if (cmd == "mul") code.push_back(protected_instruction ? MUL + ERROR_OFFSET : MUL);
            else if (cmd == "div") code.push_back(protected_instruction ? DIV + ERROR_OFFSET : DIV);
            else if (cmd == "pow") code.push_back(protected_instruction ? POW + ERROR_OFFSET : POW);
            else if (cmd == "cpr") code.push_back(protected_instruction ? CPR + ERROR_OFFSET : CPR);
            else if (cmd == "cpg") code.push_back(protected_instruction ? CPG + ERROR_OFFSET : CPG);
            else if (cmd == "and") code.push_back(AND);
            else if (cmd == "or")  code.push_back(OR);
            else if (cmd == "nnd") code.push_back(NND);
            else if (cmd == "nor") code.push_back(NOR);
            else if (cmd == "rnd") code.push_back(RND);

            encodeOperand(a, hasVar);
            encodeOperand(b, hasVar);
            continue;
        }

        std::cout << "[" << lines << "] Error: unknown op code: " << cmd << std::endl;
        exit(1);
    }

    //=====ECRITURE DU BINAIRE=====

    out.write("CASM", 4);
    uint8_t ver = 6;
    out.write((char*)&ver, 1);
    uint16_t entry = labels.count("main") ? labels["main"] : 0;
    out.write((char*)&entry, 2);

    uint16_t vc = varOrder.size();
    out.write((char*)&vc, 2);
    uint16_t strIndexVar = 0;
    uint16_t mapIndexVar = 0;

    for (auto& v : varOrder) {
        out.put(varType[v]);
        out.put(0);
        if (varType[v] == 1 || varType[v] == 4) out.write((char*)&intVars[v], 4);
        else if (varType[v] == 3) out.write((char*)&floatVars[v], 4);
        else if (varType[v] == 2) {
            uint32_t idx = strIndexVar++;
            out.write((char*)&idx, 4);
        } else if (varType[v] == 5) {
            uint32_t idx = mapIndexVar++;
            out.write((char*)&idx, 4);
        }
    }

    //chargement des strings litterales
    uint16_t sc = stringOrder.size();
    out.write((char*)&sc, 2);
    for (auto& name : stringOrder) {
        auto& s = strVars[name];
        uint16_t l = s.size();
        out.write((char*)&l, 2);
        out.write(s.c_str(), l);
    }

    //chargement des maps
    uint16_t mc = mapOrder.size();
    out.write((char*)&mc, 2);
    for(auto& name : mapOrder) {
        Map& m = mapVars[name];
        out.write((char*)&m.width, 2);
        out.write((char*)&m.height, 2);
        out.write((char*)m.pixels.data(), m.pixels.size());
    }

    //chargement du code
    for (auto& f : fixups) {
        if (labels.find(f.label) == labels.end()) {
            std::cerr << "\033[31m" << "[" << lines << "] Error: label '" << f.label << "' unfinded" << std::endl;
            exit(1);
        }

        uint16_t addr = labels[f.label];
        std::cout << "\033[33m" << "[DEBUG] Fixup for '" << f.label << "' -> adresse 0x" << std::hex << addr << std::endl;
        code[f.position] = addr & 0xFF; //octet bas
        code[f.position + 1] = (addr >> 8); //octet haut
    }

    out.write((char*)code.data(), code.size());

    std::cout << "\033[0m" << "Compilation finished: " << argv[2] << std::endl;
    return 0;
}

//magnus carlsen 2024-06 for ГПСД, XS проект
//v7
