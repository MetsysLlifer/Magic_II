#include "util.h"

Cell grid[WIDTH * HEIGHT];
Cell prev_grid[WIDTH * HEIGHT];

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));

    // RANDOM OBJECTS: Spawn "Natural" structures using the same Scalar Principles
    for (int i = 0; i < 15; i++) {
        int rx = GetRandomValue(10, WIDTH - 10);
        int ry = GetRandomValue(10, HEIGHT - 10);
        int size = GetRandomValue(2, 5);
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int idx = (ry + dy) * WIDTH + (rx + dx);
                if (idx >= 0 && idx < WIDTH * HEIGHT) {
                    grid[idx].density = 50.0f; // High Density = Rock
                    grid[idx].permanent = true; 
                }
            }
        }
    }
}

void Diffuse(float dt) {
    float rate = 0.08f * dt * WIDTH * HEIGHT;
    for (int k = 0; k < 5; k++) {
        for (int y = 1; y < HEIGHT - 1; y++) {
            for (int x = 1; x < WIDTH - 1; x++) {
                int i = y * WIDTH + x;
                if (grid[i].permanent) continue; // Permanent objects don't diffuse away
                grid[i].temp = (prev_grid[i].temp + rate * (grid[i-1].temp + grid[i+1].temp + grid[i-WIDTH].temp + grid[i+WIDTH].temp)) / (1 + 4 * rate);
                grid[i].density = (prev_grid[i].density + rate * (grid[i-1].density + grid[i+1].density + grid[i-WIDTH].density + grid[i+WIDTH].density)) / (1 + 4 * rate);
            }
        }
    }
}

void UpdateSimulation(float dt) {
    memcpy(prev_grid, grid, sizeof(grid));
    Diffuse(dt);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        if (!grid[i].permanent) {
            grid[i].temp *= 0.98f;
            grid[i].density *= 0.99f;
            grid[i].moisture *= 0.99f;
        }
    }
}

void InjectEnergy(int x, int y, SpellDNA dna) {
    if (x < 1 || x >= WIDTH - 1 || y < 1 || y >= HEIGHT - 1) return;
    int i = y * WIDTH + x;
    grid[i].temp += dna.temp;
    grid[i].density += dna.density;
    grid[i].moisture += dna.moisture;
    grid[i].velocity.x += dna.velocity.x;
    grid[i].velocity.y += dna.velocity.y;
    if (dna.isPermanent) grid[i].permanent = true;
}