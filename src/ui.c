#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

void DrawSimulation() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Cell c = grid[y * WIDTH + x];
            if (c.density < 0.1f && fabsf(c.temp) < 0.1f) continue;
            unsigned char r = (unsigned char)fmin(255, c.temp * 10);
            unsigned char g = (unsigned char)fmin(255, c.density * 15);
            unsigned char b = (unsigned char)fmin(255, c.moisture * 20);
            DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, (Color){r, g, b, 255});
        }
    }
}

void DrawInterface(Player *p, SpellDNA *draft) {
    // HUD
    DrawRectangle(0, 0, 200, 100, Fade(BLACK, 0.8f));
    DrawText(TextFormat("Slot: %d", p->activeSlot + 1), 10, 10, 20, GOLD);
    DrawText("Press ` to Craft", 10, 40, 10, RAYWHITE);

    if (!p->isCrafting) return;

    // CRAFTING TABLE
    DrawRectangle(200, 50, 400, 350, Fade(BLACK, 0.9f));
    DrawRectangleLines(200, 50, 400, 350, GOLD);
    DrawText("SPELL DNA COMPILER", 220, 70, 20, GOLD);

    GuiSlider((Rectangle){250, 120, 200, 20}, "TEMP", NULL, &draft->temp, -100, 100);
    GuiSlider((Rectangle){250, 160, 200, 20}, "MASS", NULL, &draft->density, 0, 100);
    GuiSlider((Rectangle){250, 200, 200, 20}, "WET ", NULL, &draft->moisture, 0, 100);
    GuiCheckBox((Rectangle){250, 250, 20, 20}, "PERMANENT", &draft->isPermanent);

    if (GuiButton((Rectangle){250, 320, 100, 40}, "SAVE SLOT")) {
        p->hotbar[p->activeSlot] = *draft;
        p->isCrafting = false;
    }
}