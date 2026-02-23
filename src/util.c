#include "util.h"

int gridWidth;
int gridHeight;
Area *canvas;
ManaOrb manaOrbs[MAX_ORBS];

Color getSpellColor(SpellBlueprint bp) {
    int r = 0, g = 0, b = 0, count = 0;
    if (bp.traits.elements.fire)  { r += 230; g += 41;  b += 55;  count++; }
    if (bp.traits.elements.earth) { r += 130; g += 80;  b += 40;  count++; }
    if (bp.traits.elements.water) { r += 0;   g += 121; b += 241; count++; }
    if (bp.traits.elements.air)   { r += 200; g += 200; b += 200; count++; }
    if (count == 0) return MAGENTA; 
    return (Color){r/count, g/count, b/count, 255};
}

void initMagicSlot(MagicSlot* slot) {
    slot->count = 0;
    for(int i=0; i<MAX; i++) slot->Elements[i].isActive = false;
    for(int i=0; i<MAX_ORBS; i++) manaOrbs[i].active = false;
}

void castManaOrb(Vector2 mousePos) {
    for(int i=0; i<MAX_ORBS; i++) {
        if(!manaOrbs[i].active) {
            manaOrbs[i].startPos = mousePos;
            manaOrbs[i].angle = (float)GetRandomValue(0, 314) / 100.0f;
            manaOrbs[i].active = true;
            break;
        }
    }
}

void updateManaOrbs(Player* player) {
    float maxRadius = 40.0f;
    for(int i=0; i<MAX_ORBS; i++) {
        if(!manaOrbs[i].active) continue;
        
        manaOrbs[i].startPos = player->pos; 
        manaOrbs[i].angle += GetFrameTime() * 3.0f;
        
        float x = manaOrbs[i].startPos.x + cosf(manaOrbs[i].angle) * maxRadius;
        float y = manaOrbs[i].startPos.y + sinf(manaOrbs[i].angle) * maxRadius;
        
        DrawCircle(x, y, 4, MAGENTA);
    }
}

void leakMana() {
    float maxRadius = 40.0f;
    for(int i=0; i<MAX_ORBS; i++) {
        if(!manaOrbs[i].active) continue;
        float x = manaOrbs[i].startPos.x + cosf(manaOrbs[i].angle) * maxRadius;
        float y = manaOrbs[i].startPos.y + sinf(manaOrbs[i].angle) * maxRadius;
        
        int gX = x / PIXEL_SIZE;
        int gY = y / PIXEL_SIZE;
        if (gX >= 0 && gX < gridWidth && gY >= 0 && gY < gridHeight) {
            if (canvas[gY * gridWidth + gX] == CELL_EMPTY) {
                canvas[gY * gridWidth + gX] = CELL_MANA; 
            }
        }
    }
}

void spawnElement(MagicSlot* slot, SpellBlueprint bp, int castType, Vector2 pos, Vector2 targetMouse, int groupId) {
    int index = -1;
    for(int i=0; i<MAX; i++) {
        if(!slot->Elements[i].isActive) { index = i; break; }
    }
    if(index == -1) return; // Reached max pixels!

    Element* el = &slot->Elements[index];
    el->pos = pos;
    el->startPos = pos;
    el->blueprint = bp;
    el->isActive = true;
    el->animationTimer = 0.0f;
    el->castType = castType; 
    el->currentAngle = (float)GetRandomValue(0, 314) / 100.0f; 

    // Assign group ID to treat them as a single entity
    el->castGroupId = groupId;

    float dx = targetMouse.x - pos.x;
    float dy = targetMouse.y - pos.y;
    float length = sqrtf(dx*dx + dy*dy);
    float speed = 150.0f; 

    if (length > 0 && bp.shape == SHAPE_PROJECTILE) {
        el->velocity.x = (dx / length) * speed;
        el->velocity.y = (dy / length) * speed;
    } else {
        el->velocity.x = 0; el->velocity.y = 0;
    }
    if(index >= slot->count) slot->count = index + 1;
}

void activateMana(MagicSlot* slot, Player* player, Vector2 mousePos) {
    SpellBlueprint bp = player->hotbar[player->activeSlot];
    if (bp.raw_dna == 0) return; 

    // Generate a new unique ID for this "wave" of spells
    player->castCounter++; 

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int idx = y * gridWidth + x;
            if (canvas[idx] == CELL_MANA) {
                Vector2 pos = { x * PIXEL_SIZE, y * PIXEL_SIZE };
                
                // Pass the unique counter to group them together
                spawnElement(slot, bp, player->activeCastType, pos, mousePos, player->castCounter);
                canvas[idx] = CELL_EMPTY; 
            }
        }
    }
}

void updateSpellPhysics(MagicSlot* slot) {
    float dt = GetFrameTime();

    for(int i = 0; i < slot->count; i++) {
        Element* el = &slot->Elements[i];
        if(!el->isActive) continue;

        el->animationTimer += dt;
        if(el->animationTimer > el->blueprint.lifespan_seconds) {
            el->isActive = false;
            continue;
        }

        el->startPos.x += el->velocity.x * dt;
        el->startPos.y += el->velocity.y * dt;
        el->currentAngle += dt * 15.0f; 
        
        float waveAmplitude = 15.0f;

        if (el->castType == 1) { // SIN 
            el->pos.x = el->startPos.x;
            el->pos.y = el->startPos.y + sinf(el->currentAngle) * waveAmplitude;
        } else if (el->castType == 2) { // COS 
            el->pos.x = el->startPos.x + sinf(el->currentAngle) * waveAmplitude;
            el->pos.y = el->startPos.y;
        } else if (el->castType == 3) { // ROTATE 
            el->pos.x = el->startPos.x + cosf(el->currentAngle) * waveAmplitude;
            el->pos.y = el->startPos.y + sinf(el->currentAngle) * waveAmplitude;
        } else { // STRAIGHT
            el->pos = el->startPos;
        }

        int gX = el->pos.x / PIXEL_SIZE;
        int gY = el->pos.y / PIXEL_SIZE;
        int gridIndex = gY * gridWidth + gX;
        bool inBounds = (gX >= 0 && gX < gridWidth && gY >= 0 && gY < gridHeight);

        // 1. COLLISION WITH WORLD 
        if (inBounds) {
            if (canvas[gridIndex] == CELL_SOLID && !el->blueprint.traits.physics.phasing) {
                if (el->blueprint.traits.physics.bouncing) {
                    el->velocity.x *= -1; 
                    el->velocity.y *= -1;
                    el->startPos.x += el->velocity.x * dt * 2; 
                    el->startPos.y += el->velocity.y * dt * 2;
                } else {
                    el->isActive = false; 
                }
            }

            if (el->blueprint.traits.matter.solid && el->velocity.x == 0 && el->velocity.y == 0) {
                if (canvas[gridIndex] == CELL_EMPTY) canvas[gridIndex] = CELL_SOLID;
            }

            if (el->blueprint.traits.effects.combustible && canvas[gridIndex] == CELL_SOLID) {
                for(int by = -2; by <= 2; by++) {
                    for(int bx = -2; bx <= 2; bx++) {
                        int bIdx = (gY+by) * gridWidth + (gX+bx);
                        if(bIdx > 0 && bIdx < gridWidth*gridHeight) canvas[bIdx] = CELL_EMPTY;
                    }
                }
                el->isActive = false; 
            }
        } else {
            el->isActive = false; 
        }

        // 2. OBJECT-TO-OBJECT COLLISION
        for(int j = i + 1; j < slot->count; j++) {
            Element* other = &slot->Elements[j];
            if(!other->isActive) continue;
            
            if (abs(el->pos.x - other->pos.x) < PIXEL_SIZE*2 && 
                abs(el->pos.y - other->pos.y) < PIXEL_SIZE*2) {
                
                // --- THE UPDATED FIX ---
                // "Keep them as a group, but contrary to other things"
                // ALWAYS ignore pixels from the exact same cast group
                if (el->castGroupId == other->castGroupId) {
                    continue; 
                }
                
                // If they are from DIFFERENT groups, they collide as normal!
                if ((el->blueprint.traits.elements.fire && other->blueprint.traits.elements.water) || 
                    (el->blueprint.traits.elements.water && other->blueprint.traits.elements.fire)) {
                    el->isActive = false;
                    other->isActive = false;
                } else {
                    el->velocity.x *= -1; el->velocity.y *= -1;
                }
            }
        }

        // 3. DRAWING 
        if(el->isActive) {
            Color c = getSpellColor(el->blueprint);
            if (el->blueprint.traits.physics.gravity_dir == 5) {
                DrawCircle(el->pos.x, el->pos.y, PIXEL_SIZE*3, Fade(c, 0.2f));
            }
            DrawRectangle(el->pos.x, el->pos.y, PIXEL_SIZE, PIXEL_SIZE, c);
        }
    }
}

void updatePlayerMovement(Player* player){
    if(player->isCrafting || player->isSelectingMovement) return; 
    
    Vector2 nextPos = player->pos;
    float dt = GetFrameTime();
    
    if(IsKeyDown(KEY_W)) nextPos.y -= player->speed * dt;
    if(IsKeyDown(KEY_S)) nextPos.y += player->speed * dt;
    if(IsKeyDown(KEY_A)) nextPos.x -= player->speed * dt;
    if(IsKeyDown(KEY_D)) nextPos.x += player->speed * dt;

    int gX = nextPos.x / PIXEL_SIZE;
    int gY = nextPos.y / PIXEL_SIZE;
    
    if (gX >= 0 && gX < gridWidth && gY >= 0 && gY < gridHeight) {
        if (canvas[gY * gridWidth + gX] != CELL_SOLID) { 
            player->pos = nextPos;
        }
    }
}

void DrawSpellTooltip(SpellBlueprint bp, Vector2 pos) {
    if(bp.raw_dna == 0) return;
    char tt[256] = "DNA TRAITS:\n";
    if(bp.traits.elements.fire) strcat(tt, "- Fire\n");
    if(bp.traits.elements.earth) strcat(tt, "- Earth\n");
    if(bp.traits.elements.water) strcat(tt, "- Water\n");
    if(bp.traits.elements.air) strcat(tt, "- Air\n");
    if(bp.traits.matter.solid) strcat(tt, "- Solid Matter\n");
    if(bp.traits.effects.combustible) strcat(tt, "- Combustible\n");
    if(bp.traits.physics.bouncing) strcat(tt, "- Bouncing\n");
    if(bp.traits.physics.gravity_dir == 5) strcat(tt, "- Floating (Glow)\n");
    if(bp.shape == SHAPE_PROJECTILE) strcat(tt, "Form: Projectile\n");
    if(bp.shape == SHAPE_WALL) strcat(tt, "Form: Static Wall\n");

    DrawRectangle(pos.x, pos.y - 140, 140, 130, Fade(BLACK, 0.9f));
    DrawRectangleLines(pos.x, pos.y - 140, 140, 130, GOLD);
    DrawText(tt, pos.x + 5, pos.y - 135, 10, WHITE);
}