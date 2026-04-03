#include "util.h"

int main() {
    InitWindow(800, 450, "Metsys: Procedural Simulation");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); 
    InitSimulation();

    Player player = { 0 };
    player.pos = (Vector2){400, 225};
    player.z = 0.0f;
    player.zVelocity = 0.0f;
    player.isJumping = false;
    player.animTime = 0.0f;

    player.speed = 200.0f;
    player.health = 100.0f;
    player.maxHealth = 100.0f;
    player.activeSlot = 0;
    player.castLayer = LAYER_GROUND; 
    player.chargeLevel = 0.0f;
    player.isCharging = false;
    player.visionBlend = 0.0f; 
    
    SpellDNA draft = { 0 };
    draft.temp = 20.0f; 
    draft.form = FORM_PROJECTILE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ESCAPE)) player.showGuide = !player.showGuide;
        if (IsKeyPressed(KEY_GRAVE) && !player.showGuide) player.isCrafting = !player.isCrafting;
        if (IsKeyPressed(KEY_LEFT_SHIFT)) player.castLayer = !player.castLayer; 

        if (IsKeyPressed(KEY_TAB)) player.visionBlend = (player.visionBlend > 0.5f) ? 0.0f : 1.0f;
        if (IsKeyDown(KEY_RIGHT_BRACKET)) player.visionBlend = fminf(1.0f, player.visionBlend + dt * 1.5f);
        if (IsKeyDown(KEY_LEFT_BRACKET)) player.visionBlend = fmaxf(0.0f, player.visionBlend - dt * 1.5f);

        if (!player.isCrafting && !player.showGuide && player.health > 0) {
            
            // JUMP LOGIC
            if (IsKeyPressed(KEY_SPACE) && !player.isJumping) {
                player.isJumping = true;
                player.zVelocity = 250.0f; // Initial jump impulse
            }

            Vector2 delta = {0, 0};
            if (IsKeyDown(KEY_W)) delta.y -= player.speed * dt;
            if (IsKeyDown(KEY_S)) delta.y += player.speed * dt;
            if (IsKeyDown(KEY_A)) delta.x -= player.speed * dt;
            if (IsKeyDown(KEY_D)) delta.x += player.speed * dt;
            MovePlayer(&player, delta, dt); // Now takes dt for gravity
            
            for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) player.activeSlot = i;

            if (IsMouseButtonDown(0)) {
                player.isCharging = true;
                player.chargeLevel += dt * 2.0f; 
                if (player.chargeLevel > 3.0f) player.chargeLevel = 3.0f; 
            } else if (IsMouseButtonReleased(0)) {
                float multiplier = 1.0f + player.chargeLevel;
                ExecuteSpell(&player, GetMousePosition(), player.hotbar[player.activeSlot], multiplier);
                player.isCharging = false;
                player.chargeLevel = 0.0f;
            }
        }

        UpdateSimulation(dt, &player);

        BeginDrawing();
            ClearBackground((Color){20, 20, 25, 255}); 

            DrawMaterialRealm(1.0f - (player.visionBlend * 0.8f)); 
            DrawEnergyRealm(player.visionBlend);
            DrawProjectiles();
            
            // DRAW PROCEDURAL PLAYER ANIMATION
            if (player.health > 0) {
                // Base shadow
                DrawEllipse(player.pos.x, player.pos.y, 6, 3, Fade(BLACK, 0.6f));
                
                // Procedural Squash & Stretch based on Z-Velocity
                float stretch = 1.0f + (player.zVelocity / 1000.0f); // Stretches when moving fast vertically
                float squash = 1.0f / stretch;
                
                // Idle breathing applied to width
                float breathe = (sinf(player.animTime * 4.0f) + 1.0f) * 0.5f;
                float currentWidth = (6.0f * squash) + (breathe * 1.5f);
                float currentHeight = 6.0f * stretch;

                DrawEllipse(player.pos.x, player.pos.y - player.z, currentWidth, currentHeight, RAYWHITE); 
            } else {
                DrawText("YOU DIED TO THE ELEMENTS", 250, 200, 20, RED);
            }
            
            DrawInterface(&player, &draft);
            DrawGuideMenu(&player);

        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}