#include "util.h"

int main() {
    InitWindow(800, 450, "Metsys: Dual Realm Simulation");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); 
    InitSimulation();

    Player player = { 0 };
    player.pos = (Vector2){400, 225};
    player.speed = 200.0f;
    player.health = 100.0f;
    player.maxHealth = 100.0f;
    player.activeSlot = 0;
    player.castLayer = LAYER_GROUND; 
    player.chargeLevel = 0.0f;
    player.isCharging = false;
    
    SpellDNA draft = { 0 };
    draft.temp = 20.0f; 
    draft.form = FORM_PROJECTILE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ESCAPE)) player.showGuide = !player.showGuide;
        if (IsKeyPressed(KEY_GRAVE) && !player.showGuide) player.isCrafting = !player.isCrafting;
        if (IsKeyPressed(KEY_TAB)) player.energyVision = !player.energyVision;
        if (IsKeyPressed(KEY_SPACE)) player.castLayer = !player.castLayer; 

        if (!player.isCrafting && !player.showGuide && player.health > 0) {
            
            // Movement via Collision Helper
            Vector2 delta = {0, 0};
            if (IsKeyDown(KEY_W)) delta.y -= player.speed * dt;
            if (IsKeyDown(KEY_S)) delta.y += player.speed * dt;
            if (IsKeyDown(KEY_A)) delta.x -= player.speed * dt;
            if (IsKeyDown(KEY_D)) delta.x += player.speed * dt;
            MovePlayer(&player, delta);
            
            for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) player.activeSlot = i;

            // HOLD TO CAST MECHANIC
            if (IsMouseButtonDown(0)) {
                player.isCharging = true;
                player.chargeLevel += dt * 2.0f; // Charge speed
                if (player.chargeLevel > 3.0f) player.chargeLevel = 3.0f; // Max multiplier x4
            } else if (IsMouseButtonReleased(0)) {
                // Execute with multiplier (Base 1.0 + Charge)
                float multiplier = 1.0f + player.chargeLevel;
                ExecuteSpell(&player, GetMousePosition(), player.hotbar[player.activeSlot], multiplier);
                
                // Reset charge
                player.isCharging = false;
                player.chargeLevel = 0.0f;
            }
        }

        UpdateSimulation(dt, &player);

        BeginDrawing();
            ClearBackground((Color){20, 20, 25, 255}); 

            if (player.energyVision) DrawEnergyRealm();
            else DrawMaterialRealm();
            
            DrawProjectiles();
            
            if (player.health > 0) {
                DrawCircleV(player.pos, 6, RAYWHITE); 
            } else {
                DrawText("YOU DIED TO THE ELEMENTS", 250, 200, 20, RED);
            }
            
            DrawInterface(&player, &draft);
            DrawGuideMenu(&player);

            DrawFPS(720, 10);
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}