#include "util.h"

int main() {
    InitWindow(800, 450, "Metsys: Dual Realm Simulation");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Prevents ESC from closing the game immediately
    InitSimulation();

    Player player = { 0 };
    player.pos = (Vector2){400, 225};
    player.speed = 250.0f;
    player.activeSlot = 0;
    
    SpellDNA draft = { 0 };
    draft.temp = 20.0f; // Default room temp

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // 1. SYSTEM TOGGLES
        if (IsKeyPressed(KEY_ESCAPE)) player.showGuide = !player.showGuide;
        if (IsKeyPressed(KEY_GRAVE) && !player.showGuide) player.isCrafting = !player.isCrafting;
        if (IsKeyPressed(KEY_TAB)) player.energyVision = !player.energyVision;

        // 2. WORLD INTERACTION (Disabled if menus are open)
        if (!player.isCrafting && !player.showGuide) {
            if (IsKeyDown(KEY_W)) player.pos.y -= player.speed * dt;
            if (IsKeyDown(KEY_S)) player.pos.y += player.speed * dt;
            if (IsKeyDown(KEY_A)) player.pos.x -= player.speed * dt;
            if (IsKeyDown(KEY_D)) player.pos.x += player.speed * dt;
            
            for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) player.activeSlot = i;

            if (IsMouseButtonDown(0)) {
                Vector2 m = GetMousePosition();
                InjectEnergy((int)(m.x / PIXEL_SIZE), (int)(m.y / PIXEL_SIZE), player.hotbar[player.activeSlot]);
            }
        }

        // 3. PHYSICS PROCESSING
        UpdateSimulation(dt);

        // 4. RENDERING PIPELINE
        BeginDrawing();
            ClearBackground(BLACK); // Vacuum

            if (player.energyVision) DrawEnergyRealm();
            else DrawMaterialRealm();
            
            DrawCircleV(player.pos, 5, RAYWHITE); // Player Indicator
            
            DrawInterface(&player, &draft);
            DrawGuideMenu(&player);

            DrawFPS(720, 10);
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}