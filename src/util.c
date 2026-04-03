#include "util.h"

Cell grid[2][WIDTH * HEIGHT];
Cell prev_grid[2][WIDTH * HEIGHT];
Projectile projectiles[100];

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));
    for(int i=0; i<100; i++) projectiles[i].active = false;

    // Generate Natural Walls (High density, high cohesion)
    for (int i = 0; i < 30; i++) {
        int rx = GetRandomValue(10, WIDTH - 10);
        int ry = GetRandomValue(10, HEIGHT - 10);
        int size = GetRandomValue(2, 8);
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int idx = (ry + dy) * WIDTH + (rx + dx);
                if (idx >= 0 && idx < WIDTH * HEIGHT) {
                    // Create irregular cave-like shapes
                    if (dx*dx + dy*dy <= size*size) {
                        grid[LAYER_GROUND][idx].density = 100.0f; // Very dense wall
                        grid[LAYER_GROUND][idx].cohesion = 100.0f; // Solid
                        grid[LAYER_GROUND][idx].temp = 20.0f;     
                        grid[LAYER_GROUND][idx].permanent = true; 
                    }
                }
            }
        }
    }
}

void MovePlayer(Player *p, Vector2 delta) {
    Vector2 nextPos = { p->pos.x + delta.x, p->pos.y + delta.y };
    int nx = (int)(nextPos.x / PIXEL_SIZE);
    int ny = (int)(nextPos.y / PIXEL_SIZE);

    // COLLISION DETECTION: Check if target cell is solid
    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
        if (grid[LAYER_GROUND][ny * WIDTH + nx].cohesion < 80.0f) {
            p->pos = nextPos; // Move only if not a wall
        }
    }
}

void Diffuse(float dt) {
    float base_rate = 0.1f * dt * WIDTH * HEIGHT;
    for (int z = 0; z < 2; z++) { 
        for (int k = 0; k < 5; k++) {
            for (int y = 1; y < HEIGHT - 1; y++) {
                for (int x = 1; x < WIDTH - 1; x++) {
                    int i = y * WIDTH + x;
                    if (grid[z][i].permanent && grid[z][i].cohesion > 80.0f) continue;
                    float spread = fmax(0.0f, 1.0f - (grid[z][i].cohesion / 100.0f));
                    float rate = base_rate * spread;
                    grid[z][i].temp = (prev_grid[z][i].temp + rate * (grid[z][i-1].temp + grid[z][i+1].temp + grid[z][i-WIDTH].temp + grid[z][i+WIDTH].temp)) / (1 + 4 * rate);
                    if (spread > 0.1f) {
                        grid[z][i].density = (prev_grid[z][i].density + rate * (grid[z][i-1].density + grid[z][i+1].density + grid[z][i-WIDTH].density + grid[z][i+WIDTH].density)) / (1 + 4 * rate);
                    }
                }
            }
        }
    }
}

void UpdateSimulation(float dt, Player *p) {
    memcpy(prev_grid, grid, sizeof(grid));
    Diffuse(dt);
    
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        for (int z = 0; z < 2; z++) {
            if (!grid[z][i].permanent) {
                if (grid[z][i].temp > 20.0f) grid[z][i].temp *= 0.99f; 
                if (grid[z][i].temp < 20.0f) grid[z][i].temp += 0.1f; 
                grid[z][i].moisture *= 0.99f;
                grid[z][i].charge *= 0.95f; 
            }
            if (grid[z][i].temp > 80.0f && grid[z][i].cohesion > 30.0f) grid[z][i].cohesion -= 0.5f; // Melt
            if (grid[z][i].temp < 0.0f && grid[z][i].cohesion < 90.0f && grid[z][i].moisture > 10.0f) grid[z][i].cohesion += 1.0f; // Freeze
        }

        // AIR TO GROUND INTERACTION (Kinetic Crash)
        if (grid[LAYER_AIR][i].density > 20.0f && grid[LAYER_AIR][i].cohesion > 20.0f) {
            // Massive objects falling generate heat on impact
            grid[LAYER_GROUND][i].temp += grid[LAYER_AIR][i].density * 0.5f; 
            grid[LAYER_GROUND][i].density += grid[LAYER_AIR][i].density;
            grid[LAYER_GROUND][i].cohesion = fmax(grid[LAYER_GROUND][i].cohesion, grid[LAYER_AIR][i].cohesion);
            grid[LAYER_AIR][i].density *= 0.1f; 
        }
        
        // GROUND TO AIR (Buoyancy)
        if (grid[LAYER_GROUND][i].temp > 60.0f && grid[LAYER_GROUND][i].density < 15.0f && grid[LAYER_GROUND][i].cohesion < 10.0f) {
            grid[LAYER_AIR][i].temp += grid[LAYER_GROUND][i].temp * 0.1f;
            grid[LAYER_AIR][i].density += grid[LAYER_GROUND][i].density * 0.1f;
            grid[LAYER_GROUND][i].temp *= 0.9f;
            grid[LAYER_GROUND][i].density *= 0.9f;
        }
        
        if (grid[LAYER_AIR][i].density < 10.0f && grid[LAYER_AIR][i].cohesion < 10.0f) grid[LAYER_AIR][i].density *= 0.95f; // Dissipate
    }

    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) continue;
        projectiles[i].pos.x += projectiles[i].velocity.x * dt;
        projectiles[i].pos.y += projectiles[i].velocity.y * dt;
        projectiles[i].life -= dt;

        int gx = (int)(projectiles[i].pos.x / PIXEL_SIZE);
        int gy = (int)(projectiles[i].pos.y / PIXEL_SIZE);

        bool hitSolid = (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT) ? 
                        (grid[projectiles[i].layer][gy * WIDTH + gx].cohesion > 80.0f) : true;

        if (projectiles[i].life <= 0 || hitSolid) {
            // Projectiles burst wider based on their density payload
            int radius = (int)(projectiles[i].payload.density / 20.0f) + 1;
            InjectEnergyArea(gx, gy, projectiles[i].layer, radius, projectiles[i].payload); 
            projectiles[i].active = false;
        }
    }

    int px = (int)(p->pos.x / PIXEL_SIZE);
    int py = (int)(p->pos.y / PIXEL_SIZE);
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
        Cell g = grid[LAYER_GROUND][py * WIDTH + px];
        if (g.temp > 60.0f) p->health -= (g.temp - 60.0f) * 0.5f * dt;
        if (g.temp < -10.0f) p->health -= fabsf(g.temp + 10.0f) * 0.5f * dt;
        float momentum = g.density * sqrtf(g.velocity.x*g.velocity.x + g.velocity.y*g.velocity.y);
        if (momentum > 150.0f) p->health -= (momentum - 150.0f) * 0.1f * dt;
        if (g.charge > 40.0f) p->health -= g.charge * 0.2f * dt;
    }
}

// Applies the Charge Multiplier to the DNA before casting
void ExecuteSpell(Player *p, Vector2 target, SpellDNA dna, float chargeMultiplier) {
    SpellDNA scaledDna = dna;
    scaledDna.temp *= chargeMultiplier;
    scaledDna.density *= chargeMultiplier;
    scaledDna.moisture *= chargeMultiplier;
    scaledDna.charge *= chargeMultiplier;
    // Cohesion is structural, it doesn't scale with charge.

    int gx = (int)(target.x / PIXEL_SIZE);
    int gy = (int)(target.y / PIXEL_SIZE);
    int px = (int)(p->pos.x / PIXEL_SIZE);
    int py = (int)(p->pos.y / PIXEL_SIZE);

    int radius = (int)(1 + chargeMultiplier);

    switch(dna.form) {
        case FORM_PROJECTILE:
            CastProjectile(p->pos, target, p->castLayer, scaledDna);
            break;
        case FORM_MANIFEST:
            InjectEnergyArea(gx, gy, p->castLayer, radius + 1, scaledDna); 
            break;
        case FORM_AURA:
            InjectEnergyArea(px, py, p->castLayer, radius + 3, scaledDna); 
            break;
    }
}

void CastProjectile(Vector2 start, Vector2 target, int layer, SpellDNA dna) {
    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) {
            projectiles[i].pos = start;
            projectiles[i].payload = dna;
            projectiles[i].layer = layer;
            projectiles[i].life = 1.5f; 
            projectiles[i].active = true;
            float angle = atan2f(target.y - start.y, target.x - start.x);
            projectiles[i].velocity = (Vector2){ cosf(angle) * 350.0f, sinf(angle) * 350.0f };
            break;
        }
    }
}

void InjectEnergy(int x, int y, int z, SpellDNA dna) {
    if (x < 1 || x >= WIDTH - 1 || y < 1 || y >= HEIGHT - 1) return;
    int i = y * WIDTH + x;
    grid[z][i].temp += dna.temp;
    grid[z][i].density += dna.density;
    grid[z][i].moisture += dna.moisture;
    grid[z][i].cohesion = (grid[z][i].cohesion + dna.cohesion) / 2.0f; 
    grid[z][i].charge += dna.charge;
    if (dna.isPermanent) grid[z][i].permanent = true;
}

void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna) {
    for(int dy = -radius; dy <= radius; dy++) {
        for(int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) { 
                InjectEnergy(cx + dx, cy + dy, z, dna);
            }
        }
    }
}