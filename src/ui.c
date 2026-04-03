#include "util.h"

void DrawSimulation() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell c = grid[y * WIDTH + x];
            if (c.density < 0.05f && fabsf(c.temp) < 0.1f && c.moisture < 0.1f) continue;

            // Map scalars to RGB for emergent colors
            unsigned char r = (unsigned char)fmin(255, fmax(0, c.temp * 15));
            unsigned char g = (unsigned char)fmin(255, fmax(0, c.density * 25));
            unsigned char b = (unsigned char)fmin(255, fmax(0, c.moisture * 20));
            
            DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, 255});
        }
    }
}

void DrawInterface(Player *p) {
    const char* elements[] = {"THERMAL", "FLUID", "MASS", "KINETIC"};
    
    DrawRectangle(0, 0, 320, 110, Fade(BLACK, 0.8f));
    DrawRectangleLines(0, 0, 320, 110, GOLD);
    
    DrawText("UNIVERSAL FIELD SIMULATOR", 15, 15, 15, GOLD);
    // This now correctly reads the player's activeElement from main.c
    DrawText(TextFormat("ACTIVE FIELD: [ %s ]", elements[p->activeElement]), 15, 45, 12, SKYBLUE);
    DrawText("MOUSE 1: Inject Energy", 15, 70, 10, RAYWHITE);
    DrawText("KEYS 1-4: Switch Fundamental Scalar", 15, 85, 10, LIGHTGRAY);
}