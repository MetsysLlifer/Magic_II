#include "util.h"

int main() {
    InitWindow(800, 450, "Metsys: Emergent World");
    SetTargetFPS(60);
    InitSimulation();

    Player player = { .pos = {400, 225}, .speed = 200.0f, .activeSlot = 0, .isCrafting = false };
    SpellDNA draft = { 0 };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (IsKeyPressed(KEY_GRAVE)) player.isCrafting = !player.isCrafting;

        if (!player.isCrafting) {
            if (IsKeyDown(KEY_W)) player.pos.y -= player.speed * dt;
            if (IsKeyDown(KEY_S)) player.pos.y += player.speed * dt;
            if (IsKeyDown(KEY_A)) player.pos.x -= player.speed * dt;
            if (IsKeyDown(KEY_D)) player.pos.x += player.speed * dt;
            for (int i = 0; i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) player.activeSlot = i;

            if (IsMouseButtonDown(0)) {
                Vector2 m = GetMousePosition();
                InjectEnergy(m.x / PIXEL_SIZE, m.y / PIXEL_SIZE, player.hotbar[player.activeSlot]);
            }
        }

        UpdateSimulation(dt);

        BeginDrawing();
            ClearBackground((Color){5, 5, 10, 255});
            DrawSimulation();
            DrawCircleV(player.pos, 5, WHITE);
            DrawInterface(&player, &draft);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}