#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

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

void DrawMaterialRealm() {
    // Top-to-Bottom rendering for 2.5D Depth (Painter's Algorithm)
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell c = grid[LAYER_GROUND][y * WIDTH + x];
            Color color = GetCellColor(c);
            
            if (color.a > 0) {
                // 2.5D WALL RENDERING
                if (c.density > 60.0f && c.cohesion > 80.0f) {
                    int heightOffset = (int)(c.density / 10.0f); // Taller = Denser
                    // Draw South Face (Darker)
                    DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE + heightOffset, ColorBrightness(color, -0.4f));
                    // Draw Top Face (Lighter, shifted up)
                    DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE - heightOffset, PIXEL_SIZE, PIXEL_SIZE, ColorBrightness(color, 0.2f));
                } else {
                    // Flat floor rendering
                    DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, color);
                }
            }
        }
    }

    // Air Layer Shadows & Objects
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell airCell = grid[LAYER_AIR][y * WIDTH + x];
            if (airCell.density > 5.0f || airCell.temp > 40.0f) {
                DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, Fade(BLACK, 0.4f)); // Shadow
                Color airColor = GetCellColor(airCell);
                if (airColor.a > 0) DrawRectangle(x * PIXEL_SIZE, (y * PIXEL_SIZE) - FLOAT_OFFSET, PIXEL_SIZE, PIXEL_SIZE, airColor);
            }
        }
    }
}

void DrawEnergyRealm() {
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                Cell c = grid[z][y * WIDTH + x];
                if (c.density < 0.1f && fabsf(c.temp) < 0.1f && c.charge < 0.1f) continue;
                unsigned char r = (unsigned char)fmin(255, fmax(0, c.temp * 10));
                unsigned char g = (unsigned char)fmin(255, fmax(0, c.charge * 15));
                unsigned char b = (unsigned char)fmin(255, fmax(0, c.density * 10));
                int yOffset = (z == LAYER_AIR) ? FLOAT_OFFSET : 0;
                DrawRectangle(x * PIXEL_SIZE, (y * PIXEL_SIZE) - yOffset, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, 150});
            }
        }
    }
}

void DrawProjectiles() {
    for(int i=0; i<100; i++) {
        if(projectiles[i].active) {
            int yOffset = (projectiles[i].layer == LAYER_AIR) ? FLOAT_OFFSET : 0;
            DrawCircle(projectiles[i].pos.x, projectiles[i].pos.y - yOffset, 4, GOLD);
            DrawCircleLines(projectiles[i].pos.x, projectiles[i].pos.y - yOffset, 6, WHITE);
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

    Color targetColor = (p->castLayer == LAYER_AIR) ? SKYBLUE : BROWN;
    DrawText((p->castLayer == LAYER_AIR) ? "Z-TARGET: AIR [SPACE]" : "Z-TARGET: GROUND [SPACE]", 10, 145, 12, targetColor);
    DrawText("` = Craft | ESC = Guide", 10, 160, 10, GRAY);

    // CHARGE INDICATOR (Visually expands around player)
    if (p->isCharging) {
        float maxCharge = 3.0f;
        float radius = 10.0f + (p->chargeLevel / maxCharge) * 30.0f;
        DrawCircleLines(p->pos.x, p->pos.y, radius, SKYBLUE);
        DrawText(TextFormat("x%.1f", 1.0f + p->chargeLevel), p->pos.x + radius + 5, p->pos.y, 10, GOLD);
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
    DrawText("- CHARGING: Hold [Mouse 1] to multiply the energy output.", 130, 145, 10, WHITE);
    DrawText("- HEIGHT: Dense solids now render in 2.5D and block movement.", 130, 160, 10, WHITE);
    DrawText("- KINETIC CRASH: Dropping solids from the Air layer generates heat on impact.", 130, 175, 10, WHITE);

    DrawText("CONTROLS:", 120, 210, 15, SKYBLUE);
    DrawText("- [W, A, S, D]: Move  |  [Mouse 1]: Hold to Charge & Release to Cast", 130, 235, 10, WHITE);
    DrawText("- [SPACE]: Toggle Target (Ground vs Air)", 130, 250, 10, GREEN); 
    DrawText("- [ ` ]: Compiler  |  [TAB]: Energy Vision  |  [ESC]: Guide", 130, 265, 10, WHITE);
}