#include "util.h"

int main(void)
{
    InitWindow(screenWidth, screenHeight, "Magic II");
    SetTargetFPS(60); 

    gridWidth = screenWidth / PIXEL_SIZE;
    gridHeight = screenHeight / PIXEL_SIZE;
    canvas = (unsigned char *)malloc(gridWidth * gridHeight);
    memset(canvas, 0, gridWidth * gridHeight);

    MagicSlot slot;
    initMagicSlot(&slot);

    Player player = {0};
    player.pos = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };
    player.speed = 200.0f;
    player.activeSlot = 0; 
    player.activeCastType = 0; // Default to straight path
    
    SpellBlueprint draftSpell = {0};
    draftSpell.shape = SHAPE_PROJECTILE;

    while (!WindowShouldClose()) 
    {
        // 1. INPUTS
        if(IsKeyPressed(KEY_GRAVE)) { 
            player.isCrafting = !player.isCrafting;
        }

        // --- NEW: DRAG UI FOR MOVEMENT SELECTION ---
        if(IsKeyPressed(KEY_TAB)) {
            player.isSelectingMovement = true;
            player.dragCenter = GetMousePosition();
        }
        if(IsKeyReleased(KEY_TAB)) {
            player.isSelectingMovement = false;
        }

        for(int i = 0; i < 9; i++) {
            if(IsKeyPressed(KEY_ONE + i)) player.activeSlot = i;
        }
        if(IsKeyPressed(KEY_ZERO)) player.activeSlot = 9;

        if(!player.isCrafting && !player.isSelectingMovement) {
            // Left Click spawns orbiting mana
            if(IsKeyPressed(KEY_EQUAL)) castManaOrb(GetMousePosition());
            
            // Hold Space to leak mana onto canvas
            if(IsKeyDown(KEY_SPACE)) leakMana();

            // Press Enter to ACTIVATE the leaked area into Single-Pixel elements!
            if(IsMouseButtonPressed(0)) activateMana(&slot, &player, GetMousePosition());
            
            // Press - to clear world
            if(IsKeyPressed(KEY_MINUS)) {
                memset(canvas, 0, gridWidth * gridHeight);
                initMagicSlot(&slot);
            }
        }

        // 2. UPDATES
        updatePlayerMovement(&player);

        // 3. DRAW
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            updateWorld(); // Draws Canvas (Walls + Leaked Mana)
            updateManaOrbs(&player); // Animates orbiting mana, passes player for startPos
            updateSpellPhysics(&slot); // Moves and collides active single-pixel spells
            
            // Draw Player
            DrawCircleV(player.pos, 10, BLACK);
            DrawText("PLAYER", player.pos.x - 20, player.pos.y - 20, 10, DARKGRAY);

            displayUI(&player);
            displayCraftingTable(&player, &draftSpell);
            // magiCaft(player);
            displayMovementWheel(&player); // Draws the TAB drag UI overlay
            DrawText(GetFPS, 100, 100, 10, RED);
            DrawFPS(100, 100);
        EndDrawing();
    }

    free(canvas);
    CloseWindow();       
    return 0;
}