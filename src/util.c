#include "util.h"

Cell grid[2][WIDTH * HEIGHT];
Cell prev_grid[2][WIDTH * HEIGHT];
Projectile projectiles[100];
Singularity sys_singularities[20];
int sys_sigCount = 0;

void ResetGame(Player *p) {
    InitSimulation();
    InitNPCs();
    p->pos = (Vector2){WORLD_W / 2.0f, WORLD_H / 2.0f}; 
    p->z = 0.0f; p->zVelocity = 0.0f; p->health = p->maxHealth;
    p->isJumping = false; p->chargeLevel = 0.0f; p->lifespanLevel = 0.0f;
    p->isCharging = false; p->isLifespanCharging = false; p->draggingNodeId = -1;
    for(int i=0; i<10; i++) p->editStates[i] = false;
    sys_sigCount = 0;
}

bool IsDescendant(SigilGraph *g, int child, int root) {
    if(child == root) return true;
    int curr = g->nodes[child].parentId;
    int depth = 0; 
    while(curr >= 0 && curr < MAX_NODES && depth < MAX_NODES) {
        if(curr == root) return true;
        curr = g->nodes[curr].parentId;
        depth++;
    }
    return false;
}

float GetNodeMagnitude(SigilGraph *g, int child, int root, Vector2 *outDir, float *resonance) {
    if(child == root) { *outDir = (Vector2){0,0}; *resonance = 1.0f; return 5.0f; }
    
    int pId = g->nodes[child].parentId;
    if(pId < 0 || pId >= MAX_NODES) { *outDir = (Vector2){0,0}; *resonance = 1.0f; return 1.0f; }

    Vector2 p1 = g->nodes[child].pos; Vector2 p2 = g->nodes[pId].pos;
    float dx = p1.x - p2.x; float dy = p1.y - p2.y;
    float dist = sqrtf(dx*dx + dy*dy);
    
    if(dist > 0) { outDir->x = dx/dist; outDir->y = dy/dist; }
    else { outDir->x = 0; outDir->y = 0; }
    
    float phase = dist * 0.1f + g->nodes[child].charge * 0.05f;
    *resonance = 1.0f + 0.8f * sinf(phase); 
    return (dist / 40.0f) + 1.0f;
}

void CompileSigilGraph(SpellDNA *dna) {
    dna->temp = 20.0f; dna->density = 0.0f; dna->moisture = 0.0f; 
    dna->cohesion = 0.0f; dna->charge = 0.0f; dna->velocity = (Vector2){0,0};
    dna->movement = MOVE_STRAIGHT; 
    
    if (dna->graph.nodes[0].active) dna->movement = dna->graph.nodes[0].movement;

    for(int i = 0; i < MAX_NODES; i++) {
        if (!dna->graph.nodes[i].active) continue;
        SpellNode n = dna->graph.nodes[i];
        
        Vector2 dir = {0, 0}; float resonance = 1.0f; float magnitude = 1.0f;

        if (n.parentId >= 0 && n.parentId < MAX_NODES && dna->graph.nodes[n.parentId].active) {
            magnitude = GetNodeMagnitude(&dna->graph, i, 0, &dir, &resonance);
        }

        dna->temp += n.temp * magnitude * resonance; 
        dna->density += n.density * magnitude * resonance;
        dna->moisture += n.moisture * magnitude * resonance; 
        dna->charge += n.charge * magnitude * resonance;
        dna->cohesion = (dna->cohesion == 0) ? n.cohesion : (dna->cohesion + n.cohesion) / 2.0f;

        if (n.parentId >= 0 && n.parentId < MAX_NODES) {
            float force = (fabsf(n.density) + fabsf(n.temp)) * magnitude * resonance;
            dna->velocity.x += dir.x * force; dna->velocity.y += dir.y * force;
        }
    }
}

void InitSimulation() {
    memset(grid, 0, sizeof(grid)); memset(prev_grid, 0, sizeof(prev_grid));
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
                grid[LAYER_GROUND][i].density = 80.0f; grid[LAYER_GROUND][i].cohesion = 90.0f; grid[LAYER_GROUND][i].permanent = true; 
            } else if (noise < -1.2f) { grid[LAYER_GROUND][i].moisture = 100.0f; grid[LAYER_GROUND][i].density = 30.0f;
            } else if (noise > 0.4f && noise < 0.8f) { grid[LAYER_GROUND][i].density = 40.0f; grid[LAYER_GROUND][i].moisture = 60.0f; grid[LAYER_GROUND][i].cohesion = 60.0f;
            } else { grid[LAYER_GROUND][i].density = 10.0f; grid[LAYER_GROUND][i].temp = 20.0f; }
        }
    }
}

void MovePlayer(Player *p, Vector2 delta, float dt) {
    Vector2 nextPos = { p->pos.x + delta.x, p->pos.y + delta.y };
    int nx = (int)(nextPos.x / PIXEL_SIZE); int ny = (int)(nextPos.y / PIXEL_SIZE);

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
                    
                    if (grid[z][i].permanent) continue;
                    
                    float spread = fmax(0.0f, 1.0f - (fabsf(grid[z][i].cohesion) / 100.0f));
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

    // --- TRUE QUANTUM SINGULARITY SCAN ---
    sys_sigCount = 0;
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        // High density threshold requires active resonance to achieve!
        if (grid[LAYER_GROUND][i].density > 300.0f) {
            if (sys_sigCount < 20) {
                if (grid[LAYER_GROUND][i].cohesion > 150.0f) {
                    sys_singularities[sys_sigCount++] = (Singularity){i, {(i % WIDTH) * PIXEL_SIZE, (i / WIDTH) * PIXEL_SIZE}, grid[LAYER_GROUND][i].density, grid[LAYER_GROUND][i].charge, 1, -1, p->animTime};
                } else if (grid[LAYER_GROUND][i].cohesion < -150.0f) {
                    sys_singularities[sys_sigCount++] = (Singularity){i, {(i % WIDTH) * PIXEL_SIZE, (i / WIDTH) * PIXEL_SIZE}, grid[LAYER_GROUND][i].density, grid[LAYER_GROUND][i].charge, 2, -1, p->animTime};
                }
            }
        }
    }

    // RESOLVE ENTANGLEMENT: Frequencies must match within 5 units!
    for(int i=0; i<sys_sigCount; i++) {
        if(sys_singularities[i].type == 1) { // Black Hole
            for(int j=0; j<sys_sigCount; j++) {
                if(sys_singularities[j].type == 2) { // White Hole
                    if(fabsf(sys_singularities[i].charge - sys_singularities[j].charge) < 5.0f) {
                        sys_singularities[i].linkedTo = j;
                        sys_singularities[j].linkedTo = i;
                        break;
                    }
                }
            }
        }
    }

    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        for (int z = 0; z < 2; z++) {
            if (grid[z][i].temp > 20.0f) grid[z][i].temp *= 0.99f; 
            if (grid[z][i].temp < 20.0f) grid[z][i].temp += 0.1f; 
            grid[z][i].charge *= 0.92f; grid[z][i].velocity.x *= 0.95f; grid[z][i].velocity.y *= 0.95f;
            
            if (!grid[z][i].permanent) {
                grid[z][i].moisture *= 0.99f;
                if (grid[z][i].temp > 80.0f && grid[z][i].cohesion > 30.0f) grid[z][i].cohesion -= 0.5f; 
                if (grid[z][i].temp < 0.0f && grid[z][i].cohesion < 90.0f && grid[z][i].moisture > 10.0f) grid[z][i].cohesion += 1.0f; 
                
                // Hawking Radiation: Singularities evaporate to prevent permanent world-locks
                if (grid[z][i].density > 300.0f) grid[z][i].density -= 10.0f * dt;
            }

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
            grid[LAYER_GROUND][i].temp += grid[LAYER_AIR][i].density * 0.5f; grid[LAYER_GROUND][i].density += grid[LAYER_AIR][i].density;
            grid[LAYER_GROUND][i].cohesion = fmax(grid[LAYER_GROUND][i].cohesion, grid[LAYER_AIR][i].cohesion); grid[LAYER_AIR][i].density *= 0.1f; 
        }
        if (grid[LAYER_GROUND][i].temp > 60.0f && grid[LAYER_GROUND][i].density < 15.0f && grid[LAYER_GROUND][i].cohesion < 10.0f) {
            grid[LAYER_AIR][i].temp += grid[LAYER_GROUND][i].temp * 0.1f; grid[LAYER_AIR][i].density += grid[LAYER_GROUND][i].density * 0.1f;
            grid[LAYER_GROUND][i].temp *= 0.9f; grid[LAYER_GROUND][i].density *= 0.9f;
        }
        if (grid[LAYER_AIR][i].density < 10.0f && grid[LAYER_AIR][i].cohesion < 10.0f) grid[LAYER_AIR][i].density *= 0.95f; 
    }

    // SPATIAL BENDING & TELEPORTATION
    for(int s=0; s<sys_sigCount; s++) {
        Singularity sig = sys_singularities[s];
        float forceMult = (sig.type == 2) ? -3500.0f : 3500.0f; // Push or Pull
        int targetWH = sig.linkedTo;

        float dx = p->pos.x - sig.pos.x; float dy = p->pos.y - sig.pos.y;
        float distSq = dx*dx + dy*dy;
        if (distSq > 0.1f && distSq < 250000.0f) { 
            float dist = sqrtf(distSq);
            float f = forceMult / (distSq * 0.02f + 1.0f);
            p->pos.x -= (dx/dist) * f * dt; p->pos.y -= (dy/dist) * f * dt;
            
            // Event Horizon
            if (sig.type == 1 && dist < 15.0f) {
                if (targetWH != -1) {
                    p->pos = sys_singularities[targetWH].pos; // TELEPORT!
                } else {
                    p->health -= 1000.0f; // Crushed
                    grid[LAYER_GROUND][sig.index].density += 50.0f; 
                }
            }
        }
        
        for(int n=0; n<50; n++) {
            if(!active_npcs[n].active) continue;
            float ndx = active_npcs[n].pos.x - sig.pos.x; float ndy = active_npcs[n].pos.y - sig.pos.y;
            float ndistSq = ndx*ndx + ndy*ndy;
            if(ndistSq > 0.1f && ndistSq < 250000.0f) {
                float ndist = sqrtf(ndistSq);
                float f = forceMult / (ndistSq * 0.02f + 1.0f);
                active_npcs[n].pos.x -= (ndx/ndist) * f * dt; active_npcs[n].pos.y -= (ndy/ndist) * f * dt;
                
                if (sig.type == 1 && ndist < 15.0f) {
                    if (targetWH != -1) {
                        active_npcs[n].pos = sys_singularities[targetWH].pos;
                    } else {
                        active_npcs[n].health -= 1000.0f; 
                        grid[LAYER_GROUND][sig.index].density += active_npcs[n].dna.mass; 
                    }
                }
            }
        }
        
        for(int pr=0; pr<100; pr++) {
            if(!projectiles[pr].active) continue;
            float pdx = projectiles[pr].pos.x - sig.pos.x; float pdy = projectiles[pr].pos.y - sig.pos.y;
            float pdistSq = pdx*pdx + pdy*pdy;
            if(pdistSq > 0.1f && pdistSq < 250000.0f) {
                float pdist = sqrtf(pdistSq);
                float f = forceMult / (pdistSq * 0.02f + 1.0f);
                projectiles[pr].pos.x -= (pdx/pdist) * f * dt; projectiles[pr].pos.y -= (pdy/pdist) * f * dt;
                
                if (sig.type == 1 && pdist < 15.0f) {
                    if (targetWH != -1) {
                        projectiles[pr].pos = sys_singularities[targetWH].pos;
                        projectiles[pr].basePos = projectiles[pr].pos;
                        projectiles[pr].velocity.x *= -2.0f; projectiles[pr].velocity.y *= -2.0f; // Eject
                    } else {
                        projectiles[pr].active = false;
                        grid[LAYER_GROUND][sig.index].density += projectiles[pr].payload.density; 
                    }
                } 
            }
        }
    }

    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) continue;
        
        projectiles[i].flightTime += dt;
        float t = projectiles[i].flightTime;
        
        projectiles[i].payload.temp = 20.0f; projectiles[i].payload.density = 0.0f; 
        projectiles[i].payload.moisture = 0.0f; projectiles[i].payload.charge = 0.0f; 
        projectiles[i].payload.cohesion = 0.0f; projectiles[i].payload.sizeMod = 1.0f;
        projectiles[i].payload.distortion = 0.0f; projectiles[i].payload.movement = MOVE_STRAIGHT;
        
        float currentSpeedMod = 1.0f;
        bool triggerCollisionSpread = false;

        for(int n=0; n<MAX_NODES; n++) {
            SpellNode *node = &projectiles[i].payload.graph.nodes[n];
            if(!node->active) continue;
            if(!IsDescendant(&projectiles[i].payload.graph, n, projectiles[i].rootId)) continue;
            
            float delay = node->hasDelay ? node->delay : 0.0f; 
            if(t < delay) continue; 

            if(node->hasSpread && !node->triggered && t >= delay) {
                node->triggered = true;
                if(node->spreadType == SPREAD_INSTANT) {
                    float baseAngle = atan2f(projectiles[i].velocity.y, projectiles[i].velocity.x);
                    for (int offset = -15; offset <= 15; offset += 15) {
                        float spreadAngle = baseAngle + (offset * PI / 180.0f);
                        Vector2 spreadTarget = { projectiles[i].pos.x + cosf(spreadAngle)*100.0f, projectiles[i].pos.y + sinf(spreadAngle)*100.0f };
                        CastDynamicProjectile(projectiles[i].pos, spreadTarget, projectiles[i].layer, &projectiles[i].payload, n, projectiles[i].chargeMult, projectiles[i].lifeMult);
                    }
                    continue; 
                } else if (node->spreadType == SPREAD_COLLISION) { triggerCollisionSpread = true; }
            }
            if(node->hasSpread && node->spreadType == SPREAD_INSTANT) continue; 

            Vector2 outDir; float resonance = 1.0f;
            float mag = GetNodeMagnitude(&projectiles[i].payload.graph, n, projectiles[i].rootId, &outDir, &resonance);
            
            // Intensify and Lifespan Multipliers!
            float cmult = projectiles[i].chargeMult;
            float lmult = projectiles[i].lifeMult;
            
            projectiles[i].payload.temp += (node->temp * mag * cmult) * resonance; 
            projectiles[i].payload.density += (node->density * mag * cmult * lmult) * resonance;
            projectiles[i].payload.moisture += (node->moisture * mag * cmult) * resonance; 
            projectiles[i].payload.charge += (node->charge * mag * cmult) * resonance;
            projectiles[i].payload.cohesion = (projectiles[i].payload.cohesion == 0) ? (node->cohesion * lmult) : (projectiles[i].payload.cohesion + node->cohesion * lmult)/2.0f;
            
            if(node->hasSize) projectiles[i].payload.sizeMod *= (node->sizeMod * cmult);
            if(node->hasDistort) projectiles[i].payload.distortion += node->distortion;
            projectiles[i].payload.movement = node->movement;

            if(node->hasSpeed) {
                if(node->easeTime > 0.0f) {
                    float easeProg = fminf(1.0f, (t - delay) / node->easeTime);
                    float easeOut = 1.0f - (1.0f - easeProg) * (1.0f - easeProg); 
                    currentSpeedMod *= 1.0f + (node->speedMod - 1.0f) * easeOut;
                } else { currentSpeedMod *= node->speedMod; }
            }
        }
        
        // CORE DELAY MECHANIC (Mines/Traps): Freeze entirely in mid air!
        if (projectiles[i].payload.graph.nodes[projectiles[i].rootId].hasDelay && t < projectiles[i].payload.graph.nodes[projectiles[i].rootId].delay) {
            projectiles[i].velocity = (Vector2){0,0};
        } else {
            projectiles[i].velocity.x = projectiles[i].baseVelocity.x * currentSpeedMod;
            projectiles[i].velocity.y = projectiles[i].baseVelocity.y * currentSpeedMod;
            projectiles[i].basePos.x += projectiles[i].velocity.x * dt;
            projectiles[i].basePos.y += projectiles[i].velocity.y * dt;
        }

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
                    float dx = projectiles[i].pos.x - active_npcs[j].pos.x; float dy = projectiles[i].pos.y - active_npcs[j].pos.y;
                    if (dx*dx + dy*dy < 400.0f) { hitSolid = true; active_npcs[j].health -= projectiles[i].payload.density + projectiles[i].payload.temp; break; }
                }
            }
            if (!hitSolid && p->friendlyFire && (projectiles[i].maxLife - projectiles[i].life > 0.1f)) {
                float dx = projectiles[i].pos.x - p->pos.x; float dy = projectiles[i].pos.y - p->pos.y;
                if (dx*dx + dy*dy < 200.0f) { hitSolid = true; p->health -= projectiles[i].payload.density + projectiles[i].payload.temp; }
            }
        }

        if (projectiles[i].life <= 0 || hitSolid) {
            if (triggerCollisionSpread) {
                for(int n=0; n<MAX_NODES; n++) {
                    SpellNode *cNode = &projectiles[i].payload.graph.nodes[n];
                    if(cNode->active && IsDescendant(&projectiles[i].payload.graph, n, projectiles[i].rootId) && cNode->hasSpread && cNode->spreadType == SPREAD_COLLISION && !cNode->triggered) {
                        for(int k=0; k<3; k++) {
                            Vector2 randTarget = { projectiles[i].pos.x + GetRandomValue(-100,100), projectiles[i].pos.y + GetRandomValue(-100,100) };
                            CastDynamicProjectile(projectiles[i].pos, randTarget, projectiles[i].layer, &projectiles[i].payload, n, projectiles[i].chargeMult, projectiles[i].lifeMult);
                        }
                    }
                }
            }
            int radius = (int)(projectiles[i].payload.density / 20.0f) * projectiles[i].payload.sizeMod + 1;
            InjectEnergyArea(gx, gy, projectiles[i].layer, radius, projectiles[i].payload); 
            projectiles[i].active = false;
        }
    }

    int px = (int)(p->pos.x / PIXEL_SIZE); int py = (int)(p->pos.y / PIXEL_SIZE);
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT && p->z < 5.0f) {
        Cell g = grid[LAYER_GROUND][py * WIDTH + px];
        if (g.temp > 60.0f) p->health -= (g.temp - 60.0f) * 0.5f * dt;
        if (g.temp < -10.0f) p->health -= fabsf(g.temp + 10.0f) * 0.5f * dt;
        float momentum = g.density * sqrtf(g.velocity.x*g.velocity.x + g.velocity.y*g.velocity.y);
        if (momentum > 150.0f) p->health -= (momentum - 150.0f) * 0.1f * dt;
        if (g.charge > 40.0f) p->health -= g.charge * 0.2f * dt;
    }
}

void ExecuteSpell(Player *p, Vector2 target, SpellDNA *dna, float chargeMult, float lifeMult) {
    if(!dna->graph.nodes[0].active) return;
    
    SpellDNA immediate = {0};
    immediate.temp = 20.0f; immediate.sizeMod = 1.0f;
    Vector2 dummyDir; float res; 
    
    for(int n=0; n<MAX_NODES; n++) {
        if(dna->graph.nodes[n].active) {
            float mag = GetNodeMagnitude(&dna->graph, n, 0, &dummyDir, &res);
            immediate.temp += dna->graph.nodes[n].temp * mag * chargeMult * res;
            immediate.density += dna->graph.nodes[n].density * mag * chargeMult * lifeMult * res;
            immediate.cohesion += dna->graph.nodes[n].cohesion * lifeMult;
            immediate.charge += dna->graph.nodes[n].charge * mag * chargeMult * res;
            if(dna->graph.nodes[n].hasSize) immediate.sizeMod *= dna->graph.nodes[n].sizeMod * chargeMult;
        }
    }
    
    int gx = (int)(target.x / PIXEL_SIZE); int gy = (int)(target.y / PIXEL_SIZE);
    int px = (int)(p->pos.x / PIXEL_SIZE); int py = (int)(p->pos.y / PIXEL_SIZE);
    int radius = (int)(1 + chargeMult) * immediate.sizeMod;

    switch(dna->form) {
        case FORM_PROJECTILE: CastDynamicProjectile(p->pos, target, p->castLayer, dna, 0, chargeMult, lifeMult); break;
        case FORM_MANIFEST: InjectEnergyArea(gx, gy, p->castLayer, radius + 1, immediate); break;
        case FORM_AURA: InjectEnergyArea(px, py, p->castLayer, radius + 3, immediate); break;
        case FORM_BEAM: InjectBeam(p->pos, target, p->castLayer, immediate); break;
    }
}

void CastDynamicProjectile(Vector2 start, Vector2 target, int layer, SpellDNA *dna, int rootId, float chargeMult, float lifeMult) {
    for(int i=0; i<100; i++) {
        if(!projectiles[i].active) {
            projectiles[i].pos = start; projectiles[i].basePos = start;
            projectiles[i].payload = *dna; projectiles[i].rootId = rootId;
            projectiles[i].chargeMult = chargeMult; projectiles[i].lifeMult = lifeMult;
            projectiles[i].layer = layer;
            
            for(int n=0; n<MAX_NODES; n++) projectiles[i].payload.graph.nodes[n].triggered = false;

            SpellNode root = dna->graph.nodes[rootId];
            float baseLife = (root.movement == MOVE_ORBIT) ? 10.0f : 1.5f;
            
            float accumulatedRange = 1.0f;
            for(int n=0; n<MAX_NODES; n++) if(dna->graph.nodes[n].active && dna->graph.nodes[n].hasRange && IsDescendant(&dna->graph, n, rootId)) accumulatedRange *= dna->graph.nodes[n].rangeMod;

            projectiles[i].life = baseLife * accumulatedRange * lifeMult; 
            projectiles[i].maxLife = projectiles[i].life; 
            projectiles[i].flightTime = 0.0f;
            projectiles[i].active = true; projectiles[i].animOffset = 0.0f;
            
            float angle = atan2f(target.y - start.y, target.x - start.x);
            float cosA = cosf(angle); float sinA = sinf(angle);
            
            Vector2 localV = {0}; Vector2 dummyDir; float res; 
            for(int n=0; n<MAX_NODES; n++) {
                if(dna->graph.nodes[n].active && IsDescendant(&dna->graph, n, rootId)) {
                    float mag = GetNodeMagnitude(&dna->graph, n, rootId, &dummyDir, &res);
                    float force = (fabsf(dna->graph.nodes[n].density) + fabsf(dna->graph.nodes[n].temp)) * mag * res;
                    localV.x += dummyDir.x * force; localV.y += dummyDir.y * force;
                }
            }
            if (fabsf(localV.x) < 10.0f && fabsf(localV.y) < 10.0f) { localV.x = 50.0f; }

            float rotatedVx = localV.x * cosA - localV.y * sinA; float rotatedVy = localV.x * sinA + localV.y * cosA;
            projectiles[i].baseVelocity = (Vector2){ (cosA * 350.0f) + rotatedVx, (sinA * 350.0f) + rotatedVy };
            projectiles[i].velocity = projectiles[i].baseVelocity; 
            break;
        }
    }
}

void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna) {
    float dx = target.x - start.x; float dy = target.y - start.y;
    float dist = sqrtf(dx*dx + dy*dy) * dna.rangeMod;
    if (dist <= 0.01f) return;
    float steps = dist / PIXEL_SIZE;
    if (steps <= 0.001f) return;
    
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
}

void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna) {
    for(int dy = -radius; dy <= radius; dy++) {
        for(int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) InjectEnergy(cx + dx, cy + dy, z, dna);
        }
    }
}