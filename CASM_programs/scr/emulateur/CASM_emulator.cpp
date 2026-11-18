#include <string>
#include <iostream>
#include <chrono>
#include "raylib.h"
#include "CASM_gpu.hpp"
#include "CASM_cpu.hpp"

int main(int argc, char* argv[]) {
    int scale = 4;
    if (argc > 1) {
        try {
            scale = std::stoi(argv[1]);
            if (scale < 1) scale = 4;
        } catch (const std::exception&) {
            std::cerr << "Error: first argument must be the scale of the window" << std::endl;
        }
    }

    CPU cpu;
    if (argc > 2) {
        std::string filename = argv[2];
        if (!cpu.load(filename)) { //chargement du programme CASM
            std::cerr << "Error while charging CASM: '" << filename << "'" << std::endl;
            std::cerr << "Wants: a compiled binary CASM (.bin), not a source file .casm." << std::endl;
            std::cerr << "Exemple: CASM_emulator 4 programs\\test.bin" << std::endl;
            return 1;
        }
    }

    InitWindow(GPU::WIDTH * scale, GPU::HEIGHT * scale, "CASM Emulator");
    SetTargetFPS(60);

    while (!WindowShouldClose() && cpu.running) {
        if (cpu.is_sleeping && std::chrono::steady_clock::now() >= cpu.sleep_until) cpu.is_sleeping = false;

        cpu.update_inputs(); //actualisation des registres d'entree
        if (!cpu.is_sleeping) {
            for (int i = 0; i < cpu.ipf && cpu.running && !cpu.is_sleeping; i++) cpu.exe_instruction();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        cpu.gpu.draw(scale);
        EndDrawing();
    }
    CloseWindow();
}

//@KOT76b; magnus carlasen 2024-06 for ГПСД и XS проект
