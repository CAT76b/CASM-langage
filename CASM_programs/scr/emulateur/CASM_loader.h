#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

struct Variable {
    uint8_t type = 0;
    union {
        int32_t i;
        float f;
        uint32_t index;
    };
};

struct Map {
    uint16_t width;
    uint16_t height;
    std::vector<uint8_t> pixels;
};

class Loader {
public:
    std::vector<Variable> vars;
    std::vector<std::string> strings;
    std::vector<Map> maps;
    std::vector<uint8_t> code;
    uint16_t pc = 0;

    bool load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        //lecture du header
        char magic[4];
        file.read(magic, 4);
        if (std::string(magic, 4) != "CASM") return false;
        uint8_t version;
        file.read((char*)& version, 1);
        uint16_t entry;
        file.read((char*)& entry, 2);

        //1. variables
        uint16_t varCount;
        file.read((char*)&varCount, 2);
        vars.assign(varCount, Variable());
        for (int i = 0; i < varCount; ++i) {
            file.read((char*)&vars[i].type, 1);
            file.ignore(1); 
            file.read((char*)&vars[i].index, 4);
        }

        //2. chaines
        uint16_t strCount;
        file.read((char*)&strCount, 2);
        strings.assign(strCount, "");
        for (int i = 0; i < strCount; ++i) {
            uint16_t len;
            file.read((char*)&len, 2);
            if(len > 0) {
                strings[i].resize(len);
                file.read(&strings[i][0], len);
            }
        }

        //3. maps
        uint16_t mapCount;
        file.read((char*)&mapCount, 2);
        maps.assign(mapCount, Map());
        for(int i = 0; i < mapCount; i++) {
            file.read((char*)&maps[i].width, 2);
            file.read((char*)&maps[i].height, 2);
            size_t size = maps[i].width * maps[i].height;
            if(size > 1000000) return false;
            maps[i].pixels.resize(size);
            file.read((char*)maps[i].pixels.data(), size);
        }

        //4. code
        code.clear();
        code.assign(std::istreambuf_iterator<char>(file), {});

        pc = entry; //on commence au debut du vecteur 'code'
        file.close();
        return (code.size() > 0);
    }
};

//magnus carlsen 2024-06 for ГПСД, XS проект