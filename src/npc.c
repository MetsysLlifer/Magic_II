#include "util.h"

NPC active_npcs[MAX_NPCS];

static float Vec2Length(Vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static Vector2 Vec2NormalizeSafe(Vector2 v) {
    float len = Vec2Length(v);
    if (len <= 0.0001f) return (Vector2){0};
    return (Vector2){ v.x / len, v.y / len };
}

static float SampleCellHazard(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 10.0f;
    Cell c = grid[LAYER_GROUND][y * WIDTH + x];

    float heatHarm = fmaxf(0.0f, c.temp - 60.0f) / 60.0f;
    float coldHarm = fmaxf(0.0f, -10.0f - c.temp) / 60.0f;
    float shockHarm = fmaxf(0.0f, c.charge - 40.0f) / 50.0f;
    float momentum = c.density * Vec2Length(c.velocity);
    float kinHarm = fmaxf(0.0f, momentum - 60.0f) / 120.0f;

    return heatHarm + coldHarm + shockHarm + kinHarm;
}

static Player *FindNearestPlayer(Player players[], int playerCount, Vector2 pos, float *outDist) {
    Player *best = NULL;
    float bestDist = 999999.0f;

    for (int i = 0; i < playerCount; i++) {
        if (players[i].health <= 0.0f) continue;
        float dx = players[i].pos.x - pos.x;
        float dy = players[i].pos.y - pos.y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < bestDist) {
            bestDist = d;
            best = &players[i];
        }
    }

    if (outDist) *outDist = bestDist;
    return best;
}

static Vector2 ComputeHazardAvoidance(NPC *n) {
    static const Vector2 probes[8] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 0.7f, 0.7f }, { -0.7f, 0.7f }, { 0.7f, -0.7f }, { -0.7f, -0.7f }
    };

    Vector2 avoid = {0};
    float sampleRadius = 22.0f + (n->dna.intelligence * 0.25f);

    for (int i = 0; i < 8; i++) {
        Vector2 dir = Vec2NormalizeSafe(probes[i]);
        int sx = (int)((n->pos.x + dir.x * sampleRadius) / PIXEL_SIZE);
        int sy = (int)((n->pos.y + dir.y * sampleRadius) / PIXEL_SIZE);

        float hazard = SampleCellHazard(sx, sy);
        avoid.x -= dir.x * hazard;
        avoid.y -= dir.y * hazard;
    }

    return Vec2NormalizeSafe(avoid);
}

static Vector2 ComputeSingularityIntent(NPC *n, Player *focus, float focusDist) {
    Vector2 intent = {0};
    float intel = n->dna.intelligence;
    bool wantsTraversal = (intel > 70.0f && focus && focusDist > 450.0f);

    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (!sys_singularities[i].active) continue;

        Singularity *s = &sys_singularities[i];
        float dx = s->pos.x - n->pos.x;
        float dy = s->pos.y - n->pos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < 0.001f) continue;

        Vector2 dir = { dx / dist, dy / dist };

        if (s->type == 1) {
            bool usefulTeleport = false;
            if (wantsTraversal && s->linkedTo != -1 && s->linkedTo < MAX_SINGULARITIES && sys_singularities[s->linkedTo].active && focus) {
                Singularity *white = &sys_singularities[s->linkedTo];
                float tx = white->pos.x - focus->pos.x;
                float ty = white->pos.y - focus->pos.y;
                float whiteToFocus = sqrtf(tx * tx + ty * ty);
                usefulTeleport = (whiteToFocus < 260.0f);
            }

            if (usefulTeleport) {
                intent.x += dir.x * 1.2f;
                intent.y += dir.y * 1.2f;
            } else if (dist < 220.0f) {
                float w = (220.0f - dist) / 220.0f;
                intent.x -= dir.x * w * 1.5f;
                intent.y -= dir.y * w * 1.5f;
            }
        } else {
            if (dist < 180.0f) {
                float w = (180.0f - dist) / 180.0f;
                intent.x += dir.x * w * 0.7f;
                intent.y += dir.y * w * 0.7f;
            }
         }
     }

    return Vec2NormalizeSafe(intent);
}

void InitNPCs() {
    for (int i = 0; i < MAX_NPCS; i++) active_npcs[i].active = false;
}

void SpawnNPC(Vector2 pos, NPCDNA dna) {
    for (int i = 0; i < MAX_NPCS; i++) {
        if (!active_npcs[i].active) {
            active_npcs[i].pos = pos;
            active_npcs[i].z = (dna.aero > 50.0f) ? 30.0f : 0.0f;
            active_npcs[i].velocity = (Vector2){0};
            active_npcs[i].zVelocity = 0.0f;
            active_npcs[i].dna = dna;
            active_npcs[i].health = 5.0f + dna.mass;
            active_npcs[i].animTime = GetRandomValue(0, 100) / 10.0f;
            active_npcs[i].active = true;
            break;
        }
    }
}

void UpdateNPCs(float dt, Player players[], int playerCount) {
    for (int i = 0; i < MAX_NPCS; i++) {
        if (!active_npcs[i].active) continue;

        NPC *n = &active_npcs[i];
        n->animTime += dt;

        int gx = (int)(n->pos.x / PIXEL_SIZE);
        int gy = (int)(n->pos.y / PIXEL_SIZE);
        bool inWater = false;

        if (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT) {
            Cell c = grid[LAYER_GROUND][gy * WIDTH + gx];
            if (c.moisture > 40.0f && c.density > 20.0f && c.cohesion < 50.0f) inWater = true;

            if (n->z < 5.0f) {
                if (c.temp > 60.0f) n->health -= (c.temp - 60.0f) * 0.5f * dt;
                if (c.temp < -10.0f) n->health -= fabsf(c.temp + 10.0f) * 0.5f * dt;

                float momentum = c.density * Vec2Length(c.velocity);
                if (momentum > 50.0f) {
                    n->health -= (momentum - 50.0f) * 0.1f * dt;
                    n->pos.x += c.velocity.x * (momentum / (n->dna.mass + 1.0f)) * dt;
                    n->pos.y += c.velocity.y * (momentum / (n->dna.mass + 1.0f)) * dt;
                }

                if (c.charge > 40.0f) n->health -= c.charge * 0.5f * dt;
            }
        }

        float distToPlayer = 999999.0f;
        Player *focus = FindNearestPlayer(players, playerCount, n->pos, &distToPlayer);

        Vector2 targetDir = {0};
        if (focus && n->dna.intelligence > 15.0f) {
            float dx = focus->pos.x - n->pos.x;
            float dy = focus->pos.y - n->pos.y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 0.001f && d < 320.0f) {
                float sign = (n->dna.hostility > 50.0f) ? 1.0f : -1.0f;
                targetDir.x = (dx / d) * sign;
                targetDir.y = (dy / d) * sign;

                if (n->dna.hostility > 50.0f && d < 15.0f && focus->z <= n->z + 10.0f) {
                    focus->health -= (n->dna.mass * 0.1f) * dt;
                }
            }
        }

        if (Vec2Length(targetDir) < 0.05f) {
            targetDir.x = sinf(n->animTime * (n->dna.intelligence / 45.0f + 0.2f));
            targetDir.y = cosf(n->animTime * 0.45f);
        }

        Vector2 hazardAvoid = ComputeHazardAvoidance(n);
        Vector2 singularityIntent = ComputeSingularityIntent(n, focus, distToPlayer);
        float intelFactor = fminf(1.5f, fmaxf(0.2f, n->dna.intelligence / 65.0f));

        targetDir.x += hazardAvoid.x * (0.8f + intelFactor * 0.8f);
        targetDir.y += hazardAvoid.y * (0.8f + intelFactor * 0.8f);
        targetDir.x += singularityIntent.x * intelFactor;
        targetDir.y += singularityIntent.y * intelFactor;
        targetDir = Vec2NormalizeSafe(targetDir);

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

        speed *= fmaxf(0.5f, 1.1f - SampleCellHazard(gx, gy) * 0.2f);

        n->velocity.x = targetDir.x * speed;
        n->velocity.y = targetDir.y * speed;

        float nextX = n->pos.x + n->velocity.x * dt;
        float nextY = n->pos.y + n->velocity.y * dt;
        int cx = (int)(nextX / PIXEL_SIZE);
        int cy = (int)(nextY / PIXEL_SIZE);
        if (cx >= 0 && cx < WIDTH && cy >= 0 && cy < HEIGHT) {
            Cell ahead = grid[LAYER_GROUND][cy * WIDTH + cx];
            if (ahead.cohesion > 80.0f && n->z < 15.0f) {
                n->velocity = (Vector2){0};
                nextX = n->pos.x;
                nextY = n->pos.y;
            }
        }

        n->pos.x = nextX;
        n->pos.y = nextY;

        if (n->health <= 0.0f) n->active = false;
    }
}

void DrawProceduralNPC(Vector2 pos, float z, NPCDNA dna, float alpha) {
    float scale = 3.0f + (dna.mass / 20.0f);
    float breathe = (sinf(GetTime() * 4.0f) + 1.0f) * 0.5f;
    Vector2 renderPos = { pos.x, pos.y - z };
    Color baseCol = (dna.hostility > 50.0f) ? Fade(RED, alpha) : Fade(GREEN, alpha);

    if (z > 0) DrawEllipse(pos.x, pos.y, scale, scale / 2, Fade(BLACK, 0.4f * alpha));
    if (z < 0) DrawEllipse(pos.x, pos.y, scale * 1.5f, scale * 1.5f, Fade(BLUE, 0.3f * alpha));

    if (dna.aero > 30.0f) {
        float flap = sinf(GetTime() * (dna.aero / 5.0f)) * scale;
        DrawEllipse(renderPos.x - scale, renderPos.y, scale, 2.0f + flap, Fade(SKYBLUE, alpha));
        DrawEllipse(renderPos.x + scale, renderPos.y, scale, 2.0f + flap, Fade(SKYBLUE, alpha));
    }
    if (dna.hydro > 30.0f) {
        float swish = sinf(GetTime() * 5.0f) * (scale / 2.0f);
        DrawTriangle(renderPos, (Vector2){renderPos.x - scale + swish, renderPos.y + scale * 1.5f},
                     (Vector2){renderPos.x + scale + swish, renderPos.y + scale * 1.5f}, Fade(DARKBLUE, alpha));
    }
    if (dna.terrestrial > 30.0f && z <= 0.0f) {
        float march = sinf(GetTime() * 10.0f) * 3.0f;
        DrawLineEx(renderPos, (Vector2){renderPos.x - scale / 2, renderPos.y + scale + march}, 2.0f, Fade(BROWN, alpha));
        DrawLineEx(renderPos, (Vector2){renderPos.x + scale / 2, renderPos.y + scale - march}, 2.0f, Fade(BROWN, alpha));
    }

    DrawEllipse(renderPos.x, renderPos.y, scale + (breathe * 0.5f), scale, baseCol);
    if (dna.intelligence > 0.0f) {
        float eyeSize = 1.0f + (dna.intelligence / 25.0f);
        DrawCircle(renderPos.x, renderPos.y - (scale / 2), eyeSize, Fade(RAYWHITE, alpha));
    }
}

void DrawNPCs() {
    for (int i = 0; i < MAX_NPCS; i++) {
        if (active_npcs[i].active) {
            DrawProceduralNPC(active_npcs[i].pos, active_npcs[i].z, active_npcs[i].dna, 1.0f);
            float hpPercent = active_npcs[i].health / (5.0f + active_npcs[i].dna.mass);
            DrawRectangle(active_npcs[i].pos.x - 10, active_npcs[i].pos.y - active_npcs[i].z - 15, 20, 2, RED);
            DrawRectangle(active_npcs[i].pos.x - 10, active_npcs[i].pos.y - active_npcs[i].z - 15, 20 * hpPercent, 2, GREEN);
        }
    }
}
