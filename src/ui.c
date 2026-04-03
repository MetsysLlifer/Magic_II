#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

void DrawProceduralSigil(Vector2 center, SpellNode node, float scale) {
    int points = 3 + (int)(node.charge / 15.0f); 
    if (points > 10) points = 10;
    if (points < 3) points = 3;
    
    float baseRadius = scale * (0.5f + (node.density / 100.0f)); 
    float spikeFactor = (node.temp / 100.0f) * scale; 
    float waveFactor = (node.moisture / 100.0f); 

    Color baseColor = (node.charge > 50.0f) ? PURPLE : 
                      (node.temp > 50.0f) ? RED : 
                      (node.moisture > 50.0f) ? SKYBLUE : GOLD;

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

    if (p->isCharging) {
        float maxCharge = 3.0f;
        float radius = 10.0f + (p->chargeLevel / maxCharge) * 30.0f;
        float pulse = (sinf(p->animTime * 10.0f) + 1.0f) * 0.5f; 
        
        Vector2 cCenter = {p->pos.x, p->pos.y - p->z};
        DrawCircleLines(cCenter.x, cCenter.y, radius + (pulse * 2), Fade(SKYBLUE, 0.5f + pulse * 0.5f));
        DrawText(TextFormat("x%.1f", 1.0f + p->chargeLevel), cCenter.x + radius + 5, cCenter.y, 10, GOLD);

        SpellDNA activeDNA = p->hotbar[p->activeSlot];
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

void DrawInterface(Player *p, SpellDNA *draft) {
    DrawRectangle(10, 10, 200, 15, DARKGRAY);
    DrawRectangle(10, 10, (int)((fmax(0, p->health) / p->maxHealth) * 200), 15, RED);
    DrawRectangleLines(10, 10, 200, 15, WHITE);
    DrawText("VITALS", 15, 12, 10, WHITE);

    DrawRectangle(10, 35, 180, 120, Fade(BLACK, 0.8f));
    DrawRectangleLines(10, 35, 180, 120, GOLD);
    DrawText(TextFormat("ACTIVE SLOT: %d", p->activeSlot + 1), 15, 40, 10, GOLD);
    
    SpellDNA activeDNA = p->hotbar[p->activeSlot];
    const char* formName = (activeDNA.form == FORM_PROJECTILE) ? "PROJECTILE" : 
                           (activeDNA.form == FORM_MANIFEST) ? "MANIFEST" : 
                           (activeDNA.form == FORM_AURA) ? "AURA" : "BEAM";
                           
    DrawText(TextFormat("Form: %s", formName), 15, 55, 10, SKYBLUE);
    DrawText(TextFormat("Temp: %.0f | Mass: %.0f", activeDNA.temp, activeDNA.density), 15, 70, 10, RAYWHITE);
    DrawText(TextFormat("Cohe: %.0f | Wet: %.0f", activeDNA.cohesion, activeDNA.moisture), 15, 85, 10, RAYWHITE);
    DrawText(TextFormat("Chrg: %.0f", activeDNA.charge), 15, 100, 10, RAYWHITE);

    DrawProceduralSigil((Vector2){100, 130}, p->sigil.nodes[0], 15.0f);

    DrawText(TextFormat("VISION BLEND: %d%%", (int)(p->visionBlend * 100)), 10, 165, 10, PURPLE);
    Color targetColor = (p->castLayer == LAYER_AIR) ? SKYBLUE : BROWN;
    DrawText((p->castLayer == LAYER_AIR) ? "Z-TARGET: AIR [SHIFT]" : "Z-TARGET: GROUND [SHIFT]", 10, 180, 12, targetColor);
    DrawText("`=Craft | ESC=Guide | [ / ]=Blend", 10, 195, 10, GRAY);

    // --- INFINITE NODE COMPILER UI ---
    if (p->isCrafting && !p->showGuide) {
        Rectangle canvasBounds = {250, 50, 500, 360};
        
        DrawRectangle(50, 50, 700, 360, Fade(BLACK, 0.95f));
        DrawRectangleLines(50, 50, 700, 360, GOLD);
        DrawLine(250, 50, 250, 410, DARKGRAY);

        // LEFT PANE: Node Parameters AND Node Movement Settings
        DrawText("NODE PARAMETERS", 70, 70, 15, GOLD);
        if (p->selectedNodeId != -1 && p->sigil.nodes[p->selectedNodeId].active) {
            SpellNode *n = &p->sigil.nodes[p->selectedNodeId];
            DrawText(TextFormat("Editing Node ID: %d", p->selectedNodeId), 70, 90, 10, SKYBLUE);
            GuiSlider((Rectangle){90, 110, 130, 15}, "TEMP", NULL, &n->temp, -100, 200);
            GuiSlider((Rectangle){90, 135, 130, 15}, "MASS", NULL, &n->density, 0, 100);
            GuiSlider((Rectangle){90, 160, 130, 15}, "COHE", NULL, &n->cohesion, 0, 100);
            GuiSlider((Rectangle){90, 185, 130, 15}, "WET ", NULL, &n->moisture, 0, 100);
            GuiSlider((Rectangle){90, 210, 130, 15}, "CHRG", NULL, &n->charge, 0, 100);

            // EXPLICIT NODE MOVEMENT CONTROLS
            DrawText("NODE MOVEMENT:", 70, 240, 10, WHITE);
            if(GuiButton((Rectangle){70, 255, 35, 20}, "STR")) n->movement = MOVE_STRAIGHT;
            if(GuiButton((Rectangle){110, 255, 35, 20}, "SIN")) n->movement = MOVE_SIN;
            if(GuiButton((Rectangle){150, 255, 35, 20}, "COS")) n->movement = MOVE_COS;
            if(GuiButton((Rectangle){190, 255, 35, 20}, "ORB")) n->movement = MOVE_ORBIT;
            
            // Highlight active movement
            int mx = (n->movement == 0) ? 70 : (n->movement == 1) ? 110 : (n->movement == 2) ? 150 : 190;
            DrawRectangleLines(mx, 255, 35, 20, GREEN);

        } else {
            DrawText("No Node Selected.", 70, 120, 10, GRAY);
        }

        // EXPLICIT GLOBAL FORM CONTROLS
        DrawText("GLOBAL FORM:", 70, 290, 10, WHITE);
        if(GuiButton((Rectangle){70, 305, 50, 25}, "PROJ")) p->selectedForm = FORM_PROJECTILE;
        if(GuiButton((Rectangle){125, 305, 50, 25}, "MANI")) p->selectedForm = FORM_MANIFEST;
        if(GuiButton((Rectangle){180, 305, 50, 25}, "AURA")) p->selectedForm = FORM_AURA;
        
        int fx = (p->selectedForm == 0) ? 70 : (p->selectedForm == 1) ? 125 : 180;
        DrawRectangleLines(fx, 305, 50, 25, PURPLE);

        GuiCheckBox((Rectangle){70, 345, 15, 15}, "ETERNAL", &draft->isPermanent);

        if (GuiButton((Rectangle){70, 370, 160, 30}, "IMPRINT TO SLOT")) {
            CompileSigilGraph(p, draft); 
            p->hotbar[p->activeSlot] = *draft;
            p->isCrafting = false;
        }

        // RIGHT PANE: Canvas
        DrawText("INFINITE COMPILER CANVAS", 270, 60, 20, GOLD);
        DrawText("L-Click: Add/Select | R-Click: Delete | Scroll: Zoom | Mid-Click: Pan", 270, 85, 10, GRAY);

        Vector2 mousePos = GetMousePosition();
        if (CheckCollisionPointRec(mousePos, canvasBounds)) {
            p->craftCamera.zoom += GetMouseWheelMove() * 0.1f;
            if (p->craftCamera.zoom < 0.2f) p->craftCamera.zoom = 0.2f;
            if (p->craftCamera.zoom > 3.0f) p->craftCamera.zoom = 3.0f;
            
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                Vector2 delta = GetMouseDelta();
                p->craftCamera.target.x -= delta.x / p->craftCamera.zoom;
                p->craftCamera.target.y -= delta.y / p->craftCamera.zoom;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 worldMouse = GetScreenToWorld2D(mousePos, p->craftCamera);
                int clickedId = -1;
                
                for(int i=0; i<MAX_NODES; i++) {
                    if (p->sigil.nodes[i].active && CheckCollisionPointCircle(worldMouse, p->sigil.nodes[i].pos, 20.0f)) {
                        clickedId = i; break;
                    }
                }

                if (clickedId != -1) {
                    p->selectedNodeId = clickedId; 
                } else if (p->selectedNodeId != -1) {
                    for(int i=0; i<MAX_NODES; i++) {
                        if (!p->sigil.nodes[i].active) {
                            p->sigil.nodes[i].active = true;
                            p->sigil.nodes[i].parentId = p->selectedNodeId;
                            p->sigil.nodes[i].pos = worldMouse;
                            p->sigil.nodes[i].temp = 0; p->sigil.nodes[i].density = 0;
                            p->sigil.nodes[i].cohesion = 0; p->sigil.nodes[i].moisture = 0; p->sigil.nodes[i].charge = 0;
                            p->sigil.nodes[i].movement = MOVE_STRAIGHT; // Default
                            p->selectedNodeId = i; 
                            break;
                        }
                    }
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                Vector2 worldMouse = GetScreenToWorld2D(mousePos, p->craftCamera);
                for(int i=1; i<MAX_NODES; i++) { 
                    if (p->sigil.nodes[i].active && CheckCollisionPointCircle(worldMouse, p->sigil.nodes[i].pos, 20.0f)) {
                        p->sigil.nodes[i].active = false;
                        if (p->selectedNodeId == i) p->selectedNodeId = 0;
                        for(int j=1; j<MAX_NODES; j++) {
                            if (p->sigil.nodes[j].parentId == i) p->sigil.nodes[j].active = false;
                        }
                    }
                }
            }
        }

        BeginScissorMode(canvasBounds.x, canvasBounds.y, canvasBounds.width, canvasBounds.height);
        BeginMode2D(p->craftCamera);
            
            for(int i=-1000; i<1000; i+=50) {
                DrawLine(i, -1000, i, 1000, Fade(DARKGRAY, 0.3f));
                DrawLine(-1000, i, 1000, i, Fade(DARKGRAY, 0.3f));
            }

            for(int i=0; i<MAX_NODES; i++) {
                if (!p->sigil.nodes[i].active || p->sigil.nodes[i].parentId == -1) continue;
                Vector2 parentPos = p->sigil.nodes[p->sigil.nodes[i].parentId].pos;
                DrawLineEx(parentPos, p->sigil.nodes[i].pos, 2.0f, Fade(SKYBLUE, 0.6f));
                Vector2 mid = { (parentPos.x + p->sigil.nodes[i].pos.x)/2, (parentPos.y + p->sigil.nodes[i].pos.y)/2 };
                DrawCircleV(mid, 3.0f, RAYWHITE);
            }

            for(int i=0; i<MAX_NODES; i++) {
                if (!p->sigil.nodes[i].active) continue;
                Color ringColor = (i == p->selectedNodeId) ? GOLD : RAYWHITE;
                if (i == 0) DrawCircleLines(p->sigil.nodes[i].pos.x, p->sigil.nodes[i].pos.y, 25.0f, PURPLE); 
                DrawCircleLines(p->sigil.nodes[i].pos.x, p->sigil.nodes[i].pos.y, 18.0f, ringColor);
                DrawProceduralSigil(p->sigil.nodes[i].pos, p->sigil.nodes[i], 8.0f);
            }
            
            if (p->selectedNodeId != -1 && p->sigil.nodes[p->selectedNodeId].active) {
                DrawCircleLines(p->sigil.nodes[p->selectedNodeId].pos.x, p->sigil.nodes[p->selectedNodeId].pos.y, 40.0f, Fade(SKYBLUE, 0.2f));
            }

        EndMode2D();
        EndScissorMode();

        CompileSigilGraph(p, draft);
    }
}

void DrawGuideMenu(Player *p) {
    if (!p->showGuide) return;
    DrawRectangle(100, 50, 600, 350, Fade(DARKGRAY, 0.95f));
    DrawRectangleLines(100, 50, 600, 350, WHITE);
    DrawText("METSYS: ARCHITECTURE OF MAGIC", 120, 70, 20, GOLD);
    DrawText("--------------------------------------------------", 120, 90, 20, GRAY);
    
    DrawText("EXPLICIT FORMS & KINETICS:", 120, 120, 15, SKYBLUE);
    DrawText("- MOVEMENT: Core node sets the projectile's trajectory (Straight, Sin, Cos, Orbit).", 130, 145, 10, WHITE);
    DrawText("- FORM: Select exactly how the spell enters the world via Global modifiers.", 130, 160, 10, WHITE);
    DrawText("- BEAM OVERRIDE: Extreme Heat + Charge will organically override and cast as a Beam.", 130, 175, 10, WHITE);

    DrawText("CONTROLS:", 120, 210, 15, SKYBLUE);
    DrawText("- [W, A, S, D]: Move  |  [SPACE]: Jump  |  [Mouse 1]: Hold to Cast", 130, 235, 10, WHITE);
    DrawText("- [SHIFT]: Toggle Cast Target (Ground vs Air)", 130, 250, 10, GREEN); 
    DrawText("- [ ` ]: Compiler   |   [ESC]: Guide", 130, 265, 10, WHITE);
    DrawText("- [ TAB ]: Quick-Swap Vision  |  [ [ ] & [ ] ]: Fine-tune Blend", 130, 280, 10, PURPLE);
}