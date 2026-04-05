#include "util.h"

#include <stdio.h>
#include <sys/stat.h>

Cell grid[2][WIDTH * HEIGHT];
Cell prev_grid[2][WIDTH * HEIGHT];
Projectile projectiles[MAX_PROJECTILES];
Singularity sys_singularities[MAX_SINGULARITIES];
int sys_sigCount = 0;

static const float kChargeInfluence[SCALAR_COUNT] = { 1.0f, 1.0f, 0.9f, 0.2f, 1.0f };
static const float kLifeInfluence[SCALAR_COUNT] = { 0.1f, 1.0f, 0.4f, 1.0f, 0.1f };

#define SAVE_MAGIC 0x4D535659u
#define SAVE_VERSION 2u

typedef struct {
    unsigned int magic;
    unsigned int version;
    Player player;
    Cell worldGrid[2][WIDTH * HEIGHT];
    NPC npcs[MAX_NPCS];
} PersistBlob;

static int ClampChannel(int channel) {
    if (channel < 0) return 0;
    if (channel >= SCALAR_COUNT) return SCALAR_COUNT - 1;
    return channel;
}

static float GetRuntimeScalarMultiplier(int channel, float chargeMult, float lifeMult) {
    int ch = ClampChannel(channel);
    return 1.0f + (chargeMult - 1.0f) * kChargeInfluence[ch] + (lifeMult - 1.0f) * kLifeInfluence[ch];
}

static void ResetCompiledDNA(SpellDNA *dna) {
    for (int c = 0; c < SCALAR_COUNT; c++) dna->channels[c] = 0.0f;
    dna->channels[SCALAR_TEMP] = 20.0f;

    dna->temp = 20.0f;
    dna->density = 0.0f;
    dna->moisture = 0.0f;
    dna->cohesion = 0.0f;
    dna->charge = 0.0f;
    dna->velocity = (Vector2){0};

    dna->movement = MOVE_STRAIGHT;
    dna->speedMod = 1.0f;
    dna->delay = 0.0f;
    dna->distortion = 0.0f;
    dna->rangeMod = 1.0f;
    dna->sizeMod = 1.0f;
    dna->spreadType = SPREAD_OFF;

    dna->toolType = TOOL_NONE;
    dna->toolPower = 1.0f;
    dna->toolRadius = 2.0f;
    dna->toolPermanent = false;
}

static void ApplyChannelsToLegacyFields(SpellDNA *dna) {
    dna->temp = dna->channels[SCALAR_TEMP];
    dna->density = dna->channels[SCALAR_DENSITY];
    dna->moisture = dna->channels[SCALAR_MOISTURE];
    dna->cohesion = dna->channels[SCALAR_COHESION];
    dna->charge = dna->channels[SCALAR_CHARGE];
}

void InitDefaultSpellNode(SpellNode *node) {
    if (!node) return;
    memset(node, 0, sizeof(*node));

    node->parentId = -1;
    node->movement = MOVE_STRAIGHT;

    node->speedMod = 1.0f;
    node->easeTime = 0.0f;
    node->delay = 0.0f;
    node->distortion = 0.0f;
    node->rangeMod = 1.0f;
    node->sizeMod = 1.0f;
    node->spreadType = SPREAD_OFF;

    for (int c = 0; c < SCALAR_COUNT; c++) {
        node->scalarAdd[c] = 0.0f;
        node->scalarScale[c] = 1.0f;
    }

    node->conditionType = COND_ALWAYS;
    node->conditionChannel = SCALAR_TEMP;
    node->conditionThreshold = 0.0f;
    node->detachOnCondition = false;

    node->hasTool = false;
    node->toolType = TOOL_NONE;
    node->toolPower = 1.0f;
    node->toolRadius = 2.0f;
    node->toolPermanent = false;
}

void SyncNodeLegacyScalars(SpellNode *node) {
    if (!node) return;

    node->scalarAdd[SCALAR_TEMP] = node->temp;
    node->scalarAdd[SCALAR_DENSITY] = node->density;
    node->scalarAdd[SCALAR_MOISTURE] = node->moisture;
    node->scalarAdd[SCALAR_COHESION] = node->cohesion;
    node->scalarAdd[SCALAR_CHARGE] = node->charge;

    for (int c = 0; c < SCALAR_COUNT; c++) {
        if (fabsf(node->scalarScale[c]) < 0.0001f) node->scalarScale[c] = 1.0f;
    }
}

static bool EvaluateNodeCondition(const SpellNode *node, const SpellDNA *state, float flightTime) {
    if (!node) return false;

    switch (node->conditionType) {
        case COND_FLIGHT_TIME_GT:
            return flightTime >= node->conditionThreshold;
        case COND_SCALAR_GT: {
            int ch = ClampChannel(node->conditionChannel);
            return state->channels[ch] > node->conditionThreshold;
        }
        case COND_SCALAR_LT: {
            int ch = ClampChannel(node->conditionChannel);
            return state->channels[ch] < node->conditionThreshold;
        }
        case COND_ALWAYS:
        default:
            return true;
    }
}

static void ConsumeNodeToDNA(SpellDNA *dna, SpellNode *node, float magnitude, float resonance, float chargeMult, float lifeMult, Vector2 dir) {
    if (!dna || !node) return;

    SyncNodeLegacyScalars(node);

    for (int c = 0; c < SCALAR_COUNT; c++) {
        float nodeScale = (fabsf(node->scalarScale[c]) < 0.0001f) ? 1.0f : node->scalarScale[c];
        float channelMult = GetRuntimeScalarMultiplier(c, chargeMult, lifeMult);
        dna->channels[c] += node->scalarAdd[c] * nodeScale * magnitude * resonance * channelMult;
    }

    if (node->parentId >= 0 && node->parentId < MAX_NODES) {
        float force = (fabsf(node->scalarAdd[SCALAR_DENSITY]) + fabsf(node->scalarAdd[SCALAR_TEMP])) * magnitude * resonance * fmaxf(1.0f, chargeMult);
        dna->velocity.x += dir.x * force;
        dna->velocity.y += dir.y * force;
    }

    if (node->hasSize) dna->sizeMod *= node->sizeMod;
    if (node->hasDistort) dna->distortion += node->distortion;
    if (node->hasRange) dna->rangeMod *= node->rangeMod;

    dna->movement = node->movement;

    if (node->hasTool) {
        if (dna->toolType == TOOL_NONE || node->toolPower >= dna->toolPower) {
            dna->toolType = node->toolType;
            dna->toolPower = fmaxf(0.5f, node->toolPower);
            dna->toolRadius = fmaxf(1.0f, node->toolRadius);
            dna->toolPermanent = node->toolPermanent;
        }
    }
}

static void InitSingularitySystem() {
    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        sys_singularities[i].active = false;
        sys_singularities[i].linkedTo = -1;
    }
    sys_sigCount = 0;
}

static int CountActiveSingularities() {
    int count = 0;
    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (sys_singularities[i].active) count++;
    }
    return count;
}

static int FindWeakestSingularityIndex() {
    int weakest = -1;
    float weakestMass = 999999.0f;
    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (!sys_singularities[i].active) return i;
        if (sys_singularities[i].mass < weakestMass) {
            weakestMass = sys_singularities[i].mass;
            weakest = i;
        }
    }
    return weakest;
}

static void SpawnOrFeedSingularityAtCell(int index, int type, float massSeed, float chargeSeed) {
    if (index < 0 || index >= WIDTH * HEIGHT) return;

    Vector2 pos = {
        (float)((index % WIDTH) * PIXEL_SIZE) + (PIXEL_SIZE * 0.5f),
        (float)((index / WIDTH) * PIXEL_SIZE) + (PIXEL_SIZE * 0.5f)
    };

    int target = -1;
    float bestDistSq = 999999.0f;
    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (!sys_singularities[i].active || sys_singularities[i].type != type) continue;
        float dx = sys_singularities[i].pos.x - pos.x;
        float dy = sys_singularities[i].pos.y - pos.y;
        float distSq = dx * dx + dy * dy;
        if (distSq < (35.0f * 35.0f) && distSq < bestDistSq) {
            bestDistSq = distSq;
            target = i;
        }
    }

    if (target != -1) {
        Singularity *s = &sys_singularities[target];
        s->mass = fminf(1200.0f, s->mass + massSeed * 0.15f);
        s->charge = (s->charge * 0.8f) + (chargeSeed * 0.2f);
        s->lifetime = fminf(20.0f, s->lifetime + 1.25f);
        s->drift.x += GetRandomValue(-10, 10) * 0.01f;
        s->drift.y += GetRandomValue(-10, 10) * 0.01f;
        s->index = index;
        return;
    }

    int slot = FindWeakestSingularityIndex();
    if (slot < 0) return;

    sys_singularities[slot].active = true;
    sys_singularities[slot].index = index;
    sys_singularities[slot].pos = pos;
    sys_singularities[slot].mass = fmaxf(120.0f, massSeed);
    sys_singularities[slot].charge = chargeSeed;
    sys_singularities[slot].type = type;
    sys_singularities[slot].linkedTo = -1;
    sys_singularities[slot].anim = 0.0f;
    sys_singularities[slot].radius = 12.0f + (sys_singularities[slot].mass / 55.0f);
    sys_singularities[slot].strength = 3500.0f;
    sys_singularities[slot].lifetime = 10.0f;
    sys_singularities[slot].drift = (Vector2){ GetRandomValue(-20, 20) * 0.05f, GetRandomValue(-20, 20) * 0.05f };
}

static void UpdateSingularityLifecycle(float dt) {
    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (!sys_singularities[i].active) continue;

        Singularity *s = &sys_singularities[i];
        s->anim += dt;
        s->lifetime -= dt;
        s->mass *= 0.997f;
        s->strength = fminf(8000.0f, 2000.0f + s->mass * 7.0f);
        s->radius = fmaxf(8.0f, 10.0f + s->mass / 60.0f);
        s->drift.x *= 0.995f;
        s->drift.y *= 0.995f;
        s->pos.x += s->drift.x;
        s->pos.y += s->drift.y;

        if (s->pos.x < 0.0f) s->pos.x = 0.0f;
        if (s->pos.x > WORLD_W) s->pos.x = WORLD_W;
        if (s->pos.y < 0.0f) s->pos.y = 0.0f;
        if (s->pos.y > WORLD_H) s->pos.y = WORLD_H;

        if (s->lifetime <= 0.0f || s->mass < 30.0f) {
            s->active = false;
            s->linkedTo = -1;
        }
    }
}

static void PairEntangledSingularities() {
    bool whiteTaken[MAX_SINGULARITIES] = { false };

    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (sys_singularities[i].active) sys_singularities[i].linkedTo = -1;
    }

    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        if (!sys_singularities[i].active || sys_singularities[i].type != 1) continue;

        int bestWhite = -1;
        float bestDistSq = 9999999.0f;
        for (int j = 0; j < MAX_SINGULARITIES; j++) {
            if (!sys_singularities[j].active || sys_singularities[j].type != 2 || whiteTaken[j]) continue;
            if (fabsf(sys_singularities[i].charge - sys_singularities[j].charge) >= 5.0f) continue;

            float dx = sys_singularities[i].pos.x - sys_singularities[j].pos.x;
            float dy = sys_singularities[i].pos.y - sys_singularities[j].pos.y;
            float distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestWhite = j;
            }
        }

        if (bestWhite != -1) {
            sys_singularities[i].linkedTo = bestWhite;
            sys_singularities[bestWhite].linkedTo = i;
            whiteTaken[bestWhite] = true;
        }
    }
}

static void ApplyToolEffect(int cx, int cy, int z, const SpellDNA *dna) {
    if (!dna || dna->toolType == TOOL_NONE) return;

    int radius = (int)fmaxf(1.0f, dna->toolRadius + (fabsf(dna->density) * 0.01f));
    float power = fmaxf(0.5f, dna->toolPower + fabsf(dna->charge) * 0.01f);

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius) continue;

            int x = cx + dx;
            int y = cy + dy;
            if (x < 1 || x >= WIDTH - 1 || y < 1 || y >= HEIGHT - 1) continue;

            int idx = y * WIDTH + x;
            Cell *cell = &grid[z][idx];

            if (cell->permanent && !dna->toolPermanent && dna->toolType != TOOL_SINGULARITY_BLACK && dna->toolType != TOOL_SINGULARITY_WHITE) {
                continue;
            }

            switch (dna->toolType) {
                case TOOL_BUILD:
                    cell->density += 18.0f * power;
                    cell->cohesion += 15.0f * power;
                    if (dna->toolPermanent && cell->cohesion > 85.0f) cell->permanent = true;
                    break;
                case TOOL_DIG:
                    cell->density = fmaxf(0.0f, cell->density - 22.0f * power);
                    cell->cohesion -= 12.0f * power;
                    cell->permanent = false;
                    break;
                case TOOL_MOISTEN:
                    cell->moisture = fminf(200.0f, cell->moisture + 20.0f * power);
                    cell->temp -= 3.0f * power;
                    break;
                case TOOL_DRY:
                    cell->moisture = fmaxf(0.0f, cell->moisture - 20.0f * power);
                    cell->temp += 4.0f * power;
                    break;
                case TOOL_HEAT:
                    cell->temp += 12.0f * power;
                    if (cell->cohesion > 20.0f) cell->cohesion -= 2.0f * power;
                    break;
                case TOOL_COOL:
                    cell->temp -= 12.0f * power;
                    if (cell->moisture > 5.0f) cell->cohesion += 2.0f * power;
                    break;
                case TOOL_CONDUCT:
                    cell->charge += 14.0f * power;
                    cell->velocity.x += (float)GetRandomValue(-20, 20) * 0.04f * power;
                    cell->velocity.y += (float)GetRandomValue(-20, 20) * 0.04f * power;
                    break;
                case TOOL_SINGULARITY_BLACK:
                    cell->density = fmaxf(cell->density, 320.0f + 25.0f * power);
                    cell->cohesion = fmaxf(cell->cohesion, 170.0f + 5.0f * power);
                    cell->charge += dna->charge * 0.2f;
                    SpawnOrFeedSingularityAtCell(idx, 1, cell->density, cell->charge);
                    break;
                case TOOL_SINGULARITY_WHITE:
                    cell->density = fmaxf(cell->density, 320.0f + 25.0f * power);
                    cell->cohesion = fminf(cell->cohesion, -170.0f - 5.0f * power);
                    cell->charge += dna->charge * 0.2f;
                    SpawnOrFeedSingularityAtCell(idx, 2, cell->density, cell->charge);
                    break;
                case TOOL_NONE:
                default:
                    break;
            }
        }
    }
}

void ResetGame(Player *p) {
    InitSimulation();
    InitNPCs();

    p->pos = (Vector2){WORLD_W / 2.0f, WORLD_H / 2.0f};
    p->z = 0.0f;
    p->zVelocity = 0.0f;
    p->health = p->maxHealth;
    p->isJumping = false;

    p->chargeLevel = 0.0f;
    p->lifespanLevel = 0.0f;
    p->isCharging = false;
    p->isLifespanCharging = false;
    p->showCompendium = false;

    p->draggingNodeId = -1;
    p->aimDir = (Vector2){1.0f, 0.0f};
    for (int i = 0; i < 10; i++) p->editStates[i] = false;
}

bool SaveWorldState(const char *path, Player *p) {
    if (!path || !p) return false;

    mkdir("magic_world", 0755);

    FILE *fp = fopen(path, "wb");
    if (!fp) return false;

    PersistBlob *blob = (PersistBlob *)malloc(sizeof(PersistBlob));
    if (!blob) {
        fclose(fp);
        return false;
    }

    memset(blob, 0, sizeof(*blob));
    blob->magic = SAVE_MAGIC;
    blob->version = SAVE_VERSION;
    blob->player = *p;
    memcpy(blob->worldGrid, grid, sizeof(grid));
    memcpy(blob->npcs, active_npcs, sizeof(active_npcs));

    bool ok = (fwrite(blob, sizeof(*blob), 1, fp) == 1);
    free(blob);
    fclose(fp);
    return ok;
}

bool LoadWorldState(const char *path, Player *p) {
    if (!path || !p) return false;

    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    PersistBlob *blob = (PersistBlob *)malloc(sizeof(PersistBlob));
    if (!blob) {
        fclose(fp);
        return false;
    }

    bool ok = (fread(blob, sizeof(*blob), 1, fp) == 1);
    fclose(fp);

    if (!ok) {
        free(blob);
        return false;
    }
    if (blob->magic != SAVE_MAGIC || blob->version != SAVE_VERSION) {
        free(blob);
        return false;
    }

    *p = blob->player;
    memcpy(grid, blob->worldGrid, sizeof(grid));
    memcpy(prev_grid, blob->worldGrid, sizeof(grid));
    memcpy(active_npcs, blob->npcs, sizeof(active_npcs));
    free(blob);

    for (int i = 0; i < MAX_PROJECTILES; i++) projectiles[i].active = false;
    for (int i = 0; i < MAX_SINGULARITIES; i++) {
        sys_singularities[i].active = false;
        sys_singularities[i].linkedTo = -1;
    }
    sys_sigCount = 0;

    p->draggingNodeId = -1;
    p->isCharging = false;
    p->isLifespanCharging = false;
    p->showCompendium = false;

    return true;
}

bool IsDescendant(SigilGraph *g, int child, int root) {
    if (child == root) return true;
    int curr = g->nodes[child].parentId;
    int depth = 0;

    while (curr >= 0 && curr < MAX_NODES && depth < MAX_NODES) {
        if (curr == root) return true;
        curr = g->nodes[curr].parentId;
        depth++;
    }
    return false;
}

float GetNodeMagnitude(SigilGraph *g, int child, int root, Vector2 *outDir, float *resonance) {
    if (child == root) {
        *outDir = (Vector2){0};
        *resonance = 1.0f;
        return 5.0f;
    }

    int pId = g->nodes[child].parentId;
    if (pId < 0 || pId >= MAX_NODES) {
        *outDir = (Vector2){0};
        *resonance = 1.0f;
        return 1.0f;
    }

    Vector2 p1 = g->nodes[child].pos;
    Vector2 p2 = g->nodes[pId].pos;
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist > 0.0001f) {
        outDir->x = dx / dist;
        outDir->y = dy / dist;
    } else {
        *outDir = (Vector2){0};
    }

    float phase = dist * 0.1f + g->nodes[child].charge * 0.05f;
    *resonance = 1.0f + 0.8f * sinf(phase);
    return (dist / 40.0f) + 1.0f;
}

void CompileSigilGraph(SpellDNA *dna) {
    ResetCompiledDNA(dna);
    if (dna->graph.nodes[0].active) dna->movement = dna->graph.nodes[0].movement;

    for (int i = 0; i < MAX_NODES; i++) {
        if (!dna->graph.nodes[i].active) continue;

        SpellNode *node = &dna->graph.nodes[i];
        Vector2 dir = {0};
        float resonance = 1.0f;
        float magnitude = 1.0f;

        if (node->parentId >= 0 && node->parentId < MAX_NODES && dna->graph.nodes[node->parentId].active) {
            magnitude = GetNodeMagnitude(&dna->graph, i, 0, &dir, &resonance);
        }

        ConsumeNodeToDNA(dna, node, magnitude, resonance, 1.0f, 1.0f, dir);
    }

    ApplyChannelsToLegacyFields(dna);
}

void InitSimulation() {
    memset(grid, 0, sizeof(grid));
    memset(prev_grid, 0, sizeof(prev_grid));

    for (int i = 0; i < MAX_PROJECTILES; i++) projectiles[i].active = false;
    InitSingularitySystem();

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int i = y * WIDTH + x;
            if (x < 5 || x >= WIDTH - 5 || y < 5 || y >= HEIGHT - 5) {
                grid[LAYER_GROUND][i].density = 100.0f;
                grid[LAYER_GROUND][i].cohesion = 100.0f;
                grid[LAYER_GROUND][i].permanent = true;
                continue;
            }

            float noise = sinf(x * 0.05f) + cosf(y * 0.05f) + sinf((x + y) * 0.02f);
            if (noise > 1.2f) {
                grid[LAYER_GROUND][i].density = 80.0f;
                grid[LAYER_GROUND][i].cohesion = 90.0f;
                grid[LAYER_GROUND][i].permanent = true;
            } else if (noise < -1.2f) {
                grid[LAYER_GROUND][i].moisture = 100.0f;
                grid[LAYER_GROUND][i].density = 30.0f;
            } else if (noise > 0.4f && noise < 0.8f) {
                grid[LAYER_GROUND][i].density = 40.0f;
                grid[LAYER_GROUND][i].moisture = 60.0f;
                grid[LAYER_GROUND][i].cohesion = 60.0f;
            } else {
                grid[LAYER_GROUND][i].density = 10.0f;
                grid[LAYER_GROUND][i].temp = 20.0f;
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
        }
    }

    if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
        if (p->z > 15.0f || grid[LAYER_GROUND][ny * WIDTH + nx].cohesion < 80.0f) p->pos = nextPos;
    }
}

void Diffuse(float dt) {
    float baseRate = 0.1f * dt * WIDTH * HEIGHT;
    for (int z = 0; z < 2; z++) {
        for (int k = 0; k < 5; k++) {
            for (int y = 1; y < HEIGHT - 1; y++) {
                for (int x = 1; x < WIDTH - 1; x++) {
                    int i = y * WIDTH + x;
                    float thermalRate = baseRate * 1.5f;

                    grid[z][i].temp = (prev_grid[z][i].temp + thermalRate * (grid[z][i - 1].temp + grid[z][i + 1].temp + grid[z][i - WIDTH].temp + grid[z][i + WIDTH].temp)) / (1 + 4 * thermalRate);
                    grid[z][i].charge = (prev_grid[z][i].charge + thermalRate * (grid[z][i - 1].charge + grid[z][i + 1].charge + grid[z][i - WIDTH].charge + grid[z][i + WIDTH].charge)) / (1 + 4 * thermalRate);

                    if (grid[z][i].permanent) continue;

                    float spread = fmaxf(0.0f, 1.0f - (fabsf(grid[z][i].cohesion) / 100.0f));
                    float rate = baseRate * spread;
                    if (spread > 0.05f) {
                        grid[z][i].density = (prev_grid[z][i].density + rate * (grid[z][i - 1].density + grid[z][i + 1].density + grid[z][i - WIDTH].density + grid[z][i + WIDTH].density)) / (1 + 4 * rate);
                        grid[z][i].moisture = (prev_grid[z][i].moisture + rate * (grid[z][i - 1].moisture + grid[z][i + 1].moisture + grid[z][i - WIDTH].moisture + grid[z][i + WIDTH].moisture)) / (1 + 4 * rate);
                    }
                }
            }
        }
    }
}

void UpdateSimulation(float dt, Player players[], int playerCount) {
    if (playerCount < 1) return;

    memcpy(prev_grid, grid, sizeof(grid));
    Diffuse(dt);

    for (int p = 0; p < playerCount; p++) players[p].animTime += dt;

    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        for (int z = 0; z < 2; z++) {
            if (grid[z][i].temp > 20.0f) grid[z][i].temp *= 0.99f;
            if (grid[z][i].temp < 20.0f) grid[z][i].temp += 0.1f;

            grid[z][i].charge *= 0.92f;
            grid[z][i].velocity.x *= 0.95f;
            grid[z][i].velocity.y *= 0.95f;

            if (!grid[z][i].permanent) {
                grid[z][i].moisture *= 0.99f;
                if (grid[z][i].temp > 80.0f && grid[z][i].cohesion > 30.0f) grid[z][i].cohesion -= 0.5f;
                if (grid[z][i].temp < 0.0f && grid[z][i].cohesion < 90.0f && grid[z][i].moisture > 10.0f) grid[z][i].cohesion += 1.0f;
                if (grid[z][i].density > 300.0f) grid[z][i].density -= 10.0f * dt;
            }

            if (fabsf(grid[z][i].velocity.x) > 1.0f || fabsf(grid[z][i].velocity.y) > 1.0f) {
                int nextX = (i % WIDTH) + (grid[z][i].velocity.x > 0 ? 1 : -1);
                int nextY = (i / WIDTH) + (grid[z][i].velocity.y > 0 ? 1 : -1);
                if (nextX >= 0 && nextX < WIDTH && nextY >= 0 && nextY < HEIGHT) {
                    int nextI = nextY * WIDTH + nextX;
                    if (!grid[z][nextI].permanent) {
                        float advect = 0.15f;
                        grid[z][nextI].density += grid[z][i].density * advect;
                        grid[z][i].density *= (1.0f - advect);

                        float heatDiff = grid[z][i].temp - 20.0f;
                        if (heatDiff > 0) {
                            grid[z][nextI].temp += heatDiff * advect;
                            grid[z][i].temp -= heatDiff * advect;
                        }

                        grid[z][nextI].charge += grid[z][i].charge * advect;
                        grid[z][i].charge *= (1.0f - advect);
                    }
                }
            }
        }

        if (grid[LAYER_AIR][i].density > 20.0f && grid[LAYER_AIR][i].cohesion > 20.0f) {
            grid[LAYER_GROUND][i].temp += grid[LAYER_AIR][i].density * 0.5f;
            grid[LAYER_GROUND][i].density += grid[LAYER_AIR][i].density;
            grid[LAYER_GROUND][i].cohesion = fmaxf(grid[LAYER_GROUND][i].cohesion, grid[LAYER_AIR][i].cohesion);
            grid[LAYER_AIR][i].density *= 0.1f;
        }

        if (grid[LAYER_GROUND][i].temp > 60.0f && grid[LAYER_GROUND][i].density < 15.0f && grid[LAYER_GROUND][i].cohesion < 10.0f) {
            grid[LAYER_AIR][i].temp += grid[LAYER_GROUND][i].temp * 0.1f;
            grid[LAYER_AIR][i].density += grid[LAYER_GROUND][i].density * 0.1f;
            grid[LAYER_GROUND][i].temp *= 0.9f;
            grid[LAYER_GROUND][i].density *= 0.9f;
        }

        if (grid[LAYER_AIR][i].density < 10.0f && grid[LAYER_AIR][i].cohesion < 10.0f) {
            grid[LAYER_AIR][i].density *= 0.95f;
        }

        if (grid[LAYER_GROUND][i].density > 300.0f) {
            if (grid[LAYER_GROUND][i].cohesion > 150.0f) {
                SpawnOrFeedSingularityAtCell(i, 1, grid[LAYER_GROUND][i].density, grid[LAYER_GROUND][i].charge);
            } else if (grid[LAYER_GROUND][i].cohesion < -150.0f) {
                SpawnOrFeedSingularityAtCell(i, 2, grid[LAYER_GROUND][i].density, grid[LAYER_GROUND][i].charge);
            }
        }
    }

    UpdateSingularityLifecycle(dt);
    PairEntangledSingularities();
    sys_sigCount = CountActiveSingularities();

    for (int sIdx = 0; sIdx < MAX_SINGULARITIES; sIdx++) {
        if (!sys_singularities[sIdx].active) continue;

        Singularity *sig = &sys_singularities[sIdx];
        float signedStrength = (sig->type == 2) ? -sig->strength : sig->strength;
        int targetWH = sig->linkedTo;

        for (int pIdx = 0; pIdx < playerCount; pIdx++) {
            Player *pl = &players[pIdx];
            if (pl->health <= 0.0f) continue;

            float dx = pl->pos.x - sig->pos.x;
            float dy = pl->pos.y - sig->pos.y;
            float distSq = dx * dx + dy * dy;
            if (distSq > 0.1f && distSq < 250000.0f) {
                float dist = sqrtf(distSq);
                float force = signedStrength / (distSq * 0.02f + 1.0f);
                pl->pos.x -= (dx / dist) * force * dt;
                pl->pos.y -= (dy / dist) * force * dt;

                if (sig->type == 1 && dist < sig->radius * 0.8f) {
                    if (targetWH != -1 && targetWH < MAX_SINGULARITIES && sys_singularities[targetWH].active) {
                        Vector2 exitN = { dx / dist, dy / dist };
                        pl->pos = (Vector2){
                            sys_singularities[targetWH].pos.x + exitN.x * sys_singularities[targetWH].radius,
                            sys_singularities[targetWH].pos.y + exitN.y * sys_singularities[targetWH].radius
                        };
                        pl->zVelocity *= -1.0f;
                    } else {
                        pl->health -= 1000.0f;
                        grid[LAYER_GROUND][sig->index].density += 50.0f;
                    }
                }
            }
        }

        for (int n = 0; n < MAX_NPCS; n++) {
            if (!active_npcs[n].active) continue;

            float ndx = active_npcs[n].pos.x - sig->pos.x;
            float ndy = active_npcs[n].pos.y - sig->pos.y;
            float ndSq = ndx * ndx + ndy * ndy;
            if (ndSq > 0.1f && ndSq < 250000.0f) {
                float nd = sqrtf(ndSq);
                float force = signedStrength / (ndSq * 0.02f + 1.0f);
                active_npcs[n].pos.x -= (ndx / nd) * force * dt;
                active_npcs[n].pos.y -= (ndy / nd) * force * dt;

                if (sig->type == 1 && nd < sig->radius * 0.8f) {
                    if (targetWH != -1 && targetWH < MAX_SINGULARITIES && sys_singularities[targetWH].active) {
                        Vector2 exitN = { ndx / nd, ndy / nd };
                        active_npcs[n].pos = (Vector2){
                            sys_singularities[targetWH].pos.x + exitN.x * sys_singularities[targetWH].radius,
                            sys_singularities[targetWH].pos.y + exitN.y * sys_singularities[targetWH].radius
                        };
                    } else {
                        active_npcs[n].health -= 1000.0f;
                        grid[LAYER_GROUND][sig->index].density += active_npcs[n].dna.mass;
                    }
                }
            }
        }

        for (int pr = 0; pr < MAX_PROJECTILES; pr++) {
            if (!projectiles[pr].active) continue;

            float pdx = projectiles[pr].pos.x - sig->pos.x;
            float pdy = projectiles[pr].pos.y - sig->pos.y;
            float pSq = pdx * pdx + pdy * pdy;
            if (pSq > 0.1f && pSq < 250000.0f) {
                float pd = sqrtf(pSq);
                float force = signedStrength / (pSq * 0.02f + 1.0f);
                projectiles[pr].pos.x -= (pdx / pd) * force * dt;
                projectiles[pr].pos.y -= (pdy / pd) * force * dt;

                if (sig->type == 1 && pd < sig->radius * 0.8f) {
                    if (targetWH != -1 && targetWH < MAX_SINGULARITIES && sys_singularities[targetWH].active) {
                        projectiles[pr].pos = sys_singularities[targetWH].pos;
                        projectiles[pr].basePos = projectiles[pr].pos;
                        projectiles[pr].velocity.x *= -1.5f;
                        projectiles[pr].velocity.y *= -1.5f;
                        projectiles[pr].baseVelocity.x *= -1.5f;
                        projectiles[pr].baseVelocity.y *= -1.5f;
                    } else {
                        projectiles[pr].active = false;
                        grid[LAYER_GROUND][sig->index].density += projectiles[pr].payload.density;
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        Projectile *pr = &projectiles[i];
        pr->flightTime += dt;
        float t = pr->flightTime;

        ResetCompiledDNA(&pr->payload);

        float currentSpeedMod = 1.0f;
        bool triggerCollisionSpread = false;

        for (int n = 0; n < MAX_NODES; n++) {
            SpellNode *node = &pr->payload.graph.nodes[n];
            if (!node->active) continue;
            if (!IsDescendant(&pr->payload.graph, n, pr->rootId)) continue;

            float delay = node->hasDelay ? node->delay : 0.0f;
            if (t < delay) continue;
            if (!EvaluateNodeCondition(node, &pr->payload, t)) continue;

            if (node->hasSpread && !node->triggered) {
                if (node->spreadType == SPREAD_INSTANT) {
                    node->triggered = true;
                    float baseAngle = atan2f(pr->velocity.y, pr->velocity.x);
                    for (int offset = -15; offset <= 15; offset += 15) {
                        float spreadAngle = baseAngle + (offset * PI / 180.0f);
                        Vector2 spreadTarget = {
                            pr->pos.x + cosf(spreadAngle) * 100.0f,
                            pr->pos.y + sinf(spreadAngle) * 100.0f
                        };
                        CastDynamicProjectile(pr->pos, spreadTarget, pr->layer, &pr->payload, n, pr->chargeMult, pr->lifeMult, pr->ownerId);
                    }
                    continue;
                }
                if (node->spreadType == SPREAD_COLLISION) {
                    triggerCollisionSpread = true;
                }
            }

            Vector2 outDir = {0};
            float resonance = 1.0f;
            float mag = GetNodeMagnitude(&pr->payload.graph, n, pr->rootId, &outDir, &resonance);

            if (node->detachOnCondition && !node->triggered) {
                node->triggered = true;
                Vector2 detachTarget = {
                    pr->pos.x + outDir.x * 120.0f + pr->velocity.x * 0.2f,
                    pr->pos.y + outDir.y * 120.0f + pr->velocity.y * 0.2f
                };
                CastDynamicProjectile(pr->pos, detachTarget, pr->layer, &pr->payload, n, pr->chargeMult, pr->lifeMult, pr->ownerId);
                continue;
            }

            ConsumeNodeToDNA(&pr->payload, node, mag, resonance, pr->chargeMult, pr->lifeMult, outDir);

            if (node->hasSpeed) {
                if (node->easeTime > 0.0f) {
                    float easeProg = fminf(1.0f, (t - delay) / node->easeTime);
                    float easeOut = 1.0f - (1.0f - easeProg) * (1.0f - easeProg);
                    currentSpeedMod *= 1.0f + (node->speedMod - 1.0f) * easeOut;
                } else {
                    currentSpeedMod *= node->speedMod;
                }
            }
        }

        ApplyChannelsToLegacyFields(&pr->payload);

        if (pr->rootId >= 0 && pr->rootId < MAX_NODES && pr->payload.graph.nodes[pr->rootId].hasDelay && t < pr->payload.graph.nodes[pr->rootId].delay) {
            pr->velocity = (Vector2){0};
        } else {
            pr->velocity.x = pr->baseVelocity.x * currentSpeedMod;
            pr->velocity.y = pr->baseVelocity.y * currentSpeedMod;
            pr->basePos.x += pr->velocity.x * dt;
            pr->basePos.y += pr->velocity.y * dt;
        }

        pr->life -= dt;
        pr->animOffset += dt * 15.0f;

        if (pr->payload.movement == MOVE_SIN || pr->payload.movement == MOVE_COS) {
            float len = sqrtf(pr->velocity.x * pr->velocity.x + pr->velocity.y * pr->velocity.y);
            if (len > 0.0001f) {
                float px = -pr->velocity.y / len;
                float py = pr->velocity.x / len;
                float amplitude = 15.0f + (pr->payload.moisture / 5.0f);
                float wave = (pr->payload.movement == MOVE_SIN) ? sinf(pr->animOffset) : cosf(pr->animOffset);
                pr->pos.x = pr->basePos.x + px * wave * amplitude;
                pr->pos.y = pr->basePos.y + py * wave * amplitude;
            }
        } else if (pr->payload.movement == MOVE_ORBIT && pr->ownerId >= 0 && pr->ownerId < playerCount) {
            float radius = 40.0f + (pr->payload.density / 2.0f);
            pr->pos.x = players[pr->ownerId].pos.x + cosf(pr->animOffset) * radius;
            pr->pos.y = players[pr->ownerId].pos.y - players[pr->ownerId].z + sinf(pr->animOffset) * radius;
            pr->basePos = pr->pos;
        } else {
            pr->pos = pr->basePos;
        }

        int gx = (int)(pr->pos.x / PIXEL_SIZE);
        int gy = (int)(pr->pos.y / PIXEL_SIZE);
        bool hitSolid = (gx >= 0 && gx < WIDTH && gy >= 0 && gy < HEIGHT) ? (grid[pr->layer][gy * WIDTH + gx].cohesion > 80.0f) : true;

        if (!hitSolid) {
            for (int j = 0; j < MAX_NPCS; j++) {
                if (!active_npcs[j].active) continue;
                float dx = pr->pos.x - active_npcs[j].pos.x;
                float dy = pr->pos.y - active_npcs[j].pos.y;
                if (dx * dx + dy * dy < 400.0f) {
                    hitSolid = true;
                    active_npcs[j].health -= pr->payload.density + pr->payload.temp;
                    break;
                }
            }
        }

        if (!hitSolid) {
            for (int pIdx = 0; pIdx < playerCount; pIdx++) {
                bool allowFriendlyHit = (pIdx != pr->ownerId) || players[pIdx].friendlyFire;
                if (!allowFriendlyHit) continue;
                if (players[pIdx].health <= 0.0f) continue;

                float dx = pr->pos.x - players[pIdx].pos.x;
                float dy = pr->pos.y - players[pIdx].pos.y;
                if ((pr->maxLife - pr->life > 0.1f) && (dx * dx + dy * dy < 200.0f)) {
                    hitSolid = true;
                    players[pIdx].health -= pr->payload.density + pr->payload.temp;
                    break;
                }
            }
        }

        if (pr->life <= 0.0f || hitSolid) {
            if (triggerCollisionSpread) {
                for (int n = 0; n < MAX_NODES; n++) {
                    SpellNode *cNode = &pr->payload.graph.nodes[n];
                    if (!cNode->active) continue;
                    if (!IsDescendant(&pr->payload.graph, n, pr->rootId)) continue;
                    if (!cNode->hasSpread || cNode->spreadType != SPREAD_COLLISION) continue;

                    for (int k = 0; k < 3; k++) {
                        Vector2 randTarget = {
                            pr->pos.x + GetRandomValue(-100, 100),
                            pr->pos.y + GetRandomValue(-100, 100)
                        };
                        CastDynamicProjectile(pr->pos, randTarget, pr->layer, &pr->payload, n, pr->chargeMult, pr->lifeMult, pr->ownerId);
                    }
                }
            }

            int radius = (int)(fabsf(pr->payload.density) / 20.0f * pr->payload.sizeMod) + 1;
            InjectEnergyArea(gx, gy, pr->layer, radius, pr->payload);
            ApplyToolEffect(gx, gy, pr->layer, &pr->payload);
            pr->active = false;
        }
    }

    for (int pIdx = 0; pIdx < playerCount; pIdx++) {
        int px = (int)(players[pIdx].pos.x / PIXEL_SIZE);
        int py = (int)(players[pIdx].pos.y / PIXEL_SIZE);
        if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT || players[pIdx].z >= 5.0f) continue;

        Cell g = grid[LAYER_GROUND][py * WIDTH + px];
        if (g.temp > 60.0f) players[pIdx].health -= (g.temp - 60.0f) * 0.5f * dt;
        if (g.temp < -10.0f) players[pIdx].health -= fabsf(g.temp + 10.0f) * 0.5f * dt;

        float momentum = g.density * sqrtf(g.velocity.x * g.velocity.x + g.velocity.y * g.velocity.y);
        if (momentum > 150.0f) players[pIdx].health -= (momentum - 150.0f) * 0.1f * dt;
        if (g.charge > 40.0f) players[pIdx].health -= g.charge * 0.2f * dt;
    }
}

void ExecuteSpell(Player *p, Vector2 target, SpellDNA *dna, float chargeMult, float lifeMult) {
    if (!dna->graph.nodes[0].active) return;

    SpellDNA immediate = *dna;
    ResetCompiledDNA(&immediate);

    for (int n = 0; n < MAX_NODES; n++) {
        if (!dna->graph.nodes[n].active) continue;

        SpellNode node = dna->graph.nodes[n];
        if (!EvaluateNodeCondition(&node, &immediate, 0.0f)) continue;

        Vector2 dir = {0};
        float resonance = 1.0f;
        float mag = GetNodeMagnitude(&dna->graph, n, 0, &dir, &resonance);
        ConsumeNodeToDNA(&immediate, &node, mag, resonance, chargeMult, lifeMult, dir);
    }

    ApplyChannelsToLegacyFields(&immediate);

    int gx = (int)(target.x / PIXEL_SIZE);
    int gy = (int)(target.y / PIXEL_SIZE);
    int px = (int)(p->pos.x / PIXEL_SIZE);
    int py = (int)(p->pos.y / PIXEL_SIZE);
    int radius = (int)((1.0f + chargeMult) * fmaxf(0.5f, immediate.sizeMod));
    int ownerId = (p) ? p->id : 0;

    switch (dna->form) {
        case FORM_PROJECTILE:
            CastDynamicProjectile(p->pos, target, p->castLayer, dna, 0, chargeMult, lifeMult, ownerId);
            break;
        case FORM_MANIFEST:
            InjectEnergyArea(gx, gy, p->castLayer, radius + 1, immediate);
            ApplyToolEffect(gx, gy, p->castLayer, &immediate);
            break;
        case FORM_AURA:
            InjectEnergyArea(px, py, p->castLayer, radius + 3, immediate);
            ApplyToolEffect(px, py, p->castLayer, &immediate);
            break;
        case FORM_BEAM:
            InjectBeam(p->pos, target, p->castLayer, immediate);
            break;
        default:
            break;
    }
}

void CastDynamicProjectile(Vector2 start, Vector2 target, int layer, SpellDNA *dna, int rootId, float chargeMult, float lifeMult, int ownerId) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) continue;

        if (rootId < 0 || rootId >= MAX_NODES || !dna->graph.nodes[rootId].active) rootId = 0;

        projectiles[i].pos = start;
        projectiles[i].basePos = start;
        projectiles[i].payload = *dna;
        projectiles[i].rootId = rootId;
        projectiles[i].chargeMult = chargeMult;
        projectiles[i].lifeMult = lifeMult;
        projectiles[i].ownerId = ownerId;
        projectiles[i].layer = layer;

        for (int n = 0; n < MAX_NODES; n++) projectiles[i].payload.graph.nodes[n].triggered = false;

        ResetCompiledDNA(&projectiles[i].payload);

        SpellNode root = dna->graph.nodes[rootId];
        float baseLife = (root.movement == MOVE_ORBIT) ? 10.0f : 1.5f;

        float accumulatedRange = 1.0f;
        for (int n = 0; n < MAX_NODES; n++) {
            if (dna->graph.nodes[n].active && dna->graph.nodes[n].hasRange && IsDescendant(&dna->graph, n, rootId)) {
                accumulatedRange *= dna->graph.nodes[n].rangeMod;
            }
        }

        projectiles[i].life = baseLife * accumulatedRange * lifeMult;
        projectiles[i].maxLife = projectiles[i].life;
        projectiles[i].flightTime = 0.0f;
        projectiles[i].active = true;
        projectiles[i].animOffset = 0.0f;

        float angle = atan2f(target.y - start.y, target.x - start.x);
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        Vector2 localV = {0};
        Vector2 dummyDir = {0};
        float resonance = 1.0f;

        for (int n = 0; n < MAX_NODES; n++) {
            if (!dna->graph.nodes[n].active || !IsDescendant(&dna->graph, n, rootId)) continue;

            SpellNode tempNode = dna->graph.nodes[n];
            SyncNodeLegacyScalars(&tempNode);
            float mag = GetNodeMagnitude(&dna->graph, n, rootId, &dummyDir, &resonance);
            float force = (fabsf(tempNode.scalarAdd[SCALAR_DENSITY]) + fabsf(tempNode.scalarAdd[SCALAR_TEMP])) * mag * resonance;
            localV.x += dummyDir.x * force;
            localV.y += dummyDir.y * force;
        }

        if (fabsf(localV.x) < 10.0f && fabsf(localV.y) < 10.0f) localV.x = 50.0f;

        float rotatedVx = localV.x * cosA - localV.y * sinA;
        float rotatedVy = localV.x * sinA + localV.y * cosA;
        float launchSpeed = 300.0f + (chargeMult * 40.0f);

        projectiles[i].baseVelocity = (Vector2){ (cosA * launchSpeed) + rotatedVx, (sinA * launchSpeed) + rotatedVy };
        projectiles[i].velocity = projectiles[i].baseVelocity;
        return;
    }
}

void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna) {
    float dx = target.x - start.x;
    float dy = target.y - start.y;
    float dist = sqrtf(dx * dx + dy * dy) * dna.rangeMod;
    if (dist <= 0.01f) return;

    float steps = dist / PIXEL_SIZE;
    if (steps <= 0.001f) return;

    float xInc = (dx / dist) * PIXEL_SIZE;
    float yInc = (dy / dist) * PIXEL_SIZE;
    float cx = start.x;
    float cy = start.y;

    for (int i = 0; i <= (int)steps; i++) {
        int gx = (int)(cx / PIXEL_SIZE);
        int gy = (int)(cy / PIXEL_SIZE);
        InjectEnergy(gx, gy, z, dna);
        ApplyToolEffect(gx, gy, z, &dna);
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
    grid[z][i].cohesion = (grid[z][i].cohesion + dna.cohesion) * 0.5f;
    grid[z][i].charge += dna.charge;
    grid[z][i].velocity.x += dna.velocity.x;
    grid[z][i].velocity.y += dna.velocity.y;
}

void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) InjectEnergy(cx + dx, cy + dy, z, dna);
        }
    }
}
