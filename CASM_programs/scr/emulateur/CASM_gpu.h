#pragma once
#include "raylib.h"
#include <cstdint>

enum GPU_colors {
    CPU_LIGHTGRAY = 0,
    CPU_GRAY,
    CPU_DARKGRAY,
    CPU_YELLOW,
    CPU_GOLD,
    CPU_ORANGE,
    CPU_PINK,
    CPU_RED,
    CPU_MAROON,
    CPU_GREEN,
    CPU_LIME,
    CPU_DARKGREEN,
    CPU_SKYBLUE,
    CPU_BLUE,
    CPU_DARKBLUE,
    CPU_PURPLE,
    CPU_VIOLET,
    CPU_DARKPURPLE,
    CPU_BEIGE,
    CPU_BROWN,
    CPU_DARKBROWN,
    CPU_WHITE,
    CPU_BLACK,
    CPU_MAGENTA,
    CPU_RAYWHITE
};

class GPU {
public:
    static constexpr int WIDTH = 640;
    static constexpr int HEIGHT = 480;
    Color framebuffer[WIDTH * HEIGHT];

    GPU() { clear(BLACK); }

    void clear(Color color) { for(int i = 0; i < WIDTH * HEIGHT; i++) framebuffer[i] = color; }
    void pixel(int x, int y, Color color) {
        if (x < 0 || x >= WIDTH) return;
        if (y < 0 || y >= HEIGHT) return;

        framebuffer[y * WIDTH + x] = color;
    }
    void draw(int scale) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) DrawRectangle(x * scale, y * scale, scale, scale, framebuffer[y * WIDTH + x]);
        }
    }
    void rect(int x, int y, int w, int h, Color color) {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) pixel(x + i, y + j, color);
        }
    }
    void voidrect(int x, int y, int w, int h, int thickness, Color color) {
        rect(x, y, w, thickness, color); //haut
        rect(x, y + h - thickness, w, thickness, color); //bas
        rect(x, y, thickness, h, color); //gauche
        rect(x + w - thickness, y, thickness, h, color); //droite
    }
    void circle(int centerX, int centerY, int radius, Color color) {
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                if (x * x + y * y <= radius * radius) pixel(centerX + x, centerY + y, color);
            }
        }
    }
    void voidcircle(int centerX, int centerY, int radius, int thickness, Color color) {
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                int distSq = x * x + y * y;
                if (distSq <= radius * radius && distSq >= (radius - thickness) * (radius - thickness))
                    pixel(centerX + x, centerY + y, color);
            }
        }
    }
    Color palette[25] = {
        LIGHTGRAY,
        GRAY,
        DARKGRAY,
        YELLOW,
        GOLD,
        ORANGE,
        PINK,
        RED,
        MAROON,
        GREEN,
        LIME,
        DARKGREEN,
        SKYBLUE,
        BLUE,
        DARKBLUE,
        PURPLE,
        VIOLET,
        DARKPURPLE,
        BEIGE,
        BROWN,
        DARKBROWN,
        WHITE,
        BLACK,
        MAGENTA,
        RAYWHITE
    };
    void drawmap(int x, int y, int w, int h, uint8_t* map) {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                uint8_t colorIndex = map[j * w + i];
                pixel(x + i, y + j, palette[colorIndex]);
            }
        }
    }
};

//magnus carlasen 2024-06 for ГПСД, XS проект