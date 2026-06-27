#include "raylib.h"
#include "CASM_gpu.h"
#include "CASM_emulator.h"
#include <string>
#include <iostream>

int main(int argc, char* argv[]) {
    int scale = 4;
    if (argc > 1) {
        scale = std::stoi(argv[1]);
        if (scale < 1) scale = 1;
    }

    CPU cpu;
    if (argc > 2) {
        std::string filename = argv[2];
        if (!cpu.load(filename)) {
            std::cerr << "Erreur chargement CASM" << std::endl;
            return 1;
        }
    }

    InitWindow(GPU::WIDTH * scale, GPU::HEIGHT * scale, "CASM Emulator");
    SetTargetFPS(60);

    while (!WindowShouldClose() && cpu.running) {
        cpu.load(argv[2]);
        cpu.exe_instruction();
        BeginDrawing();
        ClearBackground(BLACK);
        cpu.gpu.draw(scale);
        EndDrawing();
    }
}

//magnus carlasen 2024-06 for ГПСД, XS проект