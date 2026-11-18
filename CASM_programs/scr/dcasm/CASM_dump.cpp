
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <string>
#include <iterator>

//lecture d'un uint32_t little-endian
uint32_t readUint32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1]) << 8)
    | (static_cast<uint32_t>(data[offset + 2]) << 16) | (static_cast<uint32_t>(data[offset + 3]) << 24);
}

//affichage d'un uint32_t
void printUint32(uint32_t value) {
    std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec;
}

//affichage d'une zone memoire en hex
void printMemory(const std::vector<uint8_t>& data, size_t start, size_t size) {
    constexpr size_t BYTES_PER_LINE = 16;

    for (size_t i = 0; i < size; i += BYTES_PER_LINE) {
        size_t count = std::min(BYTES_PER_LINE, size - i);

        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << i << " | ";

        for (size_t j = 0; j < count; ++j) std::cout << std::setw(2) << static_cast<int>(data[start + i + j]) << " ";

        std::cout << std::dec << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: CASM_dump <fichier.bin>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);

    if (!file) {
        std::cerr << "Erreur: impossible d'ouvrir "
        << argv[1] << "\n";
        return 1;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::cout << "=== Analyse du fichier CASM ===\n";
    std::cout << "Fichier: " << argv[1] << "\n";
    std::cout << "Taille: " << data.size() << " octets\n\n";

    //verif header
    constexpr size_t HEADER_SIZE = 17;

    if (data.size() < HEADER_SIZE) {
        std::cerr << "Erreur: fichier trop court pour un header CASM v8.\n";
        return 1;
    }

    if (std::string(data.begin(), data.begin() + 4) != "CASM") {
        std::cerr << "Erreur: signature invalide.\n";
        return 1;
    }

    //lecture header
    uint8_t version = data[4];
    uint32_t entryPoint = readUint32(data, 5);
    uint32_t memorySize = readUint32(data, 9);
    uint32_t codeSize = readUint32(data, 13);

    std::cout << "===Header===\n";
    std::cout << "Signature: CASM\n";
    std::cout << "Version: " << static_cast<int>(version) << "\n";

    std::cout << "Point d'entree: ";
    printUint32(entryPoint);
    std::cout << "\n";

    std::cout << "Taille memoire: " << memorySize << " octets\n";
    std::cout << "Taille code: " << codeSize << " octets\n";

    //verif tailles
    uint64_t expectedSize =
    static_cast<uint64_t>(HEADER_SIZE)
    + memorySize
    + codeSize;

    if (expectedSize != data.size()) {
        std::cerr << "\nErreur: tailles incoherentes.\n";
        std::cerr << "Taille attendue: " << expectedSize << "\n";
        std::cerr << "Taille reelle: " << data.size() << "\n";
        return 1;
    }

    // Affichage de la mémoire initiale
    size_t memoryOffset = HEADER_SIZE;

    std::cout << "\n=== Memoire initiale (" << memorySize << " octets) ===\n";

    if (memorySize == 0) std::cout << "(Memoire vide)\n";
    else printMemory(data, memoryOffset, memorySize);

    //affichage du code
    size_t codeOffset = HEADER_SIZE + memorySize;

    std::cout << "\n=== Code (" << codeSize << " octets) ===\n";

    std::cout << "Offset     | Hex  | Binaire\n";
    std::cout << "-----------------------------\n";

    for (size_t i = 0; i < codeSize; ++i) {
        uint8_t byte = data[codeOffset + i];

        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << i << " | ";
        std::cout << std::setw(2) << static_cast<int>(byte) << "   | ";
        for (int k = 7; k >= 0; --k) std::cout << ((byte >> k) & 1);
        std::cout << "\n";
    }

    std::cout << std::dec;
    std::cout << "\n=== Analyse terminee ===\n";

    return 0;
}

// magnus carlsen 2024-06, for ГПСД, XS проект
// CASM v8
