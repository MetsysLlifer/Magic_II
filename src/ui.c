#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

void DrawProceduralSigil(Vector2 center, SpellNode node, float scale) {
    int points = 3 + (int)(node.charge / 15.0f); 
    if (points > 10) points = 10; if (points < 3) points = 3;
    float baseRadius = scale * (0.5f + (node.density / 100.0f)); 
    float spikeFactor = (node.temp / 100.0f) * scale; 
    float waveFactor = (node.moisture / 100.0f); 
    Color baseColor = (node.charge > 50.0f) ? PURPLE : (node.temp > 50.0f) ? RED : (node.moisture > 50.0f) ? SKYBLUE : GOLD;

    Vector2 pLast = {0};
    for (int i = 0; i <= points; i++) {
        float angle = (i * (PI * 2.0f)) / points;
        float r = baseRadius + (i % 2 == 0 ? spikeFactor : -spikeFactor * 0.5f);
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
        if (c.temp > 70.0f) return ORANGE;        
        else if (c.moisture > 30.0f) return BLUE; 
        else return BROWN;                        
    } else {
        if (c.temp > 70.0f) return RED;           
        else if (c.charge > 40.0f) return PURPLE; 
        else return Fade(LIGHTGRAY, c.density / 20.0f); 
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
                } else {
                    DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, color);
                }
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

// NEW: Hazard Realm visually parses the grid scalars for potential player harm
void DrawHazardRealm(float alpha) {
    if (alpha <= 0.0f) return;
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                Cell c = grid[z][y * WIDTH + x];
                
                // Mirroring the physics engine's damage math
                float heatHarm = fmaxf(0.0f, c.temp - 60.0f) / 50.0f; 
                float coldHarm = fmaxf(0.0f, -10.0f - c.temp) / 50.0f;
                float momentum = c.density * sqrtf(c.velocity.x*c.velocity.x + c.velocity.y*c.velocity.y);
                float kinHarm = fmaxf(0.0f, momentum - 150.0f) / 100.0f;
                float shockHarm = fmaxf(0.0f, c.charge - 40.0f) / 50.0f;
                
                float totalHarm = heatHarm + coldHarm + kinHarm + shockHarm;
                if (totalHarm <= 0.0f) continue;

                // Color mixing based on danger type
                unsigned char r = (unsigned char)fmin(255, (heatHarm + kinHarm + shockHarm) * 255);
                unsigned char g = (unsigned char)fmin(255, (kinHarm) * 255);
                unsigned char b = (unsigned char)fmin(255, (coldHarm + shockHarm) * 255);
                
                // Lethal cells pulse violently
                float pulse = (sinf(GetTime() * 15.0f) + 1.0f) * 0.5f;
                unsigned char finalAlpha = (unsigned char)(fminf(255, totalHarm * 150 + (pulse * 100)) * alpha);

                int yOffset = (z == LAYER_AIR) ? FLOAT_OFFSET : 0;
                DrawRectangle(x * PIXEL_SIZE, (y * PIXEL_SIZE) - yOffset, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, finalAlpha});
            }
        }
    }
    
    // Expose Hostile Biological Entities
    for(int i=0; i<50; i++) {
        if(active_npcs[i].active && active_npcs[i].dna.hostility > 50.0f) {
            Vector2 pCenter = {active_npcs[i].pos.x, active_npcs[i].pos.y - active_npcs[i].z};
            float pulse = (sinf(GetTime() * 10.0f) + 1.0f) * 0.5f;
            DrawCircleLines(pCenter.x, pCenter.y, 10.0f + (pulse * 5.0f), Fade(RED, alpha));
            DrawText("! HOSTILE !", pCenter.x - 25, pCenter.y - 20, 10, Fade(RED, alpha));
        }
    }
}

void DrawProjectiles(Player *p) {
    for(int i=0; i<100; i++) {
        if(projectiles[i].active) {
            int yOffset = (projectiles[i].layer == LAYER_AIR) ? FLOAT_OFFSET : 0;
            Vector2 pCenter = {projectiles[i].pos.x, projectiles[i].pos.y - yOffset};
            float pulse = sinf(projectiles[i].animOffset) * 2.0f;
            DrawCircle(pCenter.x, pCenter.y, 4 + pulse, GOLD);
            DrawCircleLines(pCenter.x, pCenter.y, 6 + pulse, RAYWHITE);
            
            if (projectiles[i].payload.charge > 10.0f) {
                float t = GetTime() * 30.0f;
                float ox = sinf(t + i) * 4.0f;
                float oy = cosf(t * 1.5f + i) * 4.0f;
                DrawLineEx(pCenter, (Vector2){pCenter.x + ox, pCenter.y + oy}, 1.5f, (Color){200, 150, 255, 200});
            }
            if (projectiles[i].layer == LAYER_AIR) DrawEllipse(projectiles[i].pos.x, projectiles[i].pos.y, 4, 2, Fade(BLACK, 0.5f));
        }
    }
}

void DrawPlayerEntity(Player *p) {
    if (p->health <= 0) return;
    DrawEllipse(p->pos.x, p->pos.y, 6, 3, Fade(BLACK, 0.6f));
    float stretch = 1.0f + (p->zVelocity / 1000.0f); 
    float squash = 1.0f / stretch;
    float breathe = (sinf(p->animTime * 4.0f) + 1.0f) * 0.5f;
    float currentWidth = (6.0f * squash) + (breathe * 1.5f);
    float currentHeight = 6.0f * stretch;

    DrawEllipse(p->pos.x, p->pos.y - p->z, currentWidth, currentHeight, RAYWHITE); 

    if (p->isCharging && p->hotbar[p->activeSlot].type == ITEM_SPELL) {
        float maxCharge = 3.0f;
        float radius = 10.0f + (p->chargeLevel / maxCharge) * 30.0f;
        float pulse = (sinf(p->animTime * 10.0f) + 1.0f) * 0.5f; 
        
        Vector2 cCenter = {p->pos.x, p->pos.y - p->z};
        DrawCircleLines(cCenter.x, cCenter.y, radius + (pulse * 2), Fade(SKYBLUE, 0.5f + pulse * 0.5f));
        DrawText(TextFormat("x%.1f", 1.0f + p->chargeLevel), cCenter.x + radius + 5, cCenter.y, 10, GOLD);

        SpellDNA activeDNA = p->hotbar[p->activeSlot].spell;
        if (activeDNA.charge > 10.0f) {
            float t = GetTime() * 40.0f;
            float sparkRadius = radius + 3.0f;
            for(int i = 0; i < 3; i++) {
                float angle = (t + i * 2.0f);
                float ox = sinf(angle * 1.3f) * sparkRadius;
                float oy = cosf(angle * 0.7f) * sparkRadius;
                DrawLineEx(cCenter, (Vector2){cCenter.x + ox, cCenter.y + oy}, 1.5f, (Color){200, 150, 255, 255});
            }
        }
    }
}

void DrawInterface(Player *p, NPCDNA *draftNPC) {
    DrawRectangle(10, 10, 200, 15, DARKGRAY);
    DrawRectangle(10, 10, (int)((fmax(0, p->health) / p->maxHealth) * 200), 15, RED);
    DrawRectangleLines(10, 10, 200, 15, WHITE);
    DrawText("VITALS", 15, 12, 10, WHITE);

    DrawRectangle(10, 35, 180, 120, Fade(BLACK, 0.8f));
    DrawRectangleLines(10, 35, 180, 120, GOLD);
    DrawText(TextFormat("ACTIVE SLOT: %d", p->activeSlot + 1), 15, 40, 10, GOLD);
    
    HotbarSlot activeSlot = p->hotbar[p->activeSlot];
    if (activeSlot.type == ITEM_SPELL) {
        DrawText("TYPE: ENERGY SCALAR", 15, 55, 10, SKYBLUE);
        DrawText(TextFormat("Temp: %.0f | Mass: %.0f", activeSlot.spell.temp, activeSlot.spell.density), 15, 70, 10, RAYWHITE);
        DrawText(TextFormat("Cohe: %.0f | Wet: %.0f", activeSlot.spell.cohesion, activeSlot.spell.moisture), 15, 85, 10, RAYWHITE);
        DrawText(TextFormat("Chrg: %.0f", activeSlot.spell.charge), 15, 100, 10, RAYWHITE);
        DrawProceduralSigil((Vector2){100, 130}, activeSlot.spell.graph.nodes[0], 15.0f);
    } else {
        DrawText("TYPE: BIO-MATRIX", 15, 55, 10, GREEN);
        DrawText(TextFormat("Mass: %.0f | Int: %.0f", activeSlot.npc.mass, activeSlot.npc.intelligence), 15, 70, 10, RAYWHITE);
        DrawText(TextFormat("Aero: %.0f | Hyd: %.0f", activeSlot.npc.aero, activeSlot.npc.hydro), 15, 85, 10, RAYWHITE);
        DrawText(TextFormat("Terr: %.0f | Hst: %.0f", activeSlot.npc.terrestrial, activeSlot.npc.hostility), 15, 100, 10, RAYWHITE);
        DrawProceduralNPC((Vector2){100, 135}, 0, activeSlot.npc, 1.0f); 
    }

    // UPDATED VISION BLEND HUD
    const char* vState = (p->visionBlend < 0.5f) ? "MAT" : (p->visionBlend < 1.5f) ? "NRG" : "HAZ";
    DrawText(TextFormat("VISION BLEND: %.1f [%s]", p->visionBlend, vState), 10, 165, 10, PURPLE);
    
    Color targetColor = (p->castLayer == LAYER_AIR) ? SKYBLUE : BROWN;
    DrawText((p->castLayer == LAYER_AIR) ? "Z-TARGET: AIR [SHIFT]" : "Z-TARGET: GROUND [SHIFT]", 10, 180, 12, targetColor);
    DrawText("`=Craft | ESC=Guide | [ / ]=Blend", 10, 195, 10, GRAY);

    int startX = (800 - (10 * 35)) / 2; 
    for(int i = 0; i < 10; i++) {
        Rectangle slotRec = { startX + (i * 35), 400, 30, 30 };
        DrawRectangleRec(slotRec, Fade(BLACK, 0.8f));
        Color bColor = (i == p->activeSlot) ? GOLD : DARKGRAY;
        DrawRectangleLinesEx(slotRec, (i == p->activeSlot) ? 2 : 1, bColor);
        DrawText(TextFormat("%d", i+1), slotRec.x + 3, slotRec.y + 2, 10, GRAY);
        
        if (p->hotbar[i].type == ITEM_SPELL) {
            if (p->hotbar[i].spell.temp != 20.0f || p->hotbar[i].spell.density != 0.0f) {
                DrawCircle(slotRec.x + 15, slotRec.y + 10, 3, PURPLE);
                const char* fStr = (p->hotbar[i].spell.form == FORM_PROJECTILE) ? "PRJ" :
                                   (p->hotbar[i].spell.form == FORM_MANIFEST) ? "MNF" :
                                   (p->hotbar[i].spell.form == FORM_AURA) ? "AUR" : "BEM";
                DrawText(fStr, slotRec.x + 6, slotRec.y + 18, 8, SKYBLUE);
            }
        } else {
            DrawCircle(slotRec.x + 15, slotRec.y + 15, 5, GREEN);
        }
    }

    if (p->isCrafting && !p->showGuide) {
        DrawRectangle(50, 30, 700, 380, Fade(BLACK, 0.95f));
        DrawRectangleLines(50, 30, 700, 380, GOLD);
        
        DrawText("TAB: SWITCH ARCHITECTURE", 60, 40, 10, GRAY);
        Color spellTab = (p->craftCategory == 0) ? GOLD : DARKGRAY;
        Color npcTab = (p->craftCategory == 1) ? GREEN : DARKGRAY;
        DrawText("SPELL MATRIX", 300, 40, 15, spellTab);
        DrawText("BIO MATRIX", 450, 40, 15, npcTab);

        if (p->craftCategory == 0) {
            SpellDNA *activeSpell = &p->hotbar[p->activeSlot].spell;
            Rectangle canvasBounds = {250, 60, 490, 340};
            DrawLine(240, 60, 240, 400, DARKGRAY);

            DrawText("NODE PARAMETERS", 60, 70, 15, GOLD);
            if (p->selectedNodeId != -1 && activeSpell->graph.nodes[p->selectedNodeId].active) {
                SpellNode *n = &activeSpell->graph.nodes[p->selectedNodeId];
                DrawText(TextFormat("Editing Node ID: %d", p->selectedNodeId), 60, 90, 10, SKYBLUE);
                GuiSlider((Rectangle){90, 110, 120, 15}, "TEMP", NULL, &n->temp, -100, 200);
                GuiSlider((Rectangle){90, 135, 120, 15}, "MASS", NULL, &n->density, 0, 100);
                GuiSlider((Rectangle){90, 160, 120, 15}, "COHE", NULL, &n->cohesion, 0, 100);
                GuiSlider((Rectangle){90, 185, 120, 15}, "WET ", NULL, &n->moisture, 0, 100);
                GuiSlider((Rectangle){90, 210, 120, 15}, "CHRG", NULL, &n->charge, 0, 100);

                DrawText("NODE MOVEMENT:", 60, 240, 10, WHITE);
                if(GuiButton((Rectangle){60, 255, 35, 20}, "STR")) n->movement = MOVE_STRAIGHT;
                if(GuiButton((Rectangle){100, 255, 35, 20}, "SIN")) n->movement = MOVE_SIN;
                if(GuiButton((Rectangle){140, 255, 35, 20}, "COS")) n->movement = MOVE_COS;
                if(GuiButton((Rectangle){180, 255, 35, 20}, "ORB")) n->movement = MOVE_ORBIT;
                int mx = (n->movement == 0) ? 60 : (n->movement == 1) ? 100 : (n->movement == 2) ? 140 : 180;
                DrawRectangleLines(mx, 255, 35, 20, GREEN);
            }

            DrawText("GLOBAL FORM:", 60, 290, 10, WHITE);
            if(GuiButton((Rectangle){60, 305, 50, 25}, "PROJ")) activeSpell->form = FORM_PROJECTILE;
            if(GuiButton((Rectangle){115, 305, 50, 25}, "MANI")) activeSpell->form = FORM_MANIFEST;
            if(GuiButton((Rectangle){170, 305, 50, 25}, "AURA")) activeSpell->form = FORM_AURA;
            int fx = (activeSpell->form == 0) ? 60 : (activeSpell->form == 1) ? 115 : 170;
            DrawRectangleLines(fx, 305, 50, 25, PURPLE);

            GuiCheckBox((Rectangle){60, 345, 15, 15}, "ETERNAL", &activeSpell->isPermanent);

            DrawText("INFINITE COMPILER CANVAS", 250, 65, 15, GOLD);
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, canvasBounds)) {
                p->craftCamera.zoom += GetMouseWheelMove() * 0.1f;
                if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                    Vector2 delta = GetMouseDelta();
                    p->craftCamera.target.x -= delta.x / p->craftCamera.zoom;
                    p->craftCamera.target.y -= delta.y / p->craftCamera.zoom;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 worldMouse = GetScreenToWorld2D(mousePos, p->craftCamera);
                    int clickedId = -1;
                    for(int i=0; i<MAX_NODES; i++) {
                        if (activeSpell->graph.nodes[i].active && CheckCollisionPointCircle(worldMouse, activeSpell->graph.nodes[i].pos, 20.0f)) {
                            clickedId = i; break;
                        }
                    }
                    if (clickedId != -1) { p->selectedNodeId = clickedId; p->draggingNodeId = clickedId; } 
                    else if (p->selectedNodeId != -1) {
                        for(int i=0; i<MAX_NODES; i++) {
                            if (!activeSpell->graph.nodes[i].active) {
                                activeSpell->graph.nodes[i].active = true; activeSpell->graph.nodes[i].parentId = p->selectedNodeId;
                                activeSpell->graph.nodes[i].pos = worldMouse; activeSpell->graph.nodes[i].movement = MOVE_STRAIGHT;
                                p->selectedNodeId = i; break;
                            }
                        }
                    }
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && p->draggingNodeId != -1) {
                    activeSpell->graph.nodes[p->draggingNodeId].pos = GetScreenToWorld2D(mousePos, p->craftCamera);
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) p->draggingNodeId = -1;

                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    Vector2 worldMouse = GetScreenToWorld2D(mousePos, p->craftCamera);
                    for(int i=1; i<MAX_NODES; i++) { 
                        if (activeSpell->graph.nodes[i].active && CheckCollisionPointCircle(worldMouse, activeSpell->graph.nodes[i].pos, 20.0f)) {
                            activeSpell->graph.nodes[i].active = false;
                            if (p->selectedNodeId == i) p->selectedNodeId = 0;
                            for(int j=1; j<MAX_NODES; j++) if (activeSpell->graph.nodes[j].parentId == i) activeSpell->graph.nodes[j].active = false;
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
                    if (!activeSpell->graph.nodes[i].active || activeSpell->graph.nodes[i].parentId == -1) continue;
                    Vector2 parentPos = activeSpell->graph.nodes[activeSpell->graph.nodes[i].parentId].pos;
                    DrawLineEx(parentPos, activeSpell->graph.nodes[i].pos, 2.0f, Fade(SKYBLUE, 0.6f));
                    DrawCircleV((Vector2){(parentPos.x+activeSpell->graph.nodes[i].pos.x)/2, (parentPos.y+activeSpell->graph.nodes[i].pos.y)/2}, 3.0f, RAYWHITE);
                }
                for(int i=0; i<MAX_NODES; i++) {
                    if (!activeSpell->graph.nodes[i].active) continue;
                    Color ringColor = (i == p->selectedNodeId) ? GOLD : RAYWHITE;
                    if (i == 0) DrawCircleLines(activeSpell->graph.nodes[i].pos.x, activeSpell->graph.nodes[i].pos.y, 25.0f, PURPLE); 
                    DrawCircleLines(activeSpell->graph.nodes[i].pos.x, activeSpell->graph.nodes[i].pos.y, 18.0f, ringColor);
                    DrawProceduralSigil(activeSpell->graph.nodes[i].pos, activeSpell->graph.nodes[i], 8.0f);
                }
            EndMode2D(); EndScissorMode();
            
            CompileSigilGraph(activeSpell); 

        } else if (p->craftCategory == 1) {
            DrawText("ARTIFICIAL SUPER INTEL (ASI)", 70, 70, 15, GREEN);
            GuiSlider((Rectangle){100, 120, 150, 20}, "MASS (HP)", NULL, &draftNPC->mass, 10, 200);
            GuiSlider((Rectangle){100, 150, 150, 20}, "AERO (Fly)", NULL, &draftNPC->aero, 0, 100);
            GuiSlider((Rectangle){100, 180, 150, 20}, "HYDR (Swim)", NULL, &draftNPC->hydro, 0, 100);
            GuiSlider((Rectangle){100, 210, 150, 20}, "TERR (Walk)", NULL, &draftNPC->terrestrial, 0, 100);
            GuiSlider((Rectangle){100, 250, 150, 20}, "INTEL (AI)", NULL, &draftNPC->intelligence, 0, 100);
            GuiSlider((Rectangle){100, 280, 150, 20}, "HOSTILE", NULL, &draftNPC->hostility, 0, 100);

            if (GuiButton((Rectangle){350, 120, 120, 30}, "RANDOM MUT")) {
                draftNPC->mass = GetRandomValue(10, 150); draftNPC->aero = GetRandomValue(0, 100);
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
    }
}

void DrawGuideMenu(Player *p) {
    if (!p->showGuide) return;
    DrawRectangle(100, 50, 600, 350, Fade(DARKGRAY, 0.95f));
    DrawRectangleLines(100, 50, 600, 350, WHITE);
    DrawText("METSYS: ARCHITECTURE OF MAGIC", 120, 70, 20, GOLD);
    DrawText("--------------------------------------------------", 120, 90, 20, GRAY);
    
    DrawText("VISION REALMS (Use [ and ] to blend):", 120, 120, 15, PURPLE);
    DrawText("- 0.0 to 1.0: Blends from Material Reality into Energy Scalars.", 130, 145, 10, WHITE);
    DrawText("- 1.0 to 2.0: Blends from Energy into HAZARD (Detective) Vision.", 130, 160, 10, WHITE);
    DrawText("- Hazard Vision highlights lethal physics (heat, crush, shock) and hostile NPCs.", 130, 175, 10, RED);

    DrawText("CONTROLS:", 120, 210, 15, SKYBLUE);
    DrawText("- [W, A, S, D]: Move  |  [SPACE]: Jump  |  [Mouse 1]: Hold to Cast", 130, 235, 10, WHITE);
    DrawText("- [SHIFT]: Toggle Cast Target (Ground vs Air)", 130, 250, 10, GREEN); 
    DrawText("- [ ` ]: Compiler   |   [ESC]: Guide", 130, 265, 10, WHITE);
    DrawText("- [ TAB ]: Quick-Swap Vision or Crafting Category", 130, 280, 10, PURPLE);
}