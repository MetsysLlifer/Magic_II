#include "util.h"

Cell grid[2][WIDTH * HEIGHT];
Cell prev_grid[2][WIDTH * HEIGHT];
Projectile projectiles[100];

// NEW: Safely resets the physical world and player vitals without touching the crafted spells in the hotbar
void ResetGame(Player *p) {
    InitSimulation();
    InitNPCs();
    p->pos = (Vector2){WORLD_W / 2.0f, WORLD_H / 2.0f}; 
    p->z = 0.0f;
    p->zVelocity = 0.0f;
    p->health = p->maxHealth;
    p->isJumping = false;
    p->chargeLevel = 0.0f;
    p->isCharging = false;
    p->draggingNodeId = -1;
}

void CompileSigilGraph(SpellDNA *dna) {
    dna->temp = 20.0f; dna->density = 0.0f; dna->moisture = 0.0f; 
    dna->cohesion = 0.0f; dna->charge = 0.0f; dna->velocity = (Vector2){0,0};
    dna->speedMod = 1.0f; dna->delay = 0.0f; dna->distortion = 0.0f;
    dna->rangeMod = 1.0f; dna->sizeMod = 1.0f; dna->spreadType = SPREAD_OFF;
    dna->movement = MOVE_STRAIGHT; dna->isPermanent = false;
    
    if (dna->graph.nodes[0].active) dna->movement = dna->graph.nodes[0].movement;

    float maxDist = 0.0f;
    for(int i = 0; i < MAX_NODES; i++) {
        if (!dna->graph.nodes[i].active) continue;
        SpellNode n = dna->graph.nodes[i];
        float magnitude = 1.0f;
        Vector2 dir = {0, 0};

        if (n.parentId != -1 && dna->graph.nodes[n.parentId].active) {
            SpellNode parent = dna->graph.nodes[n.parentId];
            float dx = n.pos.x - parent.pos.x; float dy = n.pos.y - parent.pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            magnitude = (dist / 40.0f) + 1.0f; 
            if (dist > 0) { dir.x = dx / dist; dir.y = dy / dist; }
            if (dist > maxDist) maxDist = dist;
        }

        dna->temp += n.temp * magnitude; dna->density += n.density * magnitude;
        dna->moisture += n.moisture * magnitude; dna->charge += n.charge * magnitude;
        dna->cohesion = (dna->cohesion == 0) ? n.cohesion : (dna->cohesion + n.cohesion) / 2.0f;

        dna->speedMod *= n.speedMod; dna->delay += n.delay; dna->distortion += n.distortion;
        dna->rangeMod *= n.rangeMod; dna->sizeMod *= n.sizeMod;
        if (n.spreadType > dna->spreadType) dna->spreadType = n.spreadType;

        if (n.parentId != -1) {
            float force = (fabsf(n.density) + fabsf(n.temp)) * magnitude;
            dna->velocity.x += dir.x * force; dna->velocity.y += dir.y * force;
        }
    }

    if (dna->temp > 150.0f && dna->charge > 100.0f) dna->form = FORM_BEAM; 
    if (dna->cohesion > 150.0f && dna->density > 100.0f) dna->isPermanent = true;
}

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));
    for(int i=0; i<100; i++) projectiles[i].active = false;

    for(int y=0; y<HEIGHT; y++) {
        for(int x=0; x<WIDTH; x++) {
            int i = y * WIDTH + x;
            
            if (x < 5 || x >= WIDTH-5 || y < 5 || y >= HEIGHT-5) {
                grid[LAYER_GROUND][i].density = 100.0f; grid[LAYER_GROUND][i].cohesion = 100.0f; 
                grid[LAYER_GROUND][i].permanent = true; continue;
            }

            float noise = sinf(x * 0.05f) + cosf(y * 0.05f) + sinf((x + y) * 0.02f);
            
            if (noise > 1.2f) { 
                grid[LAYER_GROUND][i].density = 80.0f; grid[LAYER_GROUND][i].cohesion = 90.0f; 
                grid[LAYER_GROUND][i].permanent = true; 
            } else if (noise < -1.2f) { 
                grid[LAYER_GROUND][i].moisture = 100.0f; grid[LAYER_GROUND][i].density = 30.0f;
            } else if (noise > 0.4f && noise < 0.8f) { 
                grid[LAYER_GROUND][i].density = 40.0f; grid[LAYER_GROUND][i].moisture = 60.0f;
                grid[LAYER_GROUND][i].cohesion = 60.0f;
            } else { 
                grid[LAYER_GROUND][i].density = 10.0f; grid[LAYER_GROUND][i].temp = 20.0f;
            }
        }
    }
}

void MovePlayer(Player *p, Vector2 delta, float dt) {
    Vector2 nextPos = { p->pos.x + delta.x, p->pos.y + delta.y };
    int nx = (int)(nextPos.x / PIXEL_SIZE);
    int ny = (int)(nextPos.y / PIXEL_SIZE);

    if (p->isJumping) {
        p->z += p->zVelocity * dt; p->zVelocity -= 600.0f * dt; 
        if (p->z <= 0.0f) { p->z = 0.0f; p->zVelocity = 0.0f; p->isJumping = false; }
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
                    float thermalRate = base_rate * 1.5f;
                    grid[z][i].temp = (prev_grid[z][i].temp + thermalRate * (grid[z][i-1].temp + grid[z][i+1].temp + grid[z][i-WIDTH].temp + grid[z][i+WIDTH].temp)) / (1 + 4 * thermalRate);
                    grid[z][i].charge = (prev_grid[z][i].charge + thermalRate * (grid[z][i-1].charge + grid[z][i+1].charge + grid[z][i-WIDTH].charge + grid[z][i+WIDTH].charge)) / (1 + 4 * thermalRate);
                    if (grid[z][i].permanent && grid[z][i].cohesion > 80.0f) continue;
                    
                    float spread = fmax(0.0f, 1.0f - (grid[z][i].cohesion / 100.0f));
                    float rate = base_rate * spread;
                    if (spread > 0.05f) {
                        grid[z][i].density = (prev_grid[z][i].density + rate * (grid[z][i-1].density + grid[z][i+1].density + grid[z][i-WIDTH].density + grid[z][i+WIDTH].density)) / (1 + 4 * rate);
                        grid[z][i].moisture = (prev_grid[z][i].moisture + rate * (grid[z][i-1].moisture + grid[z][i+1].moisture + grid[z][i-WIDTH].moisture + grid[z][i+WIDTH].moisture)) / (1 + 4 * rate);
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
            if (grid[z][i].temp > 20.0f) grid[z][i].temp *= 0.99f; 
            if (grid[z][i].temp < 20.0f) grid[z][i].temp += 0.1f; 
            grid[z][i].charge *= 0.92f; grid[z][i].velocity.x *= 0.95f; grid[z][i].velocity.y *= 0.95f;
            if (!grid[z][i].permanent) grid[z][i].moisture *= 0.99f;
            
            if (grid[z][i].temp > 80.0f && grid[z][i].cohesion > 30.0f) grid[z][i].cohesion -= 0.5f; 
            if (grid[z][i].temp < 0.0f && grid[z][i].cohesion < 90.0f && grid[z][i].moisture > 10.0f) grid[z][i].cohesion += 1.0f; 

            if (fabsf(grid[z][i].velocity.x) > 1.0f || fabsf(grid[z][i].velocity.y) > 1.0f) {
                int nextX = (i % WIDTH) + (grid[z][i].velocity.x > 0 ? 1 : -1);
                int nextY = (i / WIDTH) + (grid[z][i].velocity.y > 0 ? 1 : -1);
                if (nextX >= 0 && nextX < WIDTH && nextY >= 0 && nextY < HEIGHT) {
                    int nextI = nextY * WIDTH + nextX;
                    if (!grid[z][nextI].permanent) {
                        float advect = 0.15f; 
                        grid[z][nextI].density += grid[z][i].density * advect; grid[z][i].density *= (1.0f - advect);
                        float heatDiff = grid[z][i].temp - 20.0f;
                        if (heatDiff > 0) { grid[z][nextI].temp += heatDiff * advect; grid[z][i].temp -= heatDiff * advect; }
                        grid[z][nextI].charge += grid[z][i].charge * advect; grid[z][i].charge *= (1.0f - advect);
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
            grid[LAYER_GROUND][i].temp *= 0.9f; grid[LAYER_GROUND][i].density *= 0.9f;
        }
        if (grid[LAYER_AIR][i].density < 10.0f && grid[LAYER_AIR][i].cohesion < 10.0f) grid[LAYER_AIR][i].density *= 0.95f; 
    }

    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) continue;
        
        if (projectiles[i].currentDelay > 0.0f) { projectiles[i].currentDelay -= dt; continue; }
        
        projectiles[i].basePos.x += projectiles[i].velocity.x * dt;
        projectiles[i].basePos.y += projectiles[i].velocity.y * dt;
        projectiles[i].life -= dt; projectiles[i].animOffset += dt * 15.0f; 

        if (projectiles[i].payload.movement == MOVE_SIN) {
            float len = sqrtf(projectiles[i].velocity.x*projectiles[i].velocity.x + projectiles[i].velocity.y*projectiles[i].velocity.y);
            if (len > 0) {
                float px = -projectiles[i].velocity.y / len; float py = projectiles[i].velocity.x / len;
                float amplitude = 15.0f + (projectiles[i].payload.moisture / 5.0f);
                projectiles[i].pos.x = projectiles[i].basePos.x + px * sinf(projectiles[i].animOffset) * amplitude;
                projectiles[i].pos.y = projectiles[i].basePos.y + py * sinf(projectiles[i].animOffset) * amplitude;
            }
        } else if (projectiles[i].payload.movement == MOVE_COS) {
            float len = sqrtf(projectiles[i].velocity.x*projectiles[i].velocity.x + projectiles[i].velocity.y*projectiles[i].velocity.y);
            if (len > 0) {
                float px = -projectiles[i].velocity.y / len; float py = projectiles[i].velocity.x / len;
                float amplitude = 15.0f + (projectiles[i].payload.moisture / 5.0f);
                projectiles[i].pos.x = projectiles[i].basePos.x + px * cosf(projectiles[i].animOffset) * amplitude;
                projectiles[i].pos.y = projectiles[i].basePos.y + py * cosf(projectiles[i].animOffset) * amplitude;
            }
        } else if (projectiles[i].payload.movement == MOVE_ORBIT) {
            float radius = 40.0f + (projectiles[i].payload.density / 2.0f);
            projectiles[i].pos.x = p->pos.x + cosf(projectiles[i].animOffset) * radius;
            projectiles[i].pos.y = p->pos.y - p->z + sinf(projectiles[i].animOffset) * radius;
            projectiles[i].basePos = projectiles[i].pos; 
        } else { projectiles[i].pos = projectiles[i].basePos; }

        int gx = (int)(projectiles[i].pos.x / PIXEL_SIZE);
        int gy = (int)(projectiles[i].pos.y / PIXEL_SIZE);
        bool hitSolid = (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT) ? (grid[projectiles[i].layer][gy * WIDTH + gx].cohesion > 80.0f) : true;

        if (!hitSolid) {
            for (int j = 0; j < 50; j++) {
                if (active_npcs[j].active) {
                    float dx = projectiles[i].pos.x - active_npcs[j].pos.x;
                    float dy = projectiles[i].pos.y - active_npcs[j].pos.y;
                    if (dx*dx + dy*dy < 400.0f) { 
                        hitSolid = true; 
                        active_npcs[j].health -= projectiles[i].payload.density + projectiles[i].payload.temp; 
                        break; 
                    }
                }
            }
            if (!hitSolid && p->friendlyFire && (projectiles[i].maxLife - projectiles[i].life > 0.1f)) {
                float dx = projectiles[i].pos.x - p->pos.x;
                float dy = projectiles[i].pos.y - p->pos.y;
                if (dx*dx + dy*dy < 200.0f) {
                    hitSolid = true;
                    p->health -= projectiles[i].payload.density + projectiles[i].payload.temp; 
                }
            }
        }

        if (projectiles[i].life <= 0 || hitSolid) {
            if (projectiles[i].payload.spreadType == SPREAD_COLLISION) {
                SpellDNA childDna = projectiles[i].payload;
                childDna.spreadType = SPREAD_OFF; childDna.sizeMod *= 0.5f; childDna.delay = 0.0f;
                for(int k=0; k<3; k++) {
                    Vector2 randTarget = { projectiles[i].pos.x + GetRandomValue(-100,100), projectiles[i].pos.y + GetRandomValue(-100,100) };
                    CastProjectile(projectiles[i].pos, randTarget, projectiles[i].layer, childDna);
                }
            }

            int radius = (int)(projectiles[i].payload.density / 20.0f) * projectiles[i].payload.sizeMod + 1;
            InjectEnergyArea(gx, gy, projectiles[i].layer, radius, projectiles[i].payload); 
            projectiles[i].active = false;
        }
    }

    int px = (int)(p->pos.x / PIXEL_SIZE);
    int py = (int)(p->pos.y / PIXEL_SIZE);
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT && p->z < 5.0f) {
        Cell g = grid[LAYER_GROUND][py * WIDTH + px];
        if (g.temp > 60.0f) p->health -= (g.temp - 60.0f) * 0.5f * dt;
        if (g.temp < -10.0f) p->health -= fabsf(g.temp + 10.0f) * 0.5f * dt;
        float momentum = g.density * sqrtf(g.velocity.x*g.velocity.x + g.velocity.y*g.velocity.y);
        if (momentum > 150.0f) p->health -= (momentum - 150.0f) * 0.1f * dt;
        if (g.charge > 40.0f) p->health -= g.charge * 0.2f * dt;
    }
}

void ExecuteSpell(Player *p, Vector2 target, SpellDNA dna, float chargeMultiplier) {
    SpellDNA scaledDna = dna;
    scaledDna.temp *= chargeMultiplier; scaledDna.density *= chargeMultiplier;
    scaledDna.moisture *= chargeMultiplier; scaledDna.charge *= chargeMultiplier;
    scaledDna.velocity.x *= chargeMultiplier; scaledDna.velocity.y *= chargeMultiplier;
    scaledDna.sizeMod *= chargeMultiplier; scaledDna.rangeMod *= chargeMultiplier;

    int gx = (int)(target.x / PIXEL_SIZE); int gy = (int)(target.y / PIXEL_SIZE);
    int px = (int)(p->pos.x / PIXEL_SIZE); int py = (int)(p->pos.y / PIXEL_SIZE);
    int radius = (int)(1 + chargeMultiplier) * scaledDna.sizeMod;

    switch(dna.form) {
        case FORM_PROJECTILE:
            if (scaledDna.spreadType == SPREAD_INSTANT) {
                float angleBase = atan2f(target.y - p->pos.y, target.x - p->pos.x);
                for (int offset = -15; offset <= 15; offset += 15) {
                    float spreadAngle = angleBase + (offset * PI / 180.0f);
                    Vector2 spreadTarget = { p->pos.x + cosf(spreadAngle)*100.0f, p->pos.y + sinf(spreadAngle)*100.0f };
                    CastProjectile(p->pos, spreadTarget, p->castLayer, scaledDna);
                }
            } else CastProjectile(p->pos, target, p->castLayer, scaledDna);
            break;
        case FORM_MANIFEST: InjectEnergyArea(gx, gy, p->castLayer, radius + 1, scaledDna); break;
        case FORM_AURA: InjectEnergyArea(px, py, p->castLayer, radius + 3, scaledDna); break;
        case FORM_BEAM: InjectBeam(p->pos, target, p->castLayer, scaledDna); break;
    }
}

void CastProjectile(Vector2 start, Vector2 target, int layer, SpellDNA dna) {
    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) {
            projectiles[i].pos = start;
            projectiles[i].basePos = start;
            projectiles[i].payload = dna;
            projectiles[i].layer = layer;
            
            float baseLife = (dna.movement == MOVE_ORBIT) ? 10.0f : 1.5f;
            projectiles[i].life = baseLife * dna.rangeMod; 
            projectiles[i].maxLife = projectiles[i].life; 
            projectiles[i].currentDelay = dna.delay; 
            projectiles[i].active = true;
            projectiles[i].animOffset = 0.0f;
            
            float angle = atan2f(target.y - start.y, target.x - start.x);
            float cosA = cosf(angle); float sinA = sinf(angle);
            float localVx = dna.velocity.x; float localVy = dna.velocity.y;
            float rotatedVx = localVx * cosA - localVy * sinA;
            float rotatedVy = localVx * sinA + localVy * cosA;

            projectiles[i].velocity = (Vector2){ 
                ((cosA * 350.0f) + rotatedVx) * dna.speedMod, 
                ((sinA * 350.0f) + rotatedVy) * dna.speedMod 
            };
            break;
        }
    }
}

void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna) {
    float dx = target.x - start.x; float dy = target.y - start.y;
    float dist = sqrtf(dx*dx + dy*dy) * dna.rangeMod;
    float steps = dist / PIXEL_SIZE;
    if (steps == 0) return;
    
    float xInc = (dx / dist) * PIXEL_SIZE; float yInc = (dy / dist) * PIXEL_SIZE;
    float cx = start.x; float cy = start.y;
    for (int i = 0; i <= (int)steps; i++) {
        InjectEnergy((int)(cx / PIXEL_SIZE), (int)(cy / PIXEL_SIZE), z, dna);
        cx += xInc; cy += yInc;
    }
}

void InjectEnergy(int x, int y, int z, SpellDNA dna) {
    if (x < 1 || x >= WIDTH - 1 || y < 1 || y >= HEIGHT - 1) return;
    int i = y * WIDTH + x;
    grid[z][i].temp += dna.temp; grid[z][i].density += dna.density;
    grid[z][i].moisture += dna.moisture; grid[z][i].cohesion = (grid[z][i].cohesion + dna.cohesion) / 2.0f; 
    grid[z][i].charge += dna.charge; grid[z][i].velocity.x += dna.velocity.x; grid[z][i].velocity.y += dna.velocity.y;
    if (dna.isPermanent) grid[z][i].permanent = true;
}

void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna) {
    for(int dy = -radius; dy <= radius; dy++) {
        for(int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) InjectEnergy(cx + dx, cy + dy, z, dna);
        }
    }
}