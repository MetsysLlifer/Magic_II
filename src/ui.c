#include "util.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

void updateWorld(){
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int idx = y * gridWidth + x;
            if (canvas[idx] == CELL_SOLID) {
                DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, BROWN);
            } else if (canvas[idx] == CELL_MANA) {
                // Mana drops as a pure PIXEL_SIZE square now to match the particle aesthetics
                DrawRectangle(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, SKYBLUE);
            }
        }
    }
}

void displayUI(Player* player){
    int startX = screenWidth / 2 - (10 * 42) / 2;
    Vector2 mousePos = GetMousePosition();

    for(int i = 0; i < 10; i++) {
        int x = startX + (i * 42);
        int y = screenHeight - 50;
        
        Rectangle slotRect = {x, y, 40, 40};
        DrawRectangleLines(x, y, 40, 40, (player->activeSlot == i) ? GOLD : LIGHTGRAY);
        DrawText(TextFormat("%d", (i+1)%10), x + 4, y + 4, 10, GRAY);
        
        if(player->hotbar[i].raw_dna != 0) {
            DrawCircle(x + 20, y + 20, 10, getSpellColor(player->hotbar[i]));
        }

        if (CheckCollisionPointRec(mousePos, slotRect)) {
            DrawSpellTooltip(player->hotbar[i], (Vector2){x, y});
        }
    }
    
    // Display current movement style
    const char* moveTypes[] = {"Straight", "Sin Wave", "Cos Wave", "Rotate"};
    DrawText(TextFormat("Movement: %s (Hold TAB)", moveTypes[player->activeCastType]), 10, 10, 20, DARKGRAY);
}

// RADIAL DRAG MENU FOR MOVEMENTS
void displayMovementWheel(Player* player) {
    if(!player->isSelectingMovement) return;

    Vector2 center = player->dragCenter;
    Vector2 mouse = GetMousePosition();
    
    // Calculate angle from center drag to mouse
    float angle = atan2f(mouse.y - center.y, mouse.x - center.x) * RAD2DEG;
    if (angle < 0) angle += 360.0f;

    // Determine slice (0-3)
    int slice = 0;
    if (angle >= 45 && angle < 135) slice = 1;      // Bottom: Sin
    else if (angle >= 135 && angle < 225) slice = 2; // Left: Cos
    else if (angle >= 225 && angle < 315) slice = 3; // Top: Rotate
    else slice = 0;                                 // Right: Straight

    DrawCircle(center.x, center.y, 80, Fade(BLACK, 0.7f));
    DrawCircleLines(center.x, center.y, 80, GOLD);

    // Draw the 4 dividing lines
    DrawLineEx(center, (Vector2){center.x + 80, center.y + 80}, 2, GRAY);
    DrawLineEx(center, (Vector2){center.x - 80, center.y + 80}, 2, GRAY);
    DrawLineEx(center, (Vector2){center.x - 80, center.y - 80}, 2, GRAY);
    DrawLineEx(center, (Vector2){center.x + 80, center.y - 80}, 2, GRAY);

    // Labels
    DrawText("STRAIGHT", center.x + 30, center.y - 5, 10, (slice == 0) ? GOLD : WHITE);
    DrawText("SIN", center.x - 10, center.y + 50, 10, (slice == 1) ? GOLD : WHITE);
    DrawText("COS", center.x - 60, center.y - 5, 10, (slice == 2) ? GOLD : WHITE);
    DrawText("ROTATE", center.x - 20, center.y - 60, 10, (slice == 3) ? GOLD : WHITE);
    
    // Update player active cast type live as they drag
    player->activeCastType = slice;
}

void displayCraftingTable(Player* player, SpellBlueprint* draftSpell) {
    if(!player->isCrafting) return;

    DrawRectangle(50, 50, screenWidth - 100, screenHeight - 100, Fade(BLACK, 0.95f));
    DrawRectangleLines(50, 50, screenWidth - 100, screenHeight - 100, GOLD);
    DrawText("MAGICRAFTING TABLE - Spell DNA Compiler", 70, 70, 20, WHITE);
    
    bool f = draftSpell->traits.elements.fire;
    bool e = draftSpell->traits.elements.earth;
    bool w = draftSpell->traits.elements.water;
    bool a = draftSpell->traits.elements.air;
    bool sol = draftSpell->traits.matter.solid;
    bool flt = (draftSpell->traits.physics.gravity_dir == 5);
    bool bnc = draftSpell->traits.physics.bouncing;
    bool exp = draftSpell->traits.effects.combustible;

    GuiCheckBox((Rectangle){70, 120, 20, 20}, "Element: Fire", &f);
    GuiCheckBox((Rectangle){70, 150, 20, 20}, "Element: Earth", &e);
    GuiCheckBox((Rectangle){70, 180, 20, 20}, "Element: Water", &w);
    GuiCheckBox((Rectangle){70, 210, 20, 20}, "Element: Air", &a);

    GuiCheckBox((Rectangle){250, 120, 20, 20}, "Matter: Solid", &sol);
    GuiCheckBox((Rectangle){250, 150, 20, 20}, "Physics: Float (Glow)", &flt);
    GuiCheckBox((Rectangle){250, 180, 20, 20}, "Physics: Bounce", &bnc);
    GuiCheckBox((Rectangle){250, 210, 20, 20}, "Effect: Explode", &exp);

    draftSpell->traits.elements.fire = f;
    draftSpell->traits.elements.earth = e;
    draftSpell->traits.elements.water = w;
    draftSpell->traits.elements.air = a;
    draftSpell->traits.matter.solid = sol;
    draftSpell->traits.physics.gravity_dir = flt ? 5 : 100;
    draftSpell->traits.physics.bouncing = bnc;
    draftSpell->traits.effects.combustible = exp;

    if(GuiButton((Rectangle){450, 120, 80, 30}, "Shape: Proj")) draftSpell->shape = SHAPE_PROJECTILE;
    if(GuiButton((Rectangle){450, 160, 80, 30}, "Shape: Wall")) draftSpell->shape = SHAPE_WALL;

    DrawCircle(650, 150, 25, getSpellColor(*draftSpell));
    DrawSpellTooltip(*draftSpell, (Vector2){600, 300});

    DrawText("Save to Slot:", 70, 300, 10, WHITE);
    for(int i=0; i<10; i++) {
        if(GuiButton((Rectangle){70 + i*40, 320, 35, 35}, TextFormat("%d", (i+1)%10))) {
            player->hotbar[i] = *draftSpell; 
            player->hotbar[i].lifespan_seconds = 6.0f;
        }
    }
    
    if(GuiButton((Rectangle){450, 320, 100, 35}, "CLEAR DRAFT")) {
        draftSpell->raw_dna = 0; 
        draftSpell->shape = SHAPE_PROJECTILE;
    }
}

void magiCraft(Player player){
    if(!player.isCrafting) return;

    Vector2 center = {screenWidth/2, screenHeight/2};
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 200});

    // Core and Aux
    DrawCircleLinesV(center, 50, WHITE);
    DrawCircleLinesV(center, 100, WHITE);
    DrawCircleLinesV(center, 150, WHITE);
}