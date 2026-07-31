#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

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

class Bios {
public:
    std::vector<Variable> vars;
    std::vector<std::string> strings;
    std::vector<Map> maps;
    std::vector<uint8_t> code;
    uint16_t pc = 0;

    std::vector<std::string> disk;
    std::string disk_name;

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

        //1.variables
        uint16_t varCount;
        file.read((char*)&varCount, 2);
        vars.assign(varCount, Variable());
        for (int i = 0; i < varCount; ++i) {
            uint8_t type = 0;
            uint8_t reserved = 0;
            uint32_t rawValue = 0;
            file.read((char*)&type, 1);
            file.read((char*)&reserved, 1);
            file.read((char*)&rawValue, 4);

            vars[i].type = type;
            switch (type) {
                case 1: {
                    vars[i].i = static_cast<int32_t>(rawValue);
                    break;
                } case 3: {
                    std::memcpy(&vars[i].f, &rawValue, sizeof(rawValue));
                    break;
                } default: {
                    vars[i].index = rawValue;
                    break;
                }
            }
        }

        //2.chaines
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

        //3.maps
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

        //4.code
        code.clear();
        code.assign(std::istreambuf_iterator<char>(file), {});

        pc = entry;
        file.close();
        return (code.size() > 0);
    }

    bool disk_load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Impossible d'ouvrir le disque " << filename << std::endl;
            return false;
        }

        disk.clear();
        std::string line;
        while (std::getline(file, line)) disk.push_back(line);
        disk_name = filename;
        file.close();
        return true;
    }

    bool disk_save() {
        if (disk_name.empty()) {
            std::cerr << "Aucun disque charge, impossible de sauvegarder." << std::endl;
            return false;
        }

        std::ofstream file(disk_name);
        if (!file.is_open()) {
            std::cerr << "Impossible d'ecrire le disque " << disk_name << std::endl;
            return false;
        }

        for (const auto& line : disk) file << line << std::endl;
        file.close();
        return true;
    }
};

//magnus carlsen 2024-06 for ГПСД, XS проект