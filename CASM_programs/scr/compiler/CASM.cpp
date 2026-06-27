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
#include <string>
std::unordered_set<std::string> included_headers;
std::vector<std::string> source_lines;

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
    GPU_E
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

void include_header(const std::string& name) {
    if (included_headers.count(name)) return;
    included_headers.insert(name);

    //1. lire dependances
    std::ifstream deps("../scr/headers/" + name + ".deps.txt");
    if (deps) {
        std::string dep;
        while (std::getline(deps, dep)) {
            dep = trim(dep);
            if (!dep.empty()) include_header(dep);
        }
    }

    //2. inclure le header lui-meme
    std::ifstream file("../scr/headers/" + name);
    if (!file) {
        std::cerr << "Header non trouve: " << name << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) source_lines.push_back(line); //ou push dans ton buffer source
}

//=======MAIN=======

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: CASM <input.casm> <output.bin>" << std::endl;
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) { std::cerr << "Erreur ouverture source" << std::endl; return 1; }

    std::ofstream out(argv[2], std::ios::binary);
    if (!out) { std::cerr << "Erreur ouverture binaire" << std::endl; return 1; }

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

    bool inData = false, inCode = false;

    auto varIndex = [&](const std::string& v) -> uint8_t {
        auto it = std::find(varOrder.begin(), varOrder.end(), v);
        if (it == varOrder.end()) {
            std::cerr << "Variable inconnue: " << v << std::endl;
            exit(1);
        }
        return (uint8_t)std::distance(varOrder.begin(), it);
    };

    auto encodeOperand = [&](const std::string& t, bool& hasVar, bool forceVar = false) {
        if (!isInt(t) && !isFloat(t) || forceVar) {
            hasVar = true;
            code.push_back(varIndex(t));
        } else if (isFloat(t)) {
            float v = std::stof(t);
            code.push_back(0xFE);
            uint8_t* p = (uint8_t*)&v;
            for (int i = 0; i < 4; i++) code.push_back(p[i]);
        } else {
            int v = std::stoi(t);
            code.push_back(0xFF);
            for (int i = 0; i < 4; i++) code.push_back((v >> (i*8)) & 0xFF);
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;

        std::istringstream iss(line);
        std::string cmd, file;
        iss >> cmd;
        if (cmd == "header") {
            iss >> file;
            //retirer les guillemets si presents
            file.erase(std::remove(file.begin(), file.end(), '"'), file.end());
            include_header(file);
            continue;
        }
        source_lines.push_back(line);
    }

    for (const std::string& line : source_lines) {
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

            //=====TYPE DEDUIT DU NOM=====
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

                if (first != std::string::npos && last != std::string::npos && last > first)
                    content = parse_escape_sequences(line.substr(first + 1, last - first - 1));

                strVars[name] = content;
                stringOrder.push_back(name);
            } else if (name.rfind("b_", 0) == 0) {
                varType[name] = 4; //bool
                intVars[name] = std::stoi(val) != 0;
            } else if (name.rfind("m_", 0) == 0) {
                mapOrder.push_back(name);
                varType[name] = 5; //map
                size_t first = line.find('{');
                size_t last = line.find_last_of('}');
                std::string content = "";
                if (first != std::string::npos && last != std::string::npos && last > first)
                    content = line.substr(first + 1, last - first - 1);
                std::istringstream mapStream(content);
                std::string widthStr, heightStr, pixelsStr;
                std::getline(mapStream, widthStr, ',');
                std::getline(mapStream, heightStr, ',');
                std::getline(mapStream, pixelsStr);
                Map map;
                mapVars[name] = map;
                map.width = (uint16_t)std::stoi(trim(widthStr), nullptr, 0);
                map.height = (uint16_t)std::stoi(trim(heightStr), nullptr, 0);
                std::istringstream pixelsStream(pixelsStr);
                std::string pixel;
                while (std::getline(pixelsStream, pixel, ','))
                    map.pixels.push_back((uint8_t)std::stoi(trim(pixel), nullptr, 0));
                mapVars[name] = map;
                if (map.pixels.size() != map.width * map.height) {
                    std::cerr << "Erreur: taille de map invalide pour " << name << std::endl;
                    exit(1);
                }

            } else {
                std::cerr << "Erreur: type inconnu pour variable '" << name << "' (utiliser i_, f_, s_, b_, m_)" << std::endl;
                exit(1);
            }

            continue;
        }

        //=====SECTION CODE=====
        if (!inCode) continue;

        //1. labels
        if (cmd.back() == ':') {
            std::string label = cmd.substr(0, cmd.size() - 1);
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

        if (cmd == "lct") {
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
            code.push_back(op);
            fixups.push_back({lbl, code.size()});
            code.push_back(0); code.push_back(0);
            continue;
        }

        if (cmd == "sqr") {
            std::string a;
            iss >> a;
            code.push_back(SQR);
            bool hv = false;
            encodeOperand(stripComma(a), hv);
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
            else if (arg == "drawmap") code.push_back(5);
            continue;
        }

        //4. instructions a deux arguments (ADD, SUB, CPR, etc.)
        if (cmd == "set") {
            std::string a, b;
            iss >> a >> b;
            a = stripComma(a);
            b = stripComma(b);

            code.push_back(SET);
            code.push_back(varIndex(a));
            code.push_back(varIndex(b));
            continue;
        }

        //instructions a deux arguments classiques
        std::string a, b;
        if (iss >> a >> b) {
            a = stripComma(a);
            b = stripComma(b);
            bool hasVar = false;

            if (cmd == "add") code.push_back(ADD);
            else if (cmd == "sub") code.push_back(SUB);
            else if (cmd == "mul") code.push_back(MUL);
            else if (cmd == "div") code.push_back(DIV);
            else if (cmd == "pow") code.push_back(POW);
            else if (cmd == "cpr") code.push_back(CPR);
            else if (cmd == "cpg") code.push_back(CPG);
            else if (cmd == "and") code.push_back(AND);
            else if (cmd == "or")  code.push_back(OR);
            else if (cmd == "nnd") code.push_back(NND);
            else if (cmd == "nor") code.push_back(NOR);
            else if (cmd == "rnd") code.push_back(RND);
            else continue;

            encodeOperand(a, hasVar);
            encodeOperand(b, hasVar);
        }
    }

    //=====ECRITURE DU BINAIRE=====

    out.write("CASM", 4);
    uint8_t ver = 4;
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
        if (varType[v] == 1) out.write((char*)&intVars[v], 4);
        else if (varType[v] == 3) out.write((char*)&floatVars[v], 4);
        else if (varType[v] == 2) {
            uint32_t idx = strIndexVar++;
            out.write((char*)&idx, 4);
        } else if (varType[v] == 5) {
            uint32_t idx = mapIndexVar++;
            out.write((char*)&idx, 4);
        } else {
            auto it = std::find(stringOrder.begin(), stringOrder.end(), v);
            uint32_t idx = std::distance(stringOrder.begin(), it);
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
            std::cerr << "Erreur: Label '" << f.label << "' non trouve!" << std::endl;
            exit(1);
        }
        uint16_t addr = labels[f.label];
        std::cout << "[DEBUG] Fixup pour '" << f.label << "' -> adresse 0x" << std::hex << addr << std::endl;
        code[f.position] = addr & 0xFF; //octet bas
        code[f.position + 1] = (addr >> 8); //octet haut
    }

    out.write((char*)code.data(), code.size());

    std::cout << "Compilation OK: " << argv[2] << std::endl;
    return 0;
}

//magnus carlasen 2024-06 for ГПСД, XS проект
//v4