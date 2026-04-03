#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// -------------------------------------------------------------
// REALM VIEWERS
// -------------------------------------------------------------
void DrawMaterialRealm() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell c = grid[y * WIDTH + x];
            if (c.density < 2.0f && c.temp < 30.0f) continue; // Empty space
            
            Color renderColor = BLANK;

            // 1. SOLIDS (High Density, High Cohesion)
            if (c.density > 40.0f && c.cohesion > 60.0f) {
                if (c.temp < 0.0f) renderColor = SKYBLUE;        // Ice
                else renderColor = DARKGRAY;                     // Rock/Earth
            }
            // 2. LIQUIDS (High Density, Low Cohesion)
            else if (c.density > 20.0f && c.cohesion <= 60.0f) {
                if (c.temp > 70.0f) renderColor = ORANGE;        // Lava
                else if (c.moisture > 30.0f) renderColor = BLUE; // Water
                else renderColor = BROWN;                        // Mud/Sludge
            }
            // 3. GASES / PLASMA (Low Density)
            else {
                if (c.temp > 70.0f) renderColor = RED;           // Fire/Plasma
                else if (c.charge > 50.0f) renderColor = PURPLE; // Spark/Static cloud
                else renderColor = Fade(LIGHTGRAY, c.density / 20.0f); // Smoke/Steam
            }

            DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, renderColor);
        }
    }
}

void DrawEnergyRealm() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell c = grid[y * WIDTH + x];
            if (c.density < 0.1f && fabsf(c.temp) < 0.1f && c.charge < 0.1f) continue;

            unsigned char r = (unsigned char)fmin(255, fmax(0, c.temp * 10));
            unsigned char g = (unsigned char)fmin(255, fmax(0, c.charge * 15));
            unsigned char b = (unsigned char)fmin(255, fmax(0, c.density * 10));
            
            DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, 255});
        }
    }
}

// -------------------------------------------------------------
// MENUS & UI
// -------------------------------------------------------------
void DrawInterface(Player *p, SpellDNA *draft) {
    // Top-left HUD
    DrawText(TextFormat("SLOT: %d", p->activeSlot + 1), 10, 10, 20, GOLD);
    DrawText(p->energyVision ? "VISION: ENERGY REALM (Tab)" : "VISION: MATERIAL REALM (Tab)", 10, 35, 10, SKYBLUE);
    DrawText("` = Sigil Crafting | ESC = Guide", 10, 50, 10, GRAY);

    // Sigil Sequencer
    if (p->isCrafting && !p->showGuide) {
        DrawRectangle(200, 50, 400, 360, Fade(BLACK, 0.95f));
        DrawRectangleLines(200, 50, 400, 360, GOLD);
        DrawText("SIGIL DNA COMPILER", 220, 70, 20, GOLD);

        GuiSlider((Rectangle){250, 110, 200, 20}, "TEMP (Heat)", NULL, &draft->temp, -100, 200);
        GuiSlider((Rectangle){250, 140, 200, 20}, "DENS (Mass)", NULL, &draft->density, 0, 100);
        GuiSlider((Rectangle){250, 170, 200, 20}, "COHE (Bind)", NULL, &draft->cohesion, 0, 100);
        GuiSlider((Rectangle){250, 200, 200, 20}, "WET  (Fluid)", NULL, &draft->moisture, 0, 100);
        GuiSlider((Rectangle){250, 230, 200, 20}, "CHRG (Volt)", NULL, &draft->charge, 0, 100);
        
        GuiCheckBox((Rectangle){250, 270, 20, 20}, "ETERNAL (Permanent)", &draft->isPermanent);

        if (GuiButton((Rectangle){250, 330, 120, 40}, "IMPRINT TO SLOT")) {
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
    
    DrawText("CONTROLS:", 120, 120, 15, SKYBLUE);
    DrawText("- [W, A, S, D]: Move the observer.", 130, 145, 10, WHITE);
    DrawText("- [Mouse 1]: Inject the active slot's DNA into the world.", 130, 160, 10, WHITE);
    DrawText("- [1 - 9]: Select active hotbar slot.", 130, 175, 10, WHITE);
    DrawText("- [ ` ] (Backtick): Open the Sigil DNA Compiler.", 130, 190, 10, WHITE);
    DrawText("- [TAB]: Toggle between Material and Energy Vision.", 130, 205, 10, WHITE);

    DrawText("THE PHYSICS OF MAGIC:", 120, 235, 15, SKYBLUE);
    DrawText("- LAVA: High Temp + High Density + Low Cohesion", 130, 260, 10, ORANGE);
    DrawText("- ICE: Low Temp + High Density + High Cohesion", 130, 275, 10, LIGHTGRAY);
    DrawText("- ROCK: Room Temp + High Density + High Cohesion", 130, 290, 10, DARKGRAY);
    DrawText("- FIRE: High Temp + Low Density", 130, 305, 10, RED);
    
    DrawText("Press ESC to return to reality.", 120, 370, 10, GRAY);
}