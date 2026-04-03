#include "util.h"

int main() {
    // Standard USC-preferred resolution for USC CS projects
    InitWindow(800, 450, "Metsys: Emergent Field Simulation");
    SetTargetFPS(60);
    InitSimulation();

    // Ensure the player struct is initialized properly
    Player player = { 0 };
    player.pos = (Vector2){ 400, 225 };
    player.speed = 250.0f;
    player.activeElement = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // 1. INPUT HANDLING
        if (IsKeyPressed(KEY_ONE)) player.activeElement = 0;
        if (IsKeyPressed(KEY_TWO)) player.activeElement = 1;
        if (IsKeyPressed(KEY_THREE)) player.activeElement = 2;
        if (IsKeyPressed(KEY_FOUR)) player.activeElement = 3;

        // Corrected Movement Logic
        if (IsKeyDown(KEY_W)) player.pos.y -= player.speed * dt;
        if (IsKeyDown(KEY_S)) player.pos.y += player.speed * dt;
        if (IsKeyDown(KEY_A)) player.pos.x -= player.speed * dt;
        if (IsKeyDown(KEY_D)) player.pos.x += player.speed * dt;

        if (IsMouseButtonDown(0)) {
            Vector2 m = GetMousePosition();
            int gx = (int)(m.x / PIXEL_SIZE);
            int gy = (int)(m.y / PIXEL_SIZE);
            Cell energy = {0};
            
            if (player.activeElement == 0) energy.temp = 45.0f;
            if (player.activeElement == 1) energy.moisture = 30.0f;
            if (player.activeElement == 2) energy.density = 20.0f;
            if (player.activeElement == 3) energy.velocity = (Vector2){0, -10.0f};

            InjectEnergy(gx, gy, energy);
        }

        // 2. PHYSICS UPDATE
        UpdateSimulation(dt);

        // 3. RENDERING
        BeginDrawing();
            ClearBackground((Color){10, 10, 15, 255});
            
            DrawSimulation();
            
            // Draw Player
            DrawCircleV(player.pos, 6, RAYWHITE);
            DrawCircleLines(player.pos.x, player.pos.y, 6, GOLD);
            
            // Draw UI (Passing address of player)
            DrawInterface(&player);
            
            DrawFPS(720, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}