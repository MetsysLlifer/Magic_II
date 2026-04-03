#include "util.h"

int main() {
    InitWindow(800, 450, "Metsys: Infinite Spell Compiler");
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
    
    // Initialize the Infinite Canvas Data
    for(int i=0; i<MAX_NODES; i++) player.sigil.nodes[i].active = false;
    
    // Setup Node 0 as the un-deletable CORE
    player.sigil.nodes[0].active = true;
    player.sigil.nodes[0].parentId = -1;
    player.sigil.nodes[0].pos = (Vector2){0, 0};
    player.sigil.nodes[0].temp = 20.0f;
    player.sigil.nodes[0].movement = MOVE_STRAIGHT;
    player.selectedNodeId = 0;

    // FIX: Properly center the 2D Camera inside the Right Panel
    // The panel starts at X=250 with Width=500. Center is 250 + 250 = 500.
    // The panel starts at Y=50 with Height=360. Center is 50 + 180 = 230.
    player.craftCamera.target = (Vector2){0, 0};
    player.craftCamera.offset = (Vector2){500, 230}; 
    player.craftCamera.rotation = 0.0f;
    player.craftCamera.zoom = 1.0f;
    
    SpellDNA draft = { 0 };
    player.selectedForm = FORM_PROJECTILE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ESCAPE)) player.showGuide = !player.showGuide;
        if (IsKeyPressed(KEY_GRAVE) && !player.showGuide) player.isCrafting = !player.isCrafting;
        if (IsKeyPressed(KEY_LEFT_SHIFT)) player.castLayer = !player.castLayer; 

        if (IsKeyPressed(KEY_TAB)) player.visionBlend = (player.visionBlend > 0.5f) ? 0.0f : 1.0f;
        if (IsKeyDown(KEY_RIGHT_BRACKET)) player.visionBlend = fminf(1.0f, player.visionBlend + dt * 1.5f);
        if (IsKeyDown(KEY_LEFT_BRACKET)) player.visionBlend = fmaxf(0.0f, player.visionBlend - dt * 1.5f);

        if (!player.isCrafting && !player.showGuide && player.health > 0) {
            
            if (IsKeyPressed(KEY_SPACE) && !player.isJumping) {
                player.isJumping = true;
                player.zVelocity = 250.0f; 
            }

            Vector2 delta = {0, 0};
            if (IsKeyDown(KEY_W)) delta.y -= player.speed * dt;
            if (IsKeyDown(KEY_S)) delta.y += player.speed * dt;
            if (IsKeyDown(KEY_A)) delta.x -= player.speed * dt;
            if (IsKeyDown(KEY_D)) delta.x += player.speed * dt;
            MovePlayer(&player, delta, dt); 
            
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
            
            DrawProjectiles(&player);
            DrawPlayerEntity(&player); 
            
            if (player.health <= 0) DrawText("YOU DIED TO THE ELEMENTS", 250, 200, 20, RED);
            
            DrawInterface(&player, &draft);
            DrawGuideMenu(&player);

        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}