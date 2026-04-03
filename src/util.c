#include "util.h"

Cell grid[WIDTH * HEIGHT];
Cell prev_grid[WIDTH * HEIGHT];

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));
}

void Diffuse(float dt) {
    float rate = 0.08f * dt * WIDTH * HEIGHT; 
    for (int k = 0; k < 10; k++) {
        for (int y = 1; y < HEIGHT - 1; y++) {
            for (int x = 1; x < WIDTH - 1; x++) {
                int i = y * WIDTH + x;
                grid[i].temp = (prev_grid[i].temp + rate * (grid[i-1].temp + grid[i+1].temp + grid[i-WIDTH].temp + grid[i+WIDTH].temp)) / (1 + 4 * rate);
                grid[i].moisture = (prev_grid[i].moisture + rate * (grid[i-1].moisture + grid[i+1].moisture + grid[i-WIDTH].moisture + grid[i+WIDTH].moisture)) / (1 + 4 * rate);
                grid[i].density = (prev_grid[i].density + rate * (grid[i-1].density + grid[i+1].density + grid[i-WIDTH].density + grid[i+WIDTH].density)) / (1 + 4 * rate);
            }
        }
    }
}

void UpdateSimulation(float dt) {
    memcpy(prev_grid, grid, sizeof(grid));
    Diffuse(dt);
    
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        grid[i].temp *= 0.98f;
        grid[i].moisture *= 0.99f;
        grid[i].density *= 0.995f; 
        grid[i].velocity.x *= 0.95f;
        grid[i].velocity.y *= 0.95f;
    }
}

void InjectEnergy(int x, int y, Cell energy) {
    if (x < 1 || x >= WIDTH - 1 || y < 1 || y >= HEIGHT - 1) return;
    int i = y * WIDTH + x;
    grid[i].temp += energy.temp;
    grid[i].density += energy.density;
    grid[i].moisture += energy.moisture;
    grid[i].velocity.x += energy.velocity.x;
    grid[i].velocity.y += energy.velocity.y;
}