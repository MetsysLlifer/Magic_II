#include "util.h"

NPC active_npcs[50];

void InitNPCs() {
    for(int i=0; i<50; i++) active_npcs[i].active = false;
}

void SpawnNPC(Vector2 pos, NPCDNA dna) {
    for(int i=0; i<50; i++) {
        if(!active_npcs[i].active) {
            active_npcs[i].pos = pos;
            active_npcs[i].z = (dna.aero > 50.0f) ? 30.0f : 0.0f; 
            active_npcs[i].velocity = (Vector2){0,0};
            active_npcs[i].zVelocity = 0.0f;
            active_npcs[i].dna = dna;
            active_npcs[i].health = 100.0f + (dna.mass * 2.0f);
            active_npcs[i].animTime = GetRandomValue(0, 100) / 10.0f;
            active_npcs[i].active = true;
            break;
        }
    }
}

void UpdateNPCs(float dt, Player *p) {
    for(int i=0; i<50; i++) {
        if(!active_npcs[i].active) continue;
        NPC *n = &active_npcs[i];
        n->animTime += dt;

        int gx = (int)(n->pos.x / PIXEL_SIZE);
        int gy = (int)(n->pos.y / PIXEL_SIZE);
        
        bool inWater = false;
        if (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT) {
            Cell c = grid[LAYER_GROUND][gy * WIDTH + gx];
            if (c.moisture > 40.0f && c.density > 20.0f && c.cohesion < 50.0f) inWater = true;
            if (c.temp > 80.0f && n->z <= 0) n->health -= 10.0f * dt; 
        }

        Vector2 targetDir = {0, 0};
        float distToPlayer = sqrtf(powf(p->pos.x - n->pos.x, 2) + powf(p->pos.y - n->pos.y, 2));

        if (n->dna.intelligence > 20.0f) {
            if (distToPlayer < 300.0f) {
                float dx = p->pos.x - n->pos.x;
                float dy = p->pos.y - n->pos.y;
                float direction = (n->dna.hostility > 50.0f) ? 1.0f : -1.0f; 
                targetDir.x = (dx / distToPlayer) * direction;
                targetDir.y = (dy / distToPlayer) * direction;
                
                if (n->dna.hostility > 50.0f && distToPlayer < 15.0f && p->z <= n->z + 10.0f) {
                    p->health -= (n->dna.mass * 0.1f) * dt;
                }
            } else {
                targetDir.x = sinf(n->animTime * (n->dna.intelligence / 50.0f));
                targetDir.y = cosf(n->animTime * 0.5f);
            }
        }

        float speed = 0.0f;
        if (inWater) {
            speed = n->dna.hydro * 1.5f;
            if (n->dna.hydro > 50.0f) n->z = -10.0f - (n->dna.mass / 10.0f); 
        } else if (n->z > 5.0f) {
            speed = n->dna.aero * 2.0f;
            n->z = 20.0f + sinf(n->animTime * 2.0f) * (n->dna.aero / 10.0f);
        } else {
            speed = n->dna.terrestrial * 1.2f;
            n->z = 0.0f; 
        }

        n->velocity.x = targetDir.x * speed;
        n->velocity.y = targetDir.y * speed;

        n->pos.x += n->velocity.x * dt;
        n->pos.y += n->velocity.y * dt;

        if (n->health <= 0) n->active = false;
    }
}

void DrawProceduralNPC(Vector2 pos, float z, NPCDNA dna, float alpha) {
    float scale = 3.0f + (dna.mass / 20.0f);
    float breathe = (sinf(GetTime() * 4.0f) + 1.0f) * 0.5f;
    Vector2 renderPos = { pos.x, pos.y - z };
    
    Color baseCol = (dna.hostility > 50.0f) ? Fade(RED, alpha) : Fade(GREEN, alpha);
    
    if (z > 0) DrawEllipse(pos.x, pos.y, scale, scale/2, Fade(BLACK, 0.4f * alpha));
    if (z < 0) DrawEllipse(pos.x, pos.y, scale * 1.5f, scale * 1.5f, Fade(BLUE, 0.3f * alpha));

    if (dna.aero > 30.0f) {
        float flap = sinf(GetTime() * (dna.aero / 5.0f)) * scale;
        DrawEllipse(renderPos.x - scale, renderPos.y, scale, 2.0f + flap, Fade(SKYBLUE, alpha));
        DrawEllipse(renderPos.x + scale, renderPos.y, scale, 2.0f + flap, Fade(SKYBLUE, alpha));
    }
    
    if (dna.hydro > 30.0f) {
        float swish = sinf(GetTime() * 5.0f) * (scale / 2.0f);
        DrawTriangle(renderPos, 
                     (Vector2){renderPos.x - scale + swish, renderPos.y + scale*1.5f}, 
                     (Vector2){renderPos.x + scale + swish, renderPos.y + scale*1.5f}, Fade(DARKBLUE, alpha));
    }

    if (dna.terrestrial > 30.0f && z <= 0.0f) {
        float march = sinf(GetTime() * 10.0f) * 3.0f;
        DrawLineEx(renderPos, (Vector2){renderPos.x - scale/2, renderPos.y + scale + march}, 2.0f, Fade(BROWN, alpha));
        DrawLineEx(renderPos, (Vector2){renderPos.x + scale/2, renderPos.y + scale - march}, 2.0f, Fade(BROWN, alpha));
    }

    DrawEllipse(renderPos.x, renderPos.y, scale + (breathe * 0.5f), scale, baseCol);
    
    if (dna.intelligence > 0.0f) {
        float eyeSize = 1.0f + (dna.intelligence / 25.0f);
        DrawCircle(renderPos.x, renderPos.y - (scale/2), eyeSize, Fade(RAYWHITE, alpha));
    }
}

void DrawNPCs() {
    for(int i=0; i<50; i++) {
        if(active_npcs[i].active) {
            DrawProceduralNPC(active_npcs[i].pos, active_npcs[i].z, active_npcs[i].dna, 1.0f);
            float hpPercent = active_npcs[i].health / (100.0f + (active_npcs[i].dna.mass * 2.0f));
            DrawRectangle(active_npcs[i].pos.x - 10, active_npcs[i].pos.y - active_npcs[i].z - 15, 20, 2, RED);
            DrawRectangle(active_npcs[i].pos.x - 10, active_npcs[i].pos.y - active_npcs[i].z - 15, 20 * hpPercent, 2, GREEN);
        }
    }
}