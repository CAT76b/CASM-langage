#include "raylib.h"
#include "CASM_gpu.h"
#include "CASM_emulator.h"
#include <string>
#include <iostream>

int main(int argc, char* argv[]) {
    int scale = 4;
    if (argc > 1) {
        try {
            scale = std::stoi(argv[1]);
            if (scale < 1) scale = 1;
        } catch (const std::exception&) {
            std::cerr << "Erreur: premier argument doit etre le scale de la fenetre" << std::endl;
        }
    }

    CPU cpu;
    if (argc > 2) {
        std::string filename = argv[2];
        if (!cpu.load(filename)) { //chargement du programme CASM
            std::cerr << "Erreur chargement CASM: '" << filename << "'" << std::endl;
            std::cerr << "Attendu: un binaire CASM compile (.bin), pas un fichier source .casm." << std::endl;
            std::cerr << "Exemple: CASM_emulator 4 programs\\test.bin" << std::endl;
            return 1;
        }
    }

    InitWindow(GPU::WIDTH * scale, GPU::HEIGHT * scale, "CASM Emulator");
    SetTargetFPS(60);

    while (!WindowShouldClose() && cpu.running) {
        if (cpu.is_sleeping) {
            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            if (now >= cpu.sleep_until) cpu.is_sleeping = false;
        }

        if (!cpu.is_sleeping) {
            cpu.maj_Inputs_registers(); //actualisation des registres d'entree
            for (int i = 0; i < cpu.ipf && cpu.running && !cpu.is_sleeping; i++) cpu.exe_instruction();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        cpu.gpu.draw(scale);
        EndDrawing();
    }
    CloseWindow();
}

//magnus carlsen 2024-06 for ГПСД, XS проект