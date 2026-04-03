#include "util.h"

Cell grid[2][WIDTH * HEIGHT];
Cell prev_grid[2][WIDTH * HEIGHT];
Projectile projectiles[100];

void CompileSigilGraph(Player *p, SpellDNA *draft) {
    draft->temp = 20.0f; draft->density = 0.0f; draft->moisture = 0.0f; 
    draft->cohesion = 0.0f; draft->charge = 0.0f; draft->velocity = (Vector2){0,0};
    
    draft->form = p->selectedForm; 
    draft->movement = MOVE_STRAIGHT; 
    draft->isPermanent = false;
    
    if (p->sigil.nodes[0].active) {
        draft->movement = p->sigil.nodes[0].movement;
    }

    float maxDist = 0.0f;

    for(int i = 0; i < MAX_NODES; i++) {
        if (!p->sigil.nodes[i].active) continue;
        SpellNode n = p->sigil.nodes[i];

        float magnitude = 1.0f;
        Vector2 dir = {0, 0};

        if (n.parentId != -1 && p->sigil.nodes[n.parentId].active) {
            SpellNode parent = p->sigil.nodes[n.parentId];
            float dx = n.pos.x - parent.pos.x;
            float dy = n.pos.y - parent.pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            
            magnitude = (dist / 40.0f) + 1.0f; 
            if (dist > 0) {
                dir.x = dx / dist;
                dir.y = dy / dist;
            }
            if (dist > maxDist) maxDist = dist;
        }

        draft->temp += n.temp * magnitude;
        draft->density += n.density * magnitude;
        draft->moisture += n.moisture * magnitude;
        draft->charge += n.charge * magnitude;
        draft->cohesion = (draft->cohesion == 0) ? n.cohesion : (draft->cohesion + n.cohesion) / 2.0f;

        if (n.parentId != -1) {
            float force = (fabsf(n.density) + fabsf(n.temp)) * magnitude;
            draft->velocity.x += dir.x * force;
            draft->velocity.y += dir.y * force;
        }
    }

    if (draft->temp > 150.0f && draft->charge > 100.0f) draft->form = FORM_BEAM; 
    if (draft->cohesion > 150.0f && draft->density > 100.0f) draft->isPermanent = true;
}

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));
    for(int i=0; i<100; i++) projectiles[i].active = false;

    for (int i = 0; i < 30; i++) {
        int rx = GetRandomValue(10, WIDTH - 10);
        int ry = GetRandomValue(10, HEIGHT - 10);
        int size = GetRandomValue(2, 8);
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int idx = (ry + dy) * WIDTH + (rx + dx);
                if (idx >= 0 && idx < WIDTH * HEIGHT) {
                    if (dx*dx + dy*dy <= size*size) {
                        grid[LAYER_GROUND][idx].density = 100.0f; 
                        grid[LAYER_GROUND][idx].cohesion = 100.0f; 
                        grid[LAYER_GROUND][idx].temp = 20.0f;     
                        grid[LAYER_GROUND][idx].permanent = true; 
                    }
                }
            }
        }
    }
}

void MovePlayer(Player *p, Vector2 delta, float dt) {
    Vector2 nextPos = { p->pos.x + delta.x, p->pos.y + delta.y };
    int nx = (int)(nextPos.x / PIXEL_SIZE);
    int ny = (int)(nextPos.y / PIXEL_SIZE);

    if (p->isJumping) {
        p->z += p->zVelocity * dt;
        p->zVelocity -= 600.0f * dt; 
        if (p->z <= 0.0f) {
            p->z = 0.0f;
            p->zVelocity = 0.0f;
            p->isJumping = false;
            SpellDNA impact = {0, 5.0f, 0, 0, 0, {0,0}, FORM_AURA, MOVE_STRAIGHT, false};
            InjectEnergyArea(nx, ny, LAYER_GROUND, 1, impact);
        }
    }

    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
        if (p->z > 15.0f || grid[LAYER_GROUND][ny * WIDTH + nx].cohesion < 80.0f) p->pos = nextPos; 
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
    p->animTime += dt;

    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        for (int z = 0; z < 2; z++) {
            if (!grid[z][i].permanent) {
                if (grid[z][i].temp > 20.0f) grid[z][i].temp *= 0.99f; 
                if (grid[z][i].temp < 20.0f) grid[z][i].temp += 0.1f; 
                grid[z][i].moisture *= 0.99f;
                grid[z][i].charge *= 0.95f; 
                grid[z][i].velocity.x *= 0.95f;
                grid[z][i].velocity.y *= 0.95f;
            }
            if (grid[z][i].temp > 80.0f && grid[z][i].cohesion > 30.0f) grid[z][i].cohesion -= 0.5f; 
            if (grid[z][i].temp < 0.0f && grid[z][i].cohesion < 90.0f && grid[z][i].moisture > 10.0f) grid[z][i].cohesion += 1.0f; 

            if (fabsf(grid[z][i].velocity.x) > 1.0f || fabsf(grid[z][i].velocity.y) > 1.0f) {
                int nextX = (i % WIDTH) + (grid[z][i].velocity.x > 0 ? 1 : -1);
                int nextY = (i / WIDTH) + (grid[z][i].velocity.y > 0 ? 1 : -1);
                if (nextX >= 0 && nextX < WIDTH && nextY >= 0 && nextY < HEIGHT) {
                    int nextI = nextY * WIDTH + nextX;
                    if (!grid[z][nextI].permanent) {
                        grid[z][nextI].density += grid[z][i].density * 0.1f;
                        grid[z][i].density *= 0.9f;
                    }
                }
            }
        }

        if (grid[LAYER_AIR][i].density > 20.0f && grid[LAYER_AIR][i].cohesion > 20.0f) {
            grid[LAYER_GROUND][i].temp += grid[LAYER_AIR][i].density * 0.5f; 
            grid[LAYER_GROUND][i].density += grid[LAYER_AIR][i].density;
            grid[LAYER_GROUND][i].cohesion = fmax(grid[LAYER_GROUND][i].cohesion, grid[LAYER_AIR][i].cohesion);
            grid[LAYER_AIR][i].density *= 0.1f; 
        }
        
        if (grid[LAYER_GROUND][i].temp > 60.0f && grid[LAYER_GROUND][i].density < 15.0f && grid[LAYER_GROUND][i].cohesion < 10.0f) {
            grid[LAYER_AIR][i].temp += grid[LAYER_GROUND][i].temp * 0.1f;
            grid[LAYER_AIR][i].density += grid[LAYER_GROUND][i].density * 0.1f;
            grid[LAYER_GROUND][i].temp *= 0.9f;
            grid[LAYER_GROUND][i].density *= 0.9f;
        }
        
        if (grid[LAYER_AIR][i].density < 10.0f && grid[LAYER_AIR][i].cohesion < 10.0f) grid[LAYER_AIR][i].density *= 0.95f; 
    }

    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) continue;
        
        projectiles[i].basePos.x += projectiles[i].velocity.x * dt;
        projectiles[i].basePos.y += projectiles[i].velocity.y * dt;
        projectiles[i].life -= dt;
        projectiles[i].animOffset += dt * 15.0f; 

        if (projectiles[i].payload.movement == MOVE_SIN) {
            float len = sqrtf(projectiles[i].velocity.x*projectiles[i].velocity.x + projectiles[i].velocity.y*projectiles[i].velocity.y);
            if (len > 0) {
                float px = -projectiles[i].velocity.y / len; 
                float py = projectiles[i].velocity.x / len;
                float amplitude = 15.0f + (projectiles[i].payload.moisture / 5.0f);
                projectiles[i].pos.x = projectiles[i].basePos.x + px * sinf(projectiles[i].animOffset) * amplitude;
                projectiles[i].pos.y = projectiles[i].basePos.y + py * sinf(projectiles[i].animOffset) * amplitude;
            }
        } else if (projectiles[i].payload.movement == MOVE_COS) {
            float len = sqrtf(projectiles[i].velocity.x*projectiles[i].velocity.x + projectiles[i].velocity.y*projectiles[i].velocity.y);
            if (len > 0) {
                float px = -projectiles[i].velocity.y / len; 
                float py = projectiles[i].velocity.x / len;
                float amplitude = 15.0f + (projectiles[i].payload.moisture / 5.0f);
                projectiles[i].pos.x = projectiles[i].basePos.x + px * cosf(projectiles[i].animOffset) * amplitude;
                projectiles[i].pos.y = projectiles[i].basePos.y + py * cosf(projectiles[i].animOffset) * amplitude;
            }
        } else if (projectiles[i].payload.movement == MOVE_ORBIT) {
            float radius = 40.0f + (projectiles[i].payload.density / 2.0f);
            projectiles[i].pos.x = p->pos.x + cosf(projectiles[i].animOffset) * radius;
            projectiles[i].pos.y = p->pos.y - p->z + sinf(projectiles[i].animOffset) * radius;
            projectiles[i].basePos = projectiles[i].pos; 
        } else {
            projectiles[i].pos = projectiles[i].basePos; 
        }

        int gx = (int)(projectiles[i].pos.x / PIXEL_SIZE);
        int gy = (int)(projectiles[i].pos.y / PIXEL_SIZE);

        bool hitSolid = (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT) ? 
                        (grid[projectiles[i].layer][gy * WIDTH + gx].cohesion > 80.0f) : true;

        if (projectiles[i].life <= 0 || hitSolid) {
            int radius = (int)(projectiles[i].payload.density / 20.0f) + 1;
            InjectEnergyArea(gx, gy, projectiles[i].layer, radius, projectiles[i].payload); 
            projectiles[i].active = false;
        }
    }

    int px = (int)(p->pos.x / PIXEL_SIZE);
    int py = (int)(p->pos.y / PIXEL_SIZE);
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
        if (p->z < 5.0f) {
            Cell g = grid[LAYER_GROUND][py * WIDTH + px];
            if (g.temp > 60.0f) p->health -= (g.temp - 60.0f) * 0.5f * dt;
            if (g.temp < -10.0f) p->health -= fabsf(g.temp + 10.0f) * 0.5f * dt;
            float momentum = g.density * sqrtf(g.velocity.x*g.velocity.x + g.velocity.y*g.velocity.y);
            if (momentum > 150.0f) p->health -= (momentum - 150.0f) * 0.1f * dt;
            if (g.charge > 40.0f) p->health -= g.charge * 0.2f * dt;
        }
    }
}

void ExecuteSpell(Player *p, Vector2 target, SpellDNA dna, float chargeMultiplier) {
    SpellDNA scaledDna = dna;
    scaledDna.temp *= chargeMultiplier;
    scaledDna.density *= chargeMultiplier;
    scaledDna.moisture *= chargeMultiplier;
    scaledDna.charge *= chargeMultiplier;
    scaledDna.velocity.x *= chargeMultiplier;
    scaledDna.velocity.y *= chargeMultiplier;

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
        case FORM_BEAM:
            InjectBeam(p->pos, target, p->castLayer, scaledDna);
            break;
    }
}

void CastProjectile(Vector2 start, Vector2 target, int layer, SpellDNA dna) {
    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) {
            projectiles[i].pos = start;
            projectiles[i].basePos = start;
            projectiles[i].payload = dna;
            projectiles[i].layer = layer;
            projectiles[i].life = (dna.movement == MOVE_ORBIT) ? 10.0f : 1.5f; 
            projectiles[i].active = true;
            projectiles[i].animOffset = 0.0f;
            
            float angle = atan2f(target.y - start.y, target.x - start.x);
            projectiles[i].velocity = (Vector2){ 
                (cosf(angle) * 350.0f) + dna.velocity.x, 
                (sinf(angle) * 350.0f) + dna.velocity.y 
            };
            break;
        }
    }
}

void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna) {
    float dx = target.x - start.x;
    float dy = target.y - start.y;
    float steps = sqrtf(dx*dx + dy*dy) / PIXEL_SIZE;
    if (steps == 0) return;
    
    float xInc = dx / steps;
    float yInc = dy / steps;
    float cx = start.x;
    float cy = start.y;

    for (int i = 0; i <= (int)steps; i++) {
        InjectEnergy((int)(cx / PIXEL_SIZE), (int)(cy / PIXEL_SIZE), z, dna);
        cx += xInc;
        cy += yInc;
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
    grid[z][i].velocity.x += dna.velocity.x;
    grid[z][i].velocity.y += dna.velocity.y;
    if (dna.isPermanent) grid[z][i].permanent = true;
}

void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna) {
    for(int dy = -radius; dy <= radius; dy++) {
        for(int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) InjectEnergy(cx + dx, cy + dy, z, dna);
        }
    }
}