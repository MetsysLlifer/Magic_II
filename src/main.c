#include "util.h"

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "Metsys: Open World Ecosystem");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); 
    InitSimulation();
    InitNPCs();

    RenderTexture2D target = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    Player player = { 0 };
    player.pos = (Vector2){WORLD_W/2, WORLD_H/2}; 
    player.z = 0.0f;
    player.speed = 200.0f;
    player.health = 100.0f;
    player.maxHealth = 100.0f;
    player.activeSlot = 0;
    player.castLayer = LAYER_GROUND; 
    player.craftCategory = 0;
    player.draggingNodeId = -1;
    player.friendlyFire = false; 
    for(int i=0; i<10; i++) player.editStates[i] = false;
    
    for(int i=0; i<10; i++) {
        player.hotbar[i].type = ITEM_SPELL;
        for(int j=0; j<MAX_NODES; j++) player.hotbar[i].spell.graph.nodes[j].active = false;
        
        player.hotbar[i].spell.graph.nodes[0].active = true;
        player.hotbar[i].spell.graph.nodes[0].parentId = -1;
        player.hotbar[i].spell.graph.nodes[0].pos = (Vector2){0, 0};
        player.hotbar[i].spell.graph.nodes[0].temp = 20.0f;
        player.hotbar[i].spell.graph.nodes[0].movement = MOVE_STRAIGHT;
        
        player.hotbar[i].spell.graph.nodes[0].hasSpeed = false;
        player.hotbar[i].spell.graph.nodes[0].hasDelay = false;
        player.hotbar[i].spell.graph.nodes[0].hasDistort = false;
        player.hotbar[i].spell.graph.nodes[0].hasRange = false;
        player.hotbar[i].spell.graph.nodes[0].hasSize = false;
        player.hotbar[i].spell.graph.nodes[0].hasSpread = false;

        player.hotbar[i].spell.graph.nodes[0].speedMod = 1.0f;
        player.hotbar[i].spell.graph.nodes[0].easeTime = 0.0f;
        player.hotbar[i].spell.graph.nodes[0].delay = 0.0f;
        player.hotbar[i].spell.graph.nodes[0].distortion = 0.0f;
        player.hotbar[i].spell.graph.nodes[0].rangeMod = 1.0f;
        player.hotbar[i].spell.graph.nodes[0].sizeMod = 1.0f;
        player.hotbar[i].spell.graph.nodes[0].spreadType = SPREAD_OFF;
        
        player.hotbar[i].spell.form = FORM_PROJECTILE;
    }

    player.selectedNodeId = 0;
    
    player.worldCamera.target = player.pos;
    player.worldCamera.offset = (Vector2){SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    player.worldCamera.rotation = 0.0f;
    player.worldCamera.zoom = 1.0f;

    player.craftCamera.target = (Vector2){0, 0};
    player.craftCamera.offset = (Vector2){440, 230}; 
    player.craftCamera.zoom = 1.0f;
    
    NPCDNA draftNPC = { 50, 0, 0, 50, 50, 0 };
    bool wantsRestart = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_F11)) {
            if (!IsWindowFullscreen()) {
                int display = GetCurrentMonitor();
                SetWindowSize(GetMonitorWidth(display), GetMonitorHeight(display));
                ToggleFullscreen();
            } else {
                ToggleFullscreen();
                SetWindowSize(SCREEN_W, SCREEN_H);
            }
        }
        
        float scale = fminf((float)GetScreenWidth() / SCREEN_W, (float)GetScreenHeight() / SCREEN_H);
        
        Vector2 mousePos = GetMousePosition();
        Vector2 virtualMouse = { 0 };
        virtualMouse.x = (mousePos.x - (GetScreenWidth() - (SCREEN_W * scale)) * 0.5f) / scale;
        virtualMouse.y = (mousePos.y - (GetScreenHeight() - (SCREEN_H * scale)) * 0.5f) / scale;
        virtualMouse.x = fmaxf(0.0f, fminf(virtualMouse.x, SCREEN_W));
        virtualMouse.y = fmaxf(0.0f, fminf(virtualMouse.y, SCREEN_H));

        if (wantsRestart) {
            ResetGame(&player);
            wantsRestart = false;
        }

        if (IsKeyPressed(KEY_ESCAPE)) player.showGuide = !player.showGuide;
        if (IsKeyPressed(KEY_GRAVE) && !player.showGuide && player.health > 0) {
            player.isCrafting = !player.isCrafting;
            player.draggingNodeId = -1; 
        }
        
        if (IsKeyPressed(KEY_TAB)) {
            if (player.isCrafting) {
                player.craftCategory = (player.craftCategory == 0) ? 1 : 0; 
            } else {
                if (player.visionBlend < 0.5f) player.visionBlend = 1.0f;
                else if (player.visionBlend < 1.5f) player.visionBlend = 2.0f;
                else player.visionBlend = 0.0f;
            }
        }
        
        if (IsKeyPressed(KEY_LEFT_SHIFT)) player.castLayer = !player.castLayer; 
        if (IsKeyDown(KEY_RIGHT_BRACKET)) player.visionBlend = fminf(2.0f, player.visionBlend + dt * 1.5f);
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
            Vector2 worldMouse = GetScreenToWorld2D(virtualMouse, player.worldCamera);

            if (active->type == ITEM_SPELL) {
                
                // DUAL CHANNEL CASTING LOGIC
                bool holdingLMB = IsMouseButtonDown(0);
                bool holdingC = IsKeyDown(KEY_C);

                if (holdingLMB) {
                    player.isCharging = true;
                    player.chargeLevel += dt * 2.0f; 
                    if (player.chargeLevel > 4.0f) player.chargeLevel = 4.0f; 
                } else {
                    player.isCharging = false;
                    player.chargeLevel -= dt * 4.0f; 
                    if (player.chargeLevel < 0.0f) player.chargeLevel = 0.0f; 
                }

                if (holdingC) {
                    player.isLifespanCharging = true;
                    player.lifespanLevel += dt * 2.0f; 
                    if (player.lifespanLevel > 4.0f) player.lifespanLevel = 4.0f; 
                } else {
                    player.isLifespanCharging = false;
                    player.lifespanLevel -= dt * 4.0f; 
                    if (player.lifespanLevel < 0.0f) player.lifespanLevel = 0.0f; 
                }

                // Trigger cast on LMB Release!
                if (IsMouseButtonReleased(0)) {
                    ExecuteSpell(&player, worldMouse, &active->spell, 1.0f + player.chargeLevel, 1.0f + player.lifespanLevel);
                    player.chargeLevel = 0.0f;
                    player.lifespanLevel = 0.0f;
                }
            } 
            else if (active->type == ITEM_NPC) {
                if (IsMouseButtonPressed(0)) SpawnNPC(worldMouse, active->npc);
            }
        }

        player.worldCamera.target.x += (player.pos.x - player.worldCamera.target.x) * 5.0f * dt;
        player.worldCamera.target.y += (player.pos.y - player.worldCamera.target.y) * 5.0f * dt;

        UpdateSimulation(dt, &player);
        UpdateNPCs(dt, &player);

        BeginTextureMode(target);
            ClearBackground((Color){20, 20, 25, 255}); 

            float matAlpha = 1.0f - (fminf(player.visionBlend, 2.0f) * 0.4f); 
            float nrgAlpha = 1.0f - fabsf(player.visionBlend - 1.0f);         
            float hazAlpha = fmaxf(0.0f, player.visionBlend - 1.0f);          

            BeginMode2D(player.worldCamera); 
                DrawMaterialRealm(matAlpha); 
                if (nrgAlpha > 0.01f) DrawEnergyRealm(nrgAlpha);
                if (hazAlpha > 0.01f) DrawHazardRealm(hazAlpha);
                
                DrawSingularities(1.0f); // Render the Space-Time anomalies!

                DrawNPCs(); 
                DrawProjectiles(&player);
                DrawPlayerEntity(&player); 
            EndMode2D();
            
            if (!player.isCrafting && !player.showGuide && player.health > 0 && player.hotbar[player.activeSlot].type == ITEM_NPC) {
                DrawProceduralNPC(virtualMouse, 0, player.hotbar[player.activeSlot].npc, 0.5f); 
                DrawText("CLICK TO DEPLOY", virtualMouse.x + 20, virtualMouse.y, 10, GREEN);
            }

            if (player.health <= 0) {
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.8f));
                DrawText("YOU DIED TO THE ELEMENTS", 240, 180, 20, RED);
                DrawText("Press [ ` ] to configure spells before restarting.", 230, 210, 15, GRAY);
            }
            
            DrawInterface(&player, &draftNPC, virtualMouse);
            DrawGuideMenu(&player, &wantsRestart);

        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK); 
            Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height };
            Rectangle destRec = { 
                (GetScreenWidth() - ((float)SCREEN_W * scale)) * 0.5f, 
                (GetScreenHeight() - ((float)SCREEN_H * scale)) * 0.5f, 
                (float)SCREEN_W * scale, 
                (float)SCREEN_H * scale 
            };
            DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }
    
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}