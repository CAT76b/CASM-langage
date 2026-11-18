#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <iterator>

class Bios {
public:
    std::vector<uint8_t> code;
    std::vector<uint8_t> memory;
    uint32_t pc = 0;
    uint32_t entry = 0;
    uint32_t memory_size = 0;
    uint32_t code_size = 0;

private:
    bool read_U32(std::ifstream& file, uint32_t& value) {
        uint8_t bytes[4];
        if (!file.read(reinterpret_cast<char*>(bytes), 4)) return false;
        value = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) | (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);

        return true;
    }

public:
    bool load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);

        if (!file) return false;

        //1. signature
        char magic[4];
        if (!file.read(magic, 4)) return false;
        if (std::string(magic, 4) != "CASM") return false;

        //2. version
        uint8_t version;
        if (!file.read(reinterpret_cast<char*>(&version), 1)) return false;
        if (version < 8) {
            std::cerr << "[BIOS] Unsupported CASM version: " << static_cast<int>(version) << std::endl;
            return false;
        }

        //3. header
        if (!read_U32(file, entry)) return false;
        if (!read_U32(file, memory_size)) return false;
        if (!read_U32(file, code_size)) return false;

        //4. validation
        if (code_size == 0 || entry >= code_size) {
            std::cerr << "[BIOS] Invalid entry point or code size" << std::endl;
            return false;
        }

        //5. chargement du code
        code.resize(code_size);
        if (!file.read(reinterpret_cast<char*>(code.data()), code_size)) {
            std::cerr << "[BIOS] Incomplete code section" << std::endl;
            return false;
        }

        //verifie qu'il n'y a pas de donnees supplementaires
        if (file.peek() != EOF) {
            std::cerr << "[BIOS] Unexpected data after code section" << std::endl;
            return false;
        }

        //6. allocation des buffers
        memory.assign(memory_size, 0);

        //7. initialisation PC
        pc = entry;

        return true;
    }
};

//@KOT76b; magnus carlasen 2024-06 for ГПСД и XS проект
