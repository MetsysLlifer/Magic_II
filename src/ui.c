#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

void DeleteNodeRec(SigilGraph *graph, int id) {
    graph->nodes[id].active = false;
    for(int i = 1; i < MAX_NODES; i++) if(graph->nodes[i].active && graph->nodes[i].parentId == id) DeleteNodeRec(graph, i);
}

void DrawCompositeSigil(Vector2 center, SigilGraph *graph, float scale, float rot, float animOffset) {
    if (!graph->nodes[0].active) return;
    Vector2 corePos = graph->nodes[0].pos;
    float maxDist = 20.0f; 
    
    for(int i=1; i<MAX_NODES; i++) {
        if(graph->nodes[i].active) {
            float d = sqrtf(powf(graph->nodes[i].pos.x - corePos.x, 2) + powf(graph->nodes[i].pos.y - corePos.y, 2));
            if (d > maxDist) maxDist = d;
        }
    }
    float mapScale = scale / maxDist;
    
    for(int i=1; i<MAX_NODES; i++) {
        if(graph->nodes[i].active && graph->nodes[i].parentId != -1) {
            int pId = graph->nodes[i].parentId;
            if (pId >= 0 && pId < MAX_NODES && graph->nodes[pId].active) {
                Vector2 p1 = graph->nodes[i].pos; Vector2 p2 = graph->nodes[pId].pos;
                Vector2 r1 = { p1.x - corePos.x, p1.y - corePos.y }; Vector2 r2 = { p2.x - corePos.x, p2.y - corePos.y };
                Vector2 f1 = { center.x + (r1.x*cosf(rot) - r1.y*sinf(rot))*mapScale, center.y + (r1.x*sinf(rot) + r1.y*cosf(rot))*mapScale };
                Vector2 f2 = { center.x + (r2.x*cosf(rot) - r2.y*sinf(rot))*mapScale, center.y + (r2.x*sinf(rot) + r2.y*cosf(rot))*mapScale };
                DrawLineEx(f1, f2, fmaxf(1.0f, scale*0.1f), Fade(SKYBLUE, 0.6f));
            }
        }
    }
    
    for(int i=0; i<MAX_NODES; i++) {
        if(graph->nodes[i].active) {
            Vector2 rel = { graph->nodes[i].pos.x - corePos.x, graph->nodes[i].pos.y - corePos.y };
            Vector2 fPos = { center.x + (rel.x*cosf(rot) - rel.y*sinf(rot))*mapScale, center.y + (rel.x*sinf(rot) + rel.y*cosf(rot))*mapScale };
            float nodeScale = (i == 0) ? scale * 0.6f : scale * 0.4f;
            DrawNodeSigil(fPos, graph->nodes[i], nodeScale, animOffset);
        }
    }
}

void DrawNodeSigil(Vector2 center, SpellNode node, float scale, float animOffset) {
    int points = 3 + (int)(node.charge / 15.0f); 
    if (points > 10) points = 10; if (points < 3) points = 3;
    
    float sizeToUse = node.hasSize ? node.sizeMod : 1.0f;
    float baseRadius = scale * (0.5f + (node.density / 100.0f)) * sizeToUse; 
    float spikeFactor = (node.temp / 100.0f) * scale; 
    float waveFactor = (node.moisture / 100.0f); 

    Color baseColor = (node.charge > 50.0f) ? PURPLE : (node.temp > 50.0f) ? RED : (node.moisture > 50.0f) ? SKYBLUE : GOLD;

    Vector2 pLast = {0};
    for (int i = 0; i <= points; i++) {
        float angle = (i * (PI * 2.0f)) / points + animOffset; 
        float r = baseRadius + (i % 2 == 0 ? spikeFactor : -spikeFactor * 0.5f);
        if (node.hasDistort && node.distortion > 0) r += sinf(animOffset * 10.0f + i) * (node.distortion / 20.0f);

        Vector2 p = { center.x + cosf(angle) * r, center.y + sinf(angle) * r };
        if (i > 0) {
            DrawLineEx(pLast, p, 2.0f, baseColor); 
            if (waveFactor > 0.1f) {
                Vector2 mid = { center.x + cosf(angle - PI) * (r * waveFactor), center.y + sinf(angle - PI) * (r * waveFactor) };
                DrawLineEx(p, mid, 1.0f, Fade(SKYBLUE, 0.6f));
            }
        }
        pLast = p;
    }
    DrawCircle(center.x, center.y, scale * (node.cohesion / 100.0f) * 0.4f, RAYWHITE);
}

void DrawChargeSparks(int x, int y, int z, float charge, float density, float alpha) {
    if (charge < 5.0f || alpha <= 0.0f) return;
    float t = GetTime() * 20.0f; 
    int yOffset = (z == LAYER_AIR) ? FLOAT_OFFSET : 0;
    if (z == LAYER_GROUND && density > 60.0f) yOffset += (int)(density / 10.0f);
    Vector2 core = { x * PIXEL_SIZE + (PIXEL_SIZE / 2.0f), (y * PIXEL_SIZE) - yOffset + (PIXEL_SIZE / 2.0f) };
    float bound = PIXEL_SIZE * 0.8f; 
    for (int i = 0; i < 2; i++) {
        float angle1 = t + (x * 0.3f) + (y * 0.7f) + (i * PI);
        float angle2 = -t + (x * 0.8f) - (y * 0.4f) + (i * PI);
        float r1 = bound * fmodf(fabsf(sinf(t * 0.5f + i)), 1.0f);
        float r2 = bound * fmodf(fabsf(cosf(t * 0.5f + i)), 1.0f);
        Vector2 p1 = { core.x + cosf(angle1) * r1, core.y + sinf(angle1) * r1 };
        Vector2 p2 = { core.x + cosf(angle2) * r2, core.y + sinf(angle2) * r2 };
        unsigned char opacity = (unsigned char)(fmin(255, charge * 5) * alpha);
        DrawLineEx(p1, p2, 1.0f, (Color){220, 150, 255, opacity});
    }
}

Color GetCellColor(Cell c) {
    if (c.density < 2.0f && c.temp < 30.0f && c.charge < 10.0f) return BLANK;
    if (c.density > 40.0f && c.cohesion > 60.0f) return (c.temp < 0.0f) ? SKYBLUE : DARKGRAY;
    else if (c.density > 20.0f && c.cohesion <= 60.0f) {
        if (c.temp > 70.0f) return ORANGE; else if (c.moisture > 30.0f) return BLUE; else return BROWN;                        
    } else {
        if (c.temp > 70.0f) return RED; else if (c.charge > 40.0f) return PURPLE; else return Fade(LIGHTGRAY, c.density / 20.0f); 
    }
}

void DrawMaterialRealm(float alpha) {
    if (alpha <= 0.0f) return;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell c = grid[LAYER_GROUND][y * WIDTH + x];
            Color color = Fade(GetCellColor(c), alpha); 
            if (color.a > 0) {
                if (c.density > 60.0f && c.cohesion > 80.0f) {
                    int heightOffset = (int)(c.density / 10.0f); 
                    DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE + heightOffset, ColorBrightness(color, -0.4f));
                    DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE - heightOffset, PIXEL_SIZE, PIXEL_SIZE, ColorBrightness(color, 0.2f));
                } else { DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, color); }
            }
            DrawChargeSparks(x, y, LAYER_GROUND, c.charge, c.density, alpha);
        }
    }
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell airCell = grid[LAYER_AIR][y * WIDTH + x];
            if (airCell.density > 5.0f || airCell.temp > 40.0f) {
                DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, Fade(BLACK, 0.4f * alpha)); 
                Color airColor = Fade(GetCellColor(airCell), alpha);
                if (airColor.a > 0) DrawRectangle(x * PIXEL_SIZE, (y * PIXEL_SIZE) - FLOAT_OFFSET, PIXEL_SIZE, PIXEL_SIZE, airColor);
            }
            DrawChargeSparks(x, y, LAYER_AIR, airCell.charge, airCell.density, alpha);
        }
    }
}

void DrawEnergyRealm(float alpha) {
    if (alpha <= 0.0f) return;
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                Cell c = grid[z][y * WIDTH + x];
                if (c.density < 0.1f && fabsf(c.temp) < 0.1f && c.charge < 0.1f) continue;
                unsigned char r = (unsigned char)fmin(255, fmax(0, c.temp * 10));
                unsigned char g = (unsigned char)fmin(255, fmax(0, c.charge * 15));
                unsigned char b = (unsigned char)fmin(255, fmax(0, c.density * 10));
                int yOffset = (z == LAYER_AIR) ? FLOAT_OFFSET : 0;
                DrawRectangle(x * PIXEL_SIZE, (y * PIXEL_SIZE) - yOffset, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, (unsigned char)(150 * alpha)});
                DrawChargeSparks(x, y, z, c.charge, c.density, alpha);
            }
        }
    }
}

void DrawHazardRealm(float alpha) {
    if (alpha <= 0.0f) return;
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                Cell c = grid[z][y * WIDTH + x];
                float heatHarm = fmaxf(0.0f, c.temp - 60.0f) / 50.0f; 
                float coldHarm = fmaxf(0.0f, -10.0f - c.temp) / 50.0f;
                float momentum = c.density * sqrtf(c.velocity.x*c.velocity.x + c.velocity.y*c.velocity.y);
                float kinHarm = fmaxf(0.0f, momentum - 150.0f) / 100.0f;
                float shockHarm = fmaxf(0.0f, c.charge - 40.0f) / 50.0f;
                
                float totalHarm = heatHarm + coldHarm + kinHarm + shockHarm;
                if (totalHarm <= 0.0f) continue;

                unsigned char r = (unsigned char)fmin(255, (heatHarm + kinHarm + shockHarm) * 255);
                unsigned char g = (unsigned char)fmin(255, (kinHarm) * 255);
                unsigned char b = (unsigned char)fmin(255, (coldHarm + shockHarm) * 255);
                
                float pulse = (sinf(GetTime() * 15.0f) + 1.0f) * 0.5f;
                unsigned char finalAlpha = (unsigned char)(fminf(255, totalHarm * 150 + (pulse * 100)) * alpha);

                int yOffset = (z == LAYER_AIR) ? FLOAT_OFFSET : 0;
                DrawRectangle(x * PIXEL_SIZE, (y * PIXEL_SIZE) - yOffset, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, finalAlpha});
            }
        }
    }
    for(int i=0; i<MAX_NPCS; i++) {
        if(active_npcs[i].active && active_npcs[i].dna.hostility > 50.0f) {
            Vector2 pCenter = {active_npcs[i].pos.x, active_npcs[i].pos.y - active_npcs[i].z};
            float pulse = (sinf(GetTime() * 10.0f) + 1.0f) * 0.5f;
            DrawCircleLines(pCenter.x, pCenter.y, 10.0f + (pulse * 5.0f), Fade(RED, alpha));
            DrawText("! HOSTILE !", pCenter.x - 25, pCenter.y - 20, 10, Fade(RED, alpha));
        }
    }
}

// BEAUTIFUL BLACK HOLE & WHITE HOLE VISUALIZER
void DrawSingularities(float alpha) {
    for(int i=0; i<MAX_SINGULARITIES; i++) {
        if (!sys_singularities[i].active) continue;
        Singularity s = sys_singularities[i];
        float t = s.anim * 10.0f;
        float radius = s.radius;
        
        if (s.type == 1) { // BLACK HOLE
            DrawCircle(s.pos.x, s.pos.y, radius * 0.6f, Fade(BLACK, alpha));
            for(int r=0; r<5; r++) {
                float angle = t + (r * PI / 2.5f);
                DrawLineEx(s.pos, (Vector2){s.pos.x + cosf(angle)*radius, s.pos.y + sinf(angle)*radius}, 3.0f, Fade(PURPLE, alpha * 0.8f));
                DrawCircleLines(s.pos.x, s.pos.y, radius - (fmodf(t + r*5, radius)), Fade(DARKPURPLE, alpha)); // Spiraling IN
            }
        } else { // WHITE HOLE
            DrawCircle(s.pos.x, s.pos.y, radius * 0.4f, Fade(RAYWHITE, alpha));
            for(int r=0; r<5; r++) {
                float angle = -t + (r * PI / 2.5f);
                DrawLineEx(s.pos, (Vector2){s.pos.x + cosf(angle)*radius*1.5f, s.pos.y + sinf(angle)*radius*1.5f}, 2.0f, Fade(SKYBLUE, alpha * 0.8f));
                DrawCircleLines(s.pos.x, s.pos.y, fmodf(t + r*5, radius*1.5f), Fade(RAYWHITE, alpha)); // Spiraling OUT
            }
        }
        
        // ENTANGLEMENT TETHER (Quantum Link)
        if (s.linkedTo != -1 && s.linkedTo < MAX_SINGULARITIES && sys_singularities[s.linkedTo].active && i < s.linkedTo) { // Draw once per pair
            Vector2 p2 = sys_singularities[s.linkedTo].pos;
            float dist = sqrtf(powf(p2.x - s.pos.x, 2) + powf(p2.y - s.pos.y, 2));
            float phase = t * 5.0f;
            int segments = (int)(dist / 10.0f);
            for(int k=0; k<segments; k++) {
                float lerp = (float)k / segments;
                Vector2 mid = { s.pos.x + (p2.x - s.pos.x)*lerp, s.pos.y + (p2.y - s.pos.y)*lerp };
                mid.y += sinf(phase + k) * 5.0f; // Wavy tether
                DrawCircle(mid.x, mid.y, 2.0f, Fade(GOLD, alpha * 0.7f));
            }
            DrawText("ENTANGLED", s.pos.x - 20, s.pos.y - radius - 15, 10, Fade(GOLD, alpha));
        }
    }
}

void DrawProjectiles(Player *p) {
    (void)p;
    for(int i=0; i<MAX_PROJECTILES; i++) {
        if(projectiles[i].active) {
            int yOffset = (projectiles[i].layer == LAYER_AIR) ? FLOAT_OFFSET : 0;
            Vector2 pCenter = {projectiles[i].pos.x, projectiles[i].pos.y - yOffset};
            
            float rot = atan2f(projectiles[i].velocity.y, projectiles[i].velocity.x) + (projectiles[i].payload.movement == MOVE_ORBIT ? projectiles[i].animOffset : 0);
            float size = (4.0f + (projectiles[i].payload.density / 20.0f)) * projectiles[i].payload.sizeMod;
            
            DrawCompositeSigil(pCenter, &projectiles[i].payload.graph, size, rot, projectiles[i].animOffset);
            if (projectiles[i].layer == LAYER_AIR) DrawEllipse(projectiles[i].pos.x, projectiles[i].pos.y, 4, 2, Fade(BLACK, 0.5f));
        }
    }
}

void DrawPlayerEntity(Player *p) {
    if (p->health <= 0) return;
    DrawEllipse(p->pos.x, p->pos.y, 6, 3, Fade(BLACK, 0.6f));
    
    float stretch = fmaxf(0.1f, 1.0f + (p->zVelocity / 1000.0f)); 
    float squash = 1.0f / stretch;
    float breathe = (sinf(p->animTime * 4.0f) + 1.0f) * 0.5f;
    float currentWidth = (6.0f * squash) + (breathe * 1.5f);
    float currentHeight = 6.0f * stretch;

    DrawEllipse(p->pos.x, p->pos.y - p->z, currentWidth, currentHeight, RAYWHITE); 

    // DUAL CHANNEL CHARGING UI (Intensity vs Lifespan)
    Vector2 cCenter = {p->pos.x, p->pos.y - p->z};
    
    if (p->isCharging) {
        float radius = 10.0f + (p->chargeLevel / 4.0f) * 20.0f;
        float pulse = (sinf(p->animTime * 15.0f) + 1.0f) * 0.5f; 
        DrawCircleLines(cCenter.x, cCenter.y, radius + pulse, Fade(SKYBLUE, 0.8f));
        DrawText(TextFormat("PWR x%.1f", 1.0f + p->chargeLevel), cCenter.x + radius + 5, cCenter.y - 5, 10, SKYBLUE);
    }
    if (p->isLifespanCharging) {
        float radius = 15.0f + (p->lifespanLevel / 4.0f) * 20.0f;
        float pulse = (sinf(p->animTime * -10.0f) + 1.0f) * 0.5f; 
        DrawCircleLines(cCenter.x, cCenter.y, radius + pulse, Fade(GREEN, 0.8f));
        DrawText(TextFormat("LIFE x%.1f", 1.0f + p->lifespanLevel), cCenter.x + radius + 5, cCenter.y + 5, 10, GREEN);
    }
}

static void DrawSpellCompendiumOverlay(void) {
    DrawRectangle(85, 55, 630, 340, Fade(BLACK, 0.96f));
    DrawRectangleLines(85, 55, 630, 340, GOLD);
    DrawText("SPELL COMPENDIUM (F1)", 105, 65, 16, GOLD);

    int y = 92;
    DrawText("GEAR ABBREVIATIONS", 105, y, 12, SKYBLUE); y += 16;
    DrawText("SPD: Speed modifier | DLY: Delay gate | SPR: Spread trigger", 105, y, 10, RAYWHITE); y += 14;
    DrawText("DST: Distortion wave | RNG: Range multiplier | SIZ: Size multiplier", 105, y, 10, RAYWHITE); y += 14;
    DrawText("SPR MODES: OFF (none), INST (instant fork), COLL (fork on collision)", 105, y, 10, RAYWHITE); y += 18;

    DrawText("CONDITIONAL LOGIC", 105, y, 12, SKYBLUE); y += 16;
    DrawText("COND ALW: Always active", 105, y, 10, RAYWHITE); y += 14;
    DrawText("COND T>: Activates if flight time is above threshold", 105, y, 10, RAYWHITE); y += 14;
    DrawText("COND S>/S<: Activates if selected scalar is above/below threshold", 105, y, 10, RAYWHITE); y += 14;
    DrawText("CH TMP/MAS/WET/COH/CHG: Target scalar channel for S>/S<", 105, y, 10, RAYWHITE); y += 14;
    DrawText("DETACH: Spawns detached child-core when condition succeeds", 105, y, 10, RAYWHITE); y += 18;

    DrawText("TOOLCRAFT ABBREVIATIONS", 105, y, 12, SKYBLUE); y += 16;
    DrawText("BLD build | DIG excavate | WET moisten | DRY dehydrate", 105, y, 10, RAYWHITE); y += 14;
    DrawText("HOT heat | COO cool | CON conduct charge", 105, y, 10, RAYWHITE); y += 14;
    DrawText("BHO seed black hole | WHO seed white hole | PERSIST keeps effects", 105, y, 10, RAYWHITE); y += 18;

    DrawText("GLOBAL FORM", 105, y, 12, SKYBLUE); y += 16;
    DrawText("PRJ projectile | MNS manifest area | AUR aura around caster", 105, y, 10, RAYWHITE);
}

void DrawInterface(Player *p, NPCDNA *draftNPC, Vector2 virtualMouse) {
    if (IsKeyPressed(KEY_F1)) p->showCompendium = !p->showCompendium;

    DrawRectangle(10, 10, 200, 15, DARKGRAY);
    DrawRectangle(10, 10, (int)((fmax(0, p->health) / p->maxHealth) * 200), 15, RED);
    DrawRectangleLines(10, 10, 200, 15, WHITE);
    DrawText("VITALS", 15, 12, 10, WHITE);

    DrawRectangle(10, 35, 180, 150, Fade(BLACK, 0.8f));
    DrawRectangleLines(10, 35, 180, 150, GOLD);
    DrawText(TextFormat("ACTIVE SLOT: %d", p->activeSlot + 1), 15, 40, 10, GOLD);
    
    HotbarSlot *activeSlot = &p->hotbar[p->activeSlot];
    if (activeSlot->type == ITEM_SPELL) {
        DrawText("TYPE: ENERGY SCALAR", 15, 55, 10, SKYBLUE);
        SpellDNA dummy = activeSlot->spell;
        CompileSigilGraph(&dummy);
        DrawText(TextFormat("Temp: %.0f | Mass: %.0f", dummy.temp, dummy.density), 15, 70, 10, RAYWHITE);
        DrawText(TextFormat("Cohe: %.0f | Wet: %.0f", dummy.cohesion, dummy.moisture), 15, 85, 10, RAYWHITE);
        DrawText(TextFormat("Chrg: %.0f", dummy.charge), 15, 100, 10, RAYWHITE);
        DrawCompositeSigil((Vector2){100, 140}, &activeSlot->spell.graph, 25.0f, GetTime()*0.5f, GetTime());
    } else {
        DrawText("TYPE: BIO-MATRIX", 15, 55, 10, GREEN);
        DrawText(TextFormat("Mass: %.0f | Int: %.0f", activeSlot->npc.mass, activeSlot->npc.intelligence), 15, 70, 10, RAYWHITE);
        DrawText(TextFormat("Aero: %.0f | Hyd: %.0f", activeSlot->npc.aero, activeSlot->npc.hydro), 15, 85, 10, RAYWHITE);
        DrawText(TextFormat("Terr: %.0f | Hst: %.0f", activeSlot->npc.terrestrial, activeSlot->npc.hostility), 15, 100, 10, RAYWHITE);
        DrawProceduralNPC((Vector2){100, 150}, 0, activeSlot->npc, 1.0f); 
    }

    const char* vState = (p->visionBlend < 0.5f) ? "MAT" : (p->visionBlend < 1.5f) ? "NRG" : "HAZ";
    DrawText(TextFormat("VISION BLEND: %.1f [%s]", p->visionBlend, vState), 10, 195, 10, PURPLE);
    Color targetColor = (p->castLayer == LAYER_AIR) ? SKYBLUE : BROWN;
    DrawText((p->castLayer == LAYER_AIR) ? "Z-TARGET: AIR [SHIFT]" : "Z-TARGET: GROUND [SHIFT]", 10, 210, 12, targetColor);

    int startX = (SCREEN_W - (10 * 35)) / 2; 
    for(int i = 0; i < 10; i++) {
        Rectangle slotRec = { startX + (i * 35), 400, 30, 30 };
        DrawRectangleRec(slotRec, Fade(BLACK, 0.8f));
        Color bColor = (i == p->activeSlot) ? GOLD : DARKGRAY;
        DrawRectangleLinesEx(slotRec, (i == p->activeSlot) ? 2 : 1, bColor);
        DrawText(TextFormat("%d", i+1), slotRec.x + 3, slotRec.y + 2, 10, GRAY);
        
        if (p->hotbar[i].type == ITEM_SPELL) {
            DrawCompositeSigil((Vector2){slotRec.x + 15, slotRec.y + 10}, &p->hotbar[i].spell.graph, 8.0f, GetTime(), GetTime());
            const char* fStr = (p->hotbar[i].spell.form == FORM_PROJECTILE) ? "PRJ" :
                               (p->hotbar[i].spell.form == FORM_MANIFEST) ? "MNF" :
                               (p->hotbar[i].spell.form == FORM_AURA) ? "AUR" : "BEM";
            DrawText(fStr, slotRec.x + 6, slotRec.y + 18, 8, SKYBLUE);
        } else { DrawCircle(slotRec.x + 15, slotRec.y + 15, 5, GREEN); }
    }

    if (p->isCrafting && !p->showGuide) {
        DrawRectangle(30, 30, 740, 380, Fade(BLACK, 0.95f));
        DrawRectangleLines(30, 30, 740, 380, GOLD);
        
        DrawText("TAB: SWITCH ARCHITECTURE", 40, 40, 10, GRAY);
        Color spellTab = (p->craftCategory == 0) ? GOLD : DARKGRAY;
        Color npcTab = (p->craftCategory == 1) ? GREEN : DARKGRAY;
        DrawText("SPELL MATRIX", 280, 40, 15, spellTab);
        DrawText("BIO MATRIX", 430, 40, 15, npcTab);

        if (p->craftCategory == 0) {
            SpellDNA *activeSpell = &p->hotbar[p->activeSlot].spell;
            SigilGraph *activeGraph = &activeSpell->graph; 
            Rectangle canvasBounds = {230, 60, 420, 340}; 
            DrawLine(220, 60, 220, 400, DARKGRAY);
            Rectangle leftPanel = {36, 60, 184, 332};
            DrawRectangleLinesEx(leftPanel, 1.0f, Fade(SKYBLUE, 0.35f));
            if (GuiButton((Rectangle){668, 372, 90, 20}, p->showCompendium ? "HIDE INFO" : "COMPEND")) p->showCompendium = !p->showCompendium;

            BeginScissorMode((int)leftPanel.x, (int)leftPanel.y, (int)leftPanel.width, (int)leftPanel.height);
            DrawText("SCALARS & BEHAVIOR", 40, 65, 12, GOLD);
            if (p->selectedNodeId != -1 && activeGraph->nodes[p->selectedNodeId].active) {
                SpellNode *n = &activeGraph->nodes[p->selectedNodeId];
                DrawText(TextFormat("ID: %d", p->selectedNodeId), 40, 80, 10, SKYBLUE);

                int ySp = 95;
                GuiSlider((Rectangle){80, ySp, 78, 10}, "TEMP", NULL, &n->temp, -100, 200);
                int tV = (int)n->temp; if(GuiValueBox((Rectangle){162, ySp-2, 40, 14}, NULL, &tV, -100, 200, p->editStates[0])) p->editStates[0] = !p->editStates[0]; n->temp = tV; ySp+=18;

                GuiSlider((Rectangle){80, ySp, 78, 10}, "MASS", NULL, &n->density, 0, 100);
                int mV = (int)n->density; if(GuiValueBox((Rectangle){162, ySp-2, 40, 14}, NULL, &mV, 0, 100, p->editStates[1])) p->editStates[1] = !p->editStates[1]; n->density = mV; ySp+=18;

                GuiSlider((Rectangle){80, ySp, 78, 10}, "COHE", NULL, &n->cohesion, -200, 200);
                int cV = (int)n->cohesion; if(GuiValueBox((Rectangle){162, ySp-2, 40, 14}, NULL, &cV, -200, 200, p->editStates[2])) p->editStates[2] = !p->editStates[2]; n->cohesion = cV; ySp+=18;

                GuiSlider((Rectangle){80, ySp, 78, 10}, "WET", NULL, &n->moisture, 0, 100);
                int wV = (int)n->moisture; if(GuiValueBox((Rectangle){162, ySp-2, 40, 14}, NULL, &wV, 0, 100, p->editStates[3])) p->editStates[3] = !p->editStates[3]; n->moisture = wV; ySp+=18;

                GuiSlider((Rectangle){80, ySp, 78, 10}, "CHRG", NULL, &n->charge, 0, 100);
                int hV = (int)n->charge; if(GuiValueBox((Rectangle){162, ySp-2, 40, 14}, NULL, &hV, 0, 100, p->editStates[4])) p->editStates[4] = !p->editStates[4]; n->charge = hV;

                DrawLine(40, 190, 220, 190, DARKGRAY);

                DrawText("ATTACH BEHAVIOR GEARS:", 40, 195, 10, SKYBLUE);
                GuiToggle((Rectangle){40, 205, 26, 15}, "SPD", &n->hasSpeed);
                GuiToggle((Rectangle){69, 205, 26, 15}, "DLY", &n->hasDelay);
                GuiToggle((Rectangle){98, 205, 26, 15}, "SPR", &n->hasSpread);
                GuiToggle((Rectangle){127, 205, 26, 15}, "DST", &n->hasDistort);
                GuiToggle((Rectangle){156, 205, 26, 15}, "RNG", &n->hasRange);
                GuiToggle((Rectangle){185, 205, 26, 15}, "SIZ", &n->hasSize);

                int gY = 225;
                if(n->hasSpeed) {
                    GuiSlider((Rectangle){80, gY, 120, 10}, "SPEED", NULL, &n->speedMod, 0.1f, 3.0f); gY+=15;
                    GuiSlider((Rectangle){80, gY, 120, 10}, "EASE", NULL, &n->easeTime, 0.0f, 3.0f); gY+=15;
                }
                if(n->hasDelay) { GuiSlider((Rectangle){80, gY, 120, 10}, "DELAY", NULL, &n->delay, 0.0f, 5.0f); gY+=15; }
                if(n->hasDistort) { GuiSlider((Rectangle){80, gY, 120, 10}, "DSTRT", NULL, &n->distortion, 0.0f, 100.0f); gY+=15; }
                if(n->hasRange) { GuiSlider((Rectangle){80, gY, 120, 10}, "RANGE", NULL, &n->rangeMod, 0.1f, 5.0f); gY+=15; }
                if(n->hasSize) { GuiSlider((Rectangle){80, gY, 120, 10}, "SIZE", NULL, &n->sizeMod, 0.1f, 5.0f); gY+=15; }

                if(n->hasSpread) {
                    DrawText("SPREAD TYPE:", 40, gY, 10, WHITE);
                    if(GuiButton((Rectangle){40, gY+10, 42, 15}, "OFF")) n->spreadType = SPREAD_OFF;
                    if(GuiButton((Rectangle){86, gY+10, 42, 15}, "INST")) n->spreadType = SPREAD_INSTANT;
                    if(GuiButton((Rectangle){132, gY+10, 42, 15}, "COLL")) n->spreadType = SPREAD_COLLISION;
                    int sx = (n->spreadType == SPREAD_OFF) ? 40 : (n->spreadType == SPREAD_INSTANT) ? 86 : 132;
                    DrawRectangleLines(sx, gY+10, 42, 15, PURPLE);
                    gY += 30;
                }

                DrawText("COND:", 40, gY, 10, WHITE);
                if(GuiButton((Rectangle){40, gY, 40, 15}, "ALW")) n->conditionType = COND_ALWAYS;
                if(GuiButton((Rectangle){84, gY, 40, 15}, "T>")) n->conditionType = COND_FLIGHT_TIME_GT;
                if(GuiButton((Rectangle){128, gY, 40, 15}, "S>")) n->conditionType = COND_SCALAR_GT;
                if(GuiButton((Rectangle){172, gY, 40, 15}, "S<")) n->conditionType = COND_SCALAR_LT;
                int csel = (n->conditionType == COND_ALWAYS) ? 40 :
                           (n->conditionType == COND_FLIGHT_TIME_GT) ? 84 :
                           (n->conditionType == COND_SCALAR_GT) ? 128 : 172;
                DrawRectangleLines(csel, gY, 40, 15, GOLD);
                gY += 18;

                if(n->conditionType != COND_ALWAYS) {
                    if(n->conditionType == COND_FLIGHT_TIME_GT) {
                        GuiSlider((Rectangle){80, gY, 120, 10}, "TIME", NULL, &n->conditionThreshold, 0.0f, 8.0f);
                        gY += 15;
                    } else {
                        DrawText("CH:", 40, gY, 10, WHITE);
                        if(GuiButton((Rectangle){40, gY+10, 30, 14}, "TMP")) n->conditionChannel = SCALAR_TEMP;
                        if(GuiButton((Rectangle){74, gY+10, 30, 14}, "MAS")) n->conditionChannel = SCALAR_DENSITY;
                        if(GuiButton((Rectangle){108, gY+10, 30, 14}, "WET")) n->conditionChannel = SCALAR_MOISTURE;
                        if(GuiButton((Rectangle){142, gY+10, 30, 14}, "COH")) n->conditionChannel = SCALAR_COHESION;
                        if(GuiButton((Rectangle){176, gY+10, 30, 14}, "CHG")) n->conditionChannel = SCALAR_CHARGE;
                        int clampedCh = n->conditionChannel;
                        if (clampedCh < 0) clampedCh = 0;
                        if (clampedCh >= SCALAR_COUNT) clampedCh = SCALAR_COUNT - 1;
                        int cx = 40 + (clampedCh * 34);
                        DrawRectangleLines(cx, gY+10, 30, 14, SKYBLUE);
                        gY += 26;
                        GuiSlider((Rectangle){80, gY, 120, 10}, "THR", NULL, &n->conditionThreshold, -250.0f, 350.0f);
                        gY += 15;
                    }
                    GuiToggle((Rectangle){40, gY, 90, 15}, "DETACH", &n->detachOnCondition);
                    gY += 18;
                }

                GuiToggle((Rectangle){40, gY, 85, 15}, "TOOL", &n->hasTool);
                gY += 18;
                if(n->hasTool) {
                    DrawText("TOOL:", 40, gY, 10, WHITE);
                    if(GuiButton((Rectangle){40, gY+10, 30, 14}, "BLD")) n->toolType = TOOL_BUILD;
                    if(GuiButton((Rectangle){74, gY+10, 30, 14}, "DIG")) n->toolType = TOOL_DIG;
                    if(GuiButton((Rectangle){108, gY+10, 30, 14}, "WET")) n->toolType = TOOL_MOISTEN;
                    if(GuiButton((Rectangle){142, gY+10, 30, 14}, "DRY")) n->toolType = TOOL_DRY;
                    if(GuiButton((Rectangle){40, gY+26, 30, 14}, "HOT")) n->toolType = TOOL_HEAT;
                    if(GuiButton((Rectangle){74, gY+26, 30, 14}, "COO")) n->toolType = TOOL_COOL;
                    if(GuiButton((Rectangle){108, gY+26, 30, 14}, "CON")) n->toolType = TOOL_CONDUCT;
                    if(GuiButton((Rectangle){142, gY+26, 30, 14}, "BHO")) n->toolType = TOOL_SINGULARITY_BLACK;
                    if(GuiButton((Rectangle){176, gY+26, 30, 14}, "WHO")) n->toolType = TOOL_SINGULARITY_WHITE;
                    gY += 45;
                    GuiSlider((Rectangle){80, gY, 120, 10}, "PWR", NULL, &n->toolPower, 0.5f, 5.0f); gY += 15;
                    GuiSlider((Rectangle){80, gY, 120, 10}, "RAD", NULL, &n->toolRadius, 1.0f, 12.0f); gY += 15;
                    GuiToggle((Rectangle){40, gY, 100, 15}, "PERSIST", &n->toolPermanent); gY += 18;
                }

                int moveY = (gY > 345) ? gY : 345;
                DrawText("MOVE:", 40, moveY, 10, WHITE);
                if(GuiButton((Rectangle){40, moveY + 10, 35, 15}, "STR")) n->movement = MOVE_STRAIGHT;
                if(GuiButton((Rectangle){80, moveY + 10, 35, 15}, "SIN")) n->movement = MOVE_SIN;
                if(GuiButton((Rectangle){120, moveY + 10, 35, 15}, "COS")) n->movement = MOVE_COS;
                if(GuiButton((Rectangle){160, moveY + 10, 35, 15}, "ORB")) n->movement = MOVE_ORBIT;
                int mx = (n->movement == MOVE_STRAIGHT) ? 40 : (n->movement == MOVE_SIN) ? 80 : (n->movement == MOVE_COS) ? 120 : 160;
                DrawRectangleLines(mx, moveY + 10, 35, 15, GREEN);
            }
            EndScissorMode();

            DrawText("GLOBAL FORM:", 40, 395, 10, WHITE);
            if(GuiButton((Rectangle){110, 395, 30, 15}, "PRJ")) activeSpell->form = FORM_PROJECTILE;
            if(GuiButton((Rectangle){145, 395, 30, 15}, "MNS")) activeSpell->form = FORM_MANIFEST;
            if(GuiButton((Rectangle){180, 395, 30, 15}, "AUR")) activeSpell->form = FORM_AURA;
            int fx = (activeSpell->form == 0) ? 110 : (activeSpell->form == 1) ? 145 : 180;
            DrawRectangleLines(fx, 395, 30, 15, PURPLE);

            DrawLine(660, 60, 660, 400, DARKGRAY);
            DrawText("ANALYSIS", 670, 70, 12, GOLD);

            float maxTime = 1.5f; 
            for(int i=0; i<MAX_NODES; i++) {
                if(activeGraph->nodes[i].active && activeGraph->nodes[i].hasDelay) {
                    if(activeGraph->nodes[i].delay + 1.5f > maxTime) maxTime = activeGraph->nodes[i].delay + 1.5f;
                }
            }
            DrawText(TextFormat("MAX TIME: %.1fs", maxTime), 670, 100, 10, RAYWHITE);
            DrawText("String Resonance: ACTIVE", 670, 120, 10, PURPLE);

            DrawText("INFINITE COMPILER CANVAS", 260, 65, 15, GOLD);
            if (CheckCollisionPointRec(virtualMouse, canvasBounds)) {
                p->craftCamera.zoom += GetMouseWheelMove() * 0.1f;
                if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                    Vector2 delta = GetMouseDelta();
                    p->craftCamera.target.x -= delta.x / p->craftCamera.zoom;
                    p->craftCamera.target.y -= delta.y / p->craftCamera.zoom;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 worldMouse = GetScreenToWorld2D(virtualMouse, p->craftCamera);
                    int clickedId = -1;
                    for(int i=0; i<MAX_NODES; i++) {
                        if (activeGraph->nodes[i].active && CheckCollisionPointCircle(worldMouse, activeGraph->nodes[i].pos, 20.0f)) {
                            clickedId = i; break;
                        }
                    }
                    if (clickedId != -1) { p->selectedNodeId = clickedId; p->draggingNodeId = clickedId; } 
                    else if (p->selectedNodeId != -1) {
                        for(int i=0; i<MAX_NODES; i++) {
                            if (!activeGraph->nodes[i].active) {
                                InitDefaultSpellNode(&activeGraph->nodes[i]);
                                activeGraph->nodes[i].active = true;
                                activeGraph->nodes[i].parentId = p->selectedNodeId;
                                activeGraph->nodes[i].pos = worldMouse;
                                p->selectedNodeId = i; break;
                            }
                        }
                    }
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && p->draggingNodeId != -1) {
                    activeGraph->nodes[p->draggingNodeId].pos = GetScreenToWorld2D(virtualMouse, p->craftCamera);
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) p->draggingNodeId = -1;

                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    Vector2 worldMouse = GetScreenToWorld2D(virtualMouse, p->craftCamera);
                    for(int i=1; i<MAX_NODES; i++) { 
                        if (activeGraph->nodes[i].active && CheckCollisionPointCircle(worldMouse, activeGraph->nodes[i].pos, 20.0f)) {
                            DeleteNodeRec(activeGraph, i); 
                            if (!activeGraph->nodes[p->selectedNodeId].active) p->selectedNodeId = 0;
                            break; 
                        }
                    }
                }
            }

            BeginScissorMode(canvasBounds.x, canvasBounds.y, canvasBounds.width, canvasBounds.height);
            BeginMode2D(p->craftCamera);
                for(int i=-1000; i<1000; i+=50) {
                    DrawLine(i, -1000, i, 1000, Fade(DARKGRAY, 0.3f)); DrawLine(-1000, i, 1000, i, Fade(DARKGRAY, 0.3f));
                }
                for(int i=0; i<MAX_NODES; i++) {
                    if (!activeGraph->nodes[i].active || activeGraph->nodes[i].parentId == -1) continue;
                    
                    int pId = activeGraph->nodes[i].parentId;
                    if (pId >= 0 && pId < MAX_NODES && activeGraph->nodes[pId].active) {
                        Vector2 parentPos = activeGraph->nodes[pId].pos;
                        DrawLineEx(parentPos, activeGraph->nodes[i].pos, 2.0f, Fade(SKYBLUE, 0.6f));
                        
                        Vector2 mid = { (parentPos.x + activeGraph->nodes[i].pos.x)/2, (parentPos.y + activeGraph->nodes[i].pos.y)/2 };
                        if (activeGraph->nodes[i].hasDelay) DrawText(TextFormat("%.1fs", activeGraph->nodes[i].delay), mid.x-10, mid.y-10, 10, RAYWHITE);
                        if (activeGraph->nodes[i].hasSpeed) DrawCircle(mid.x, mid.y+5, 2.0f, GOLD); 
                        DrawCircleV(mid, 3.0f, RAYWHITE);
                    }
                }
                for(int i=0; i<MAX_NODES; i++) {
                    if (!activeGraph->nodes[i].active) continue;
                    Color ringColor = (i == p->selectedNodeId) ? GOLD : RAYWHITE;
                    if (i == 0) DrawCircleLines(activeGraph->nodes[i].pos.x, activeGraph->nodes[i].pos.y, 25.0f, PURPLE); 
                    DrawCircleLines(activeGraph->nodes[i].pos.x, activeGraph->nodes[i].pos.y, 18.0f, ringColor);
                    DrawNodeSigil(activeGraph->nodes[i].pos, activeGraph->nodes[i], 8.0f, GetTime());
                }
            EndMode2D(); EndScissorMode();

        } else if (p->craftCategory == 1) {
            DrawText("ARTIFICIAL SUPER INTEL (ASI)", 70, 70, 15, GREEN);
            GuiSlider((Rectangle){100, 120, 150, 20}, "MASS (HP)", NULL, &draftNPC->mass, 1, 200);
            GuiSlider((Rectangle){100, 150, 150, 20}, "AERO (Fly)", NULL, &draftNPC->aero, 0, 100);
            GuiSlider((Rectangle){100, 180, 150, 20}, "HYDR (Swim)", NULL, &draftNPC->hydro, 0, 100);
            GuiSlider((Rectangle){100, 210, 150, 20}, "TERR (Walk)", NULL, &draftNPC->terrestrial, 0, 100);
            GuiSlider((Rectangle){100, 250, 150, 20}, "INTEL (AI)", NULL, &draftNPC->intelligence, 0, 100);
            GuiSlider((Rectangle){100, 280, 150, 20}, "HOSTILE", NULL, &draftNPC->hostility, 0, 100);

            if (GuiButton((Rectangle){350, 120, 120, 30}, "RANDOM MUT")) {
                draftNPC->mass = GetRandomValue(1, 150); draftNPC->aero = GetRandomValue(0, 100);
                draftNPC->hydro = GetRandomValue(0, 100); draftNPC->terrestrial = GetRandomValue(0, 100);
                draftNPC->intelligence = GetRandomValue(0, 100); draftNPC->hostility = GetRandomValue(0, 100);
            }

            DrawRectangle(400, 180, 200, 150, Fade(DARKGRAY, 0.5f));
            DrawText("BLUEPRINT PREVIEW", 430, 190, 10, WHITE);
            DrawProceduralNPC((Vector2){500, 260}, 0, *draftNPC, 1.0f);

            if (GuiButton((Rectangle){70, 350, 150, 40}, "IMPRINT BLUEPRINT")) {
                p->hotbar[p->activeSlot].type = ITEM_NPC;
                p->hotbar[p->activeSlot].npc = *draftNPC;
                p->isCrafting = false;
            }
        }

        if (p->showCompendium) {
            DrawSpellCompendiumOverlay();
        }
    }
}

void DrawGuideMenu(Player *p,
                   bool *wantsRestart,
                   bool *wantsReturnToMenu,
                   bool *wantsRevive,
                   bool *showAbout,
                   bool *showMore,
                   bool canRestartWorld,
                   bool multiplayerActive,
                   bool isHost) {
    if (!p->showGuide) return;

    DrawRectangle(90, 45, 620, 360, Fade(DARKGRAY, 0.96f));
    DrawRectangleLines(90, 45, 620, 360, WHITE);
    DrawText("PAUSED", 112, 62, 24, GOLD);
    DrawText(multiplayerActive ? (isHost ? "SESSION: MULTIPLAYER HOST" : "SESSION: MULTIPLAYER CLIENT") : "SESSION: MY WORLD", 112, 90, 10, SKYBLUE);

    DrawText("SPELLCAST BASICS", 112, 116, 13, SKYBLUE);
    DrawText("LMB hold = power, C hold = duration, release LMB to cast", 112, 132, 10, RAYWHITE);
    DrawText("TAB switches lens/craft tabs, SHIFT flips cast layer", 112, 146, 10, RAYWHITE);
#if defined(PLATFORM_ANDROID)
    DrawText("Android build: map pause/craft controls to touch UI layer", 112, 160, 10, RAYWHITE);
#else
    DrawText("F11 fullscreen, ` opens compiler, ESC closes pause", 112, 160, 10, RAYWHITE);
#endif

    if (GuiButton((Rectangle){112, 190, 138, 30}, "RESUME")) p->showGuide = false;
    if (GuiButton((Rectangle){260, 190, 138, 30}, "RETURN MENU") && wantsReturnToMenu) *wantsReturnToMenu = true;
    if (GuiButton((Rectangle){408, 190, 138, 30}, "ABOUT") && showAbout) *showAbout = !(*showAbout);
    if (GuiButton((Rectangle){556, 190, 138, 30}, "MORE") && showMore) *showMore = !(*showMore);

    if (canRestartWorld) {
        if (GuiButton((Rectangle){112, 232, 180, 30}, "RESTART WORLD") && wantsRestart) *wantsRestart = true;
    } else {
        DrawRectangle(112, 232, 180, 30, Fade(BLACK, 0.5f));
        DrawRectangleLines(112, 232, 180, 30, GRAY);
        DrawText("RESTART LOCKED", 130, 241, 10, GRAY);
    }

    if (GuiButton((Rectangle){304, 232, 180, 30}, multiplayerActive ? "REVIVE PLAYER" : "REVIVE")) {
        if (wantsRevive) *wantsRevive = true;
    }

    if (multiplayerActive && !isHost) {
        DrawText("Client sessions cannot restart the world. Use Revive.", 112, 272, 10, ORANGE);
    } else if (multiplayerActive && isHost) {
        DrawText("Host controls world reset. Revive can be used anytime.", 112, 272, 10, LIGHTGRAY);
    } else {
        DrawText("Singleplayer supports restart and revive.", 112, 272, 10, LIGHTGRAY);
    }

    if (showAbout && *showAbout) {
        DrawRectangle(112, 292, 280, 100, Fade(BLACK, 0.85f));
        DrawRectangleLines(112, 292, 280, 100, GOLD);
        DrawText("ABOUT", 126, 304, 12, GOLD);
        DrawText("Metsys Engine is a temporal-scalar", 126, 322, 10, RAYWHITE);
        DrawText("spell sandbox with mutable terrain,", 126, 336, 10, RAYWHITE);
        DrawText("NPC ecology, and LAN-style multiplayer.", 126, 350, 10, RAYWHITE);
    }

    if (showMore && *showMore) {
        DrawRectangle(404, 292, 290, 100, Fade(BLACK, 0.85f));
        DrawRectangleLines(404, 292, 290, 100, SKYBLUE);
        DrawText("MORE", 418, 304, 12, SKYBLUE);
        DrawText("In Multiplayer: pick a world slot first.", 418, 322, 10, RAYWHITE);
        DrawText("Host world save drives the session state.", 418, 336, 10, RAYWHITE);
        DrawText("Each player keeps item loadout locally.", 418, 350, 10, RAYWHITE);
    }
}