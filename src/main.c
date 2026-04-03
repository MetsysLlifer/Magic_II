#include "util.h"

int main() {
    InitWindow(800, 450, "Metsys: Ecosystem Engine");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); 
    InitSimulation();
    InitNPCs();

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
    player.craftCategory = 0;
    
    for(int i=0; i<10; i++) player.hotbar[i].type = ITEM_SPELL;
    
    for(int i=0; i<MAX_NODES; i++) player.sigil.nodes[i].active = false;
    
    player.sigil.nodes[0].active = true;
    player.sigil.nodes[0].parentId = -1;
    player.sigil.nodes[0].pos = (Vector2){0, 0};
    player.sigil.nodes[0].temp = 20.0f;
    player.sigil.nodes[0].movement = MOVE_STRAIGHT;
    player.selectedNodeId = 0;

    player.craftCamera.target = (Vector2){0, 0};
    player.craftCamera.offset = (Vector2){495, 230}; 
    player.craftCamera.rotation = 0.0f;
    player.craftCamera.zoom = 1.0f;
    
    SpellDNA draftSpell = { 0 };
    NPCDNA draftNPC = { 50, 0, 0, 50, 50, 0 };
    player.selectedForm = FORM_PROJECTILE;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ESCAPE)) player.showGuide = !player.showGuide;
        if (IsKeyPressed(KEY_GRAVE) && !player.showGuide) player.isCrafting = !player.isCrafting;
        
        if (IsKeyPressed(KEY_TAB)) {
            if (player.isCrafting) {
                player.craftCategory = (player.craftCategory == 0) ? 1 : 0; 
            } else {
                player.visionBlend = (player.visionBlend > 0.5f) ? 0.0f : 1.0f; 
            }
        }
        
        if (IsKeyPressed(KEY_LEFT_SHIFT)) player.castLayer = !player.castLayer; 
        if (IsKeyDown(KEY_RIGHT_BRACKET)) player.visionBlend = fminf(1.0f, player.visionBlend + dt * 1.5f);
        if (IsKeyDown(KEY_LEFT_BRACKET)) player.visionBlend = fmaxf(0.0f, player.visionBlend - dt * 1.5f);

        if (!player.isCrafting && !player.showGuide && player.health > 0) {
            
            if (IsKeyPressed(KEY_SPACE) && !player.isJumping) {
                player.isJumping = true; player.zVelocity = 250.0f; 
            }
            Vector2 delta = {0, 0};
            if (IsKeyDown(KEY_W)) delta.y -= player.speed * dt;
            if (IsKeyDown(KEY_S)) delta.y += player.speed * dt;
            if (IsKeyDown(KEY_A)) delta.x -= player.speed * dt;
            if (IsKeyDown(KEY_D)) delta.x += player.speed * dt;
            MovePlayer(&player, delta, dt); 
            
            for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) player.activeSlot = i;
            if (IsKeyPressed(KEY_ZERO)) player.activeSlot = 9;

            HotbarSlot *active = &player.hotbar[player.activeSlot];

            if (active->type == ITEM_SPELL) {
                if (IsMouseButtonDown(0)) {
                    player.isCharging = true;
                    player.chargeLevel += dt * 2.0f; 
                    if (player.chargeLevel > 3.0f) player.chargeLevel = 3.0f; 
                } else if (IsMouseButtonReleased(0)) {
                    ExecuteSpell(&player, GetMousePosition(), active->spell, 1.0f + player.chargeLevel);
                    player.isCharging = false;
                    player.chargeLevel = 0.0f;
                }
            } 
            else if (active->type == ITEM_NPC) {
                if (IsMouseButtonPressed(0)) {
                    SpawnNPC(GetMousePosition(), active->npc);
                }
            }
        }

        UpdateSimulation(dt, &player);
        UpdateNPCs(dt, &player);

        BeginDrawing();
            ClearBackground((Color){20, 20, 25, 255}); 

            DrawMaterialRealm(1.0f - (player.visionBlend * 0.8f)); 
            DrawEnergyRealm(player.visionBlend);
            
            DrawNPCs(); 
            DrawProjectiles(&player);
            DrawPlayerEntity(&player); 
            
            if (!player.isCrafting && !player.showGuide && player.hotbar[player.activeSlot].type == ITEM_NPC) {
                Vector2 mPos = GetMousePosition();
                DrawProceduralNPC(mPos, 0, player.hotbar[player.activeSlot].npc, 0.5f); 
                DrawText("CLICK TO DEPLOY", mPos.x + 20, mPos.y, 10, GREEN);
            }

            if (player.health <= 0) DrawText("YOU DIED TO THE ELEMENTS", 250, 200, 20, RED);
            
            DrawInterface(&player, &draftSpell, &draftNPC);
            DrawGuideMenu(&player);

        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}