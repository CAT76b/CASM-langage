#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <string>

//structure pour l'en-tete CASM (4 premiers octets = "CASM")
#pragma pack(push, 1)
struct CASMHeader {
    char     magic[4];    //"CASM"
    uint8_t  version;     //version (1 octet)
    uint16_t entryPoint;  //point d'entree (2 octets, little-endian)
    uint16_t varCount;    //nombre de variables (2 octets)
};
#pragma pack(pop)

//fonction pour afficher un uint16_t en little-endian
void printUint16(const uint8_t* data) {
    uint16_t value = data[0] | (data[1] << 8);
    std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0') << value;
}

//fonction pour afficher un uint32_t en little-endian
void printUint32(const uint8_t* data) {
    uint32_t value = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
}

//fonction pour afficher un float en little-endian
void printFloat(const uint8_t* data) {
    float value;
    uint8_t* p = (uint8_t*)&value;
    p[0] = data[0];
    p[1] = data[1];
    p[2] = data[2];
    p[3] = data[3];
    std::cout << value;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: CASM_dump <fichier.bin>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Erreur: impossible d'ouvrir " << argv[1] << "\n";
        return 1;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::cout << "===Analyse du fichier CASM: " << argv[1] << "===\n";
    std::cout << "Taille: " << data.size() << " octets\n\n";

    if (data.size() < 4) {
        std::cerr << "Erreur: fichier trop court pour etre un binaire CASM.\n";
        return 1;
    }

    //1. vrification de la signature
    std::cout << "===En-tete===\n";
    std::cout << "Signature: ";
    for (int i = 0; i < 4; ++i) std::cout << data[i];
    std::cout << " (attendu: \"CASM\")\n";

    if (std::string(data.begin(), data.begin() + 4) != "CASM") {
        std::cerr << "Erreur: signature invalide.\n";
        return 1;
    }

    //2. version et point d'entrée
    std::cout << "Version: " << (int)data[4] << "\n";
    std::cout << "Point d'entree: ";
    printUint16(&data[5]);
    std::cout << "\n";

    //3. nombre de variables
    uint16_t varCount = data[7] | (data[8] << 8);
    std::cout << "Nombre de variables: " << varCount << "\n";

    //4. affichage des variables (simplifie)
    size_t offset = 9; // Début des variables
    std::cout << "\n===Variables (" << varCount << ")===\n";
    for (uint16_t i = 0; i < varCount; ++i) {
        uint8_t type = data[offset];
        std::cout << "Variable " << i << ": type=" << (int)type << " | ";

        if (type == 1) { //int
            std::cout << "valeur (int32): ";
            printUint32(&data[offset + 2]);
        } else if (type == 2) { //string (index)
            std::cout << "index chaine: ";
            printUint32(&data[offset + 2]);
        } else if (type == 3) { //float
            std::cout << "valeur (float): ";
            printFloat(&data[offset + 2]);
        }
        std::cout << "\n";
        offset += 6; //type (1) + reserve (1) + valeur (4)
    }

    //5. affichage des chaines
    uint16_t stringCount = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    std::cout << "\n===Chaines (" << stringCount << ")===\n";
    for (uint16_t i = 0; i < stringCount; ++i) {
        uint16_t len = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        std::cout << "Chaine " << i << " (longueur=" << len << "): \"";
        for (uint16_t j = 0; j < len; ++j) std::cout << data[offset + j];
        std::cout << "\"\n";
        offset += len;
    }

    //6. affichage du code (brut)
    std::cout << "\n===Code===\n";
    std::cout << "Offset | Hex       | Binaire (par octet)\n";
    std::cout << "========================================\n";
    for (; offset < data.size(); ++offset) {
        std::cout << std::dec << std::setw(5) << offset << " | ";
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[offset] << "        | ";
        for (int k = 7; k >= 0; --k) std::cout << ((data[offset] >> k) & 1);
        std::cout << "\n";
    }
    return 0;
}

//magnus carlsen 2024-06
//2