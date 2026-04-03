#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// NEW: Procedural Electricity Generator
void DrawChargeSparks(int x, int y, int z, float charge, float alpha) {
    if (charge < 10.0f || alpha <= 0.0f) return;
    
    // Fast flickering time baseline
    float t = GetTime() * 25.0f; 
    
    // Pseudo-random chaotic offsets using Sin/Cos interference patterns
    float ox1 = sinf(t + x * 1.3f + y * 0.7f) * (PIXEL_SIZE * 0.9f);
    float oy1 = cosf(t * 1.1f + x * 0.8f + y * 1.5f) * (PIXEL_SIZE * 0.9f);
    float ox2 = sinf(t * 1.4f - x * 1.1f + y * 0.9f) * (PIXEL_SIZE * 0.9f);
    float oy2 = cosf(t * 0.9f - x * 1.4f - y * 1.2f) * (PIXEL_SIZE * 0.9f);

    int yOffset = (z == LAYER_AIR) ? FLOAT_OFFSET : 0;
    Vector2 start = { x * PIXEL_SIZE + (PIXEL_SIZE / 2.0f) + ox1, (y * PIXEL_SIZE) - yOffset + (PIXEL_SIZE / 2.0f) + oy1 };
    Vector2 end = { x * PIXEL_SIZE + (PIXEL_SIZE / 2.0f) + ox2, (y * PIXEL_SIZE) - yOffset + (PIXEL_SIZE / 2.0f) + oy2 };

    unsigned char opacity = (unsigned char)(fmin(255, charge * 5) * alpha);
    
    // Draw the jagged spark
    DrawLineEx(start, end, 1.0f, (Color){220, 200, 255, opacity}); // Bright violet-white static
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
            
            // Render Ground Sparks
            DrawChargeSparks(x, y, LAYER_GROUND, c.charge, alpha);
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
            
            // Render Air Sparks
            DrawChargeSparks(x, y, LAYER_AIR, airCell.charge, alpha);
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
                
                // Static renders brightly in the energy realm too
                DrawChargeSparks(x, y, z, c.charge, alpha);
            }
        }
    }
}

void DrawProjectiles() {
    for(int i=0; i<100; i++) {
        if(projectiles[i].active) {
            int yOffset = (projectiles[i].layer == LAYER_AIR) ? FLOAT_OFFSET : 0;
            Vector2 pCenter = {projectiles[i].pos.x, projectiles[i].pos.y - yOffset};
            
            float pulse = sinf(projectiles[i].animOffset) * 2.0f;
            DrawCircle(pCenter.x, pCenter.y, 4 + pulse, GOLD);
            DrawCircleLines(pCenter.x, pCenter.y, 6 + pulse, RAYWHITE);
            
            // Add sparking trails to charged projectiles
            if (projectiles[i].payload.charge > 10.0f) {
                float t = GetTime() * 30.0f;
                float ox = sinf(t + i) * 8.0f;
                float oy = cosf(t * 1.5f + i) * 8.0f;
                DrawLineEx(pCenter, (Vector2){pCenter.x + ox, pCenter.y + oy}, 2.0f, (Color){200, 150, 255, 200});
            }
            
            if (projectiles[i].layer == LAYER_AIR) DrawEllipse(projectiles[i].pos.x, projectiles[i].pos.y, 4, 2, Fade(BLACK, 0.5f));
        }
    }
}

void DrawInterface(Player *p, SpellDNA *draft) {
    DrawRectangle(10, 10, 200, 15, DARKGRAY);
    DrawRectangle(10, 10, (int)((fmax(0, p->health) / p->maxHealth) * 200), 15, RED);
    DrawRectangleLines(10, 10, 200, 15, WHITE);
    DrawText("VITALS", 15, 12, 10, WHITE);

    DrawRectangle(10, 35, 180, 100, Fade(BLACK, 0.8f));
    DrawRectangleLines(10, 35, 180, 100, GOLD);
    DrawText(TextFormat("ACTIVE SLOT: %d", p->activeSlot + 1), 15, 40, 10, GOLD);
    
    SpellDNA activeDNA = p->hotbar[p->activeSlot];
    const char* formName = (activeDNA.form == FORM_PROJECTILE) ? "PROJECTILE" : 
                           (activeDNA.form == FORM_MANIFEST) ? "MANIFEST" : "AURA";
                           
    DrawText(TextFormat("Form: %s", formName), 15, 55, 10, SKYBLUE);
    DrawText(TextFormat("Temp: %.0f | Mass: %.0f", activeDNA.temp, activeDNA.density), 15, 70, 10, RAYWHITE);
    DrawText(TextFormat("Cohe: %.0f | Wet: %.0f", activeDNA.cohesion, activeDNA.moisture), 15, 85, 10, RAYWHITE);
    DrawText(TextFormat("Chrg: %.0f", activeDNA.charge), 15, 100, 10, RAYWHITE);

    DrawText(TextFormat("VISION BLEND: %d%%", (int)(p->visionBlend * 100)), 10, 145, 10, PURPLE);
    
    Color targetColor = (p->castLayer == LAYER_AIR) ? SKYBLUE : BROWN;
    DrawText((p->castLayer == LAYER_AIR) ? "Z-TARGET: AIR [SHIFT]" : "Z-TARGET: GROUND [SHIFT]", 10, 160, 12, targetColor);
    DrawText("`=Craft | ESC=Guide | [ / ]=Blend", 10, 175, 10, GRAY);

    if (p->isCharging) {
        float maxCharge = 3.0f;
        float radius = 10.0f + (p->chargeLevel / maxCharge) * 30.0f;
        float pulse = (sinf(p->animTime * 10.0f) + 1.0f) * 0.5f; 
        
        Vector2 cCenter = {p->pos.x, p->pos.y - p->z};
        DrawCircleLines(cCenter.x, cCenter.y, radius + (pulse * 2), Fade(SKYBLUE, 0.5f + pulse * 0.5f));
        DrawText(TextFormat("x%.1f", 1.0f + p->chargeLevel), cCenter.x + radius + 5, cCenter.y, 10, GOLD);

        // Player emits static electricity based on the spell's charge!
        if (activeDNA.charge > 10.0f) {
            float t = GetTime() * 40.0f;
            float sparkRadius = radius + 5.0f;
            for(int i = 0; i < 3; i++) {
                float angle = (t + i * 2.0f);
                float ox = sinf(angle * 1.3f) * sparkRadius;
                float oy = cosf(angle * 0.7f) * sparkRadius;
                DrawLineEx(cCenter, (Vector2){cCenter.x + ox, cCenter.y + oy}, 2.0f, (Color){200, 150, 255, 255});
            }
        }
    }

    if (p->isCrafting && !p->showGuide) {
        DrawRectangle(200, 50, 420, 400, Fade(BLACK, 0.95f));
        DrawRectangleLines(200, 50, 420, 400, GOLD);
        DrawText("SIGIL DNA COMPILER", 220, 70, 20, GOLD);

        DrawText("SPELL FORM:", 250, 105, 10, WHITE);
        if(GuiButton((Rectangle){250, 120, 80, 25}, "PROJ")) draft->form = FORM_PROJECTILE;
        if(GuiButton((Rectangle){340, 120, 80, 25}, "MANIFEST")) draft->form = FORM_MANIFEST;
        if(GuiButton((Rectangle){430, 120, 80, 25}, "AURA")) draft->form = FORM_AURA;
        
        int fx = (draft->form == 0) ? 250 : (draft->form == 1) ? 340 : 430;
        DrawRectangleLines(fx, 120, 80, 25, GREEN);

        GuiSlider((Rectangle){250, 160, 200, 20}, "TEMP", NULL, &draft->temp, -100, 200);
        GuiSlider((Rectangle){250, 190, 200, 20}, "MASS", NULL, &draft->density, 0, 100);
        GuiSlider((Rectangle){250, 220, 200, 20}, "COHE", NULL, &draft->cohesion, 0, 100);
        GuiSlider((Rectangle){250, 250, 200, 20}, "WET ", NULL, &draft->moisture, 0, 100);
        GuiSlider((Rectangle){250, 280, 200, 20}, "CHRG", NULL, &draft->charge, 0, 100);
        GuiCheckBox((Rectangle){250, 320, 20, 20}, "ETERNAL", &draft->isPermanent);

        if (GuiButton((Rectangle){250, 360, 120, 40}, "IMPRINT")) {
            p->hotbar[p->activeSlot] = *draft;
            p->isCrafting = false;
        }
    }
}

void DrawGuideMenu(Player *p) {
    if (!p->showGuide) return;
    DrawRectangle(100, 50, 600, 350, Fade(DARKGRAY, 0.95f));
    DrawRectangleLines(100, 50, 600, 350, WHITE);
    DrawText("METSYS: ARCHITECTURE OF MAGIC", 120, 70, 20, GOLD);
    DrawText("--------------------------------------------------", 120, 90, 20, GRAY);
    
    DrawText("MECHANICS:", 120, 120, 15, SKYBLUE);
    DrawText("- CHARGING: Hold [Mouse 1] to multiply energy output.", 130, 145, 10, WHITE);
    DrawText("- Z-AXIS JUMPING: Jump over walls and avoid ground hazards.", 130, 160, 10, WHITE);
    DrawText("- PROCEDURAL MATH: Animations run on pure sine logic, no sprites.", 130, 175, 10, WHITE);

    DrawText("CONTROLS:", 120, 210, 15, SKYBLUE);
    DrawText("- [W, A, S, D]: Move  |  [SPACE]: Jump  |  [Mouse 1]: Hold to Cast", 130, 235, 10, WHITE);
    DrawText("- [SHIFT]: Toggle Cast Target (Ground vs Air)", 130, 250, 10, GREEN); 
    DrawText("- [ ` ]: Compiler   |   [ESC]: Guide", 130, 265, 10, WHITE);
    DrawText("- [ TAB ]: Quick-Swap Vision  |  [ [ ] & [ ] ]: Fine-tune Blend", 130, 280, 10, PURPLE);
}