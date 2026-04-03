#include "util.h"

Cell grid[WIDTH * HEIGHT];
Cell prev_grid[WIDTH * HEIGHT];

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));

    // Spawn random Natural Objects (Rocks)
    for (int i = 0; i < 20; i++) {
        int rx = GetRandomValue(10, WIDTH - 10);
        int ry = GetRandomValue(10, HEIGHT - 10);
        int size = GetRandomValue(2, 6);
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int idx = (ry + dy) * WIDTH + (rx + dx);
                if (idx >= 0 && idx < WIDTH * HEIGHT) {
                    grid[idx].density = 80.0f;
                    grid[idx].cohesion = 90.0f; // High cohesion makes it solid
                    grid[idx].temp = 20.0f;     // Room temp
                    grid[idx].permanent = true; 
                }
            }
        }
    }
}

void Diffuse(float dt) {
    float base_rate = 0.1f * dt * WIDTH * HEIGHT;
    
    for (int k = 0; k < 5; k++) {
        for (int y = 1; y < HEIGHT - 1; y++) {
            for (int x = 1; x < WIDTH - 1; x++) {
                int i = y * WIDTH + x;
                if (grid[i].permanent && grid[i].cohesion > 80.0f) continue; // Pure solids don't leak mass

                // Cohesion limits spreading. If Cohesion is 100, spread_factor is 0.
                float spread_factor = fmax(0.0f, 1.0f - (grid[i].cohesion / 100.0f));
                float rate = base_rate * spread_factor;

                grid[i].temp = (prev_grid[i].temp + rate * (grid[i-1].temp + grid[i+1].temp + grid[i-WIDTH].temp + grid[i+WIDTH].temp)) / (1 + 4 * rate);
                
                // Only spread density if it's acting like a liquid or gas
                if (spread_factor > 0.1f) {
                    grid[i].density = (prev_grid[i].density + rate * (grid[i-1].density + grid[i+1].density + grid[i-WIDTH].density + grid[i+WIDTH].density)) / (1 + 4 * rate);
                }
            }
        }
    }
}

void UpdateSimulation(float dt) {
    memcpy(prev_grid, grid, sizeof(grid));
    Diffuse(dt);
    
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        // TOP-DOWN GRAVITY / DISSIPATION LOGIC
        if (grid[i].density < 20.0f && grid[i].cohesion < 20.0f) {
            // It's a GAS: It dissipates into the Z-axis (disappears) rapidly
            grid[i].density *= 0.95f; 
        }

        if (!grid[i].permanent) {
            // Ambient cooling/drying
            if (grid[i].temp > 20.0f) grid[i].temp *= 0.99f; 
            grid[i].moisture *= 0.99f;
        }

        // PHASE TRANSITIONS (Math altering scalars organically)
        // Extreme heat breaks down cohesion (Melting)
        if (grid[i].temp > 80.0f && grid[i].cohesion > 30.0f) {
            grid[i].cohesion -= 0.5f; 
        }
        // Extreme cold increases cohesion (Freezing)
        if (grid[i].temp < 0.0f && grid[i].cohesion < 90.0f && grid[i].moisture > 10.0f) {
            grid[i].cohesion += 1.0f;
        }
    }
}

void InjectEnergy(int x, int y, SpellDNA dna) {
    if (x < 1 || x >= WIDTH - 1 || y < 1 || y >= HEIGHT - 1) return;
    int i = y * WIDTH + x;
    grid[i].temp += dna.temp;
    grid[i].density += dna.density;
    grid[i].moisture += dna.moisture;
    grid[i].cohesion = (grid[i].cohesion + dna.cohesion) / 2.0f; // Average out the structural integrity
    grid[i].charge += dna.charge;
    grid[i].velocity.x += dna.velocity.x;
    grid[i].velocity.y += dna.velocity.y;
    if (dna.isPermanent) grid[i].permanent = true;
}