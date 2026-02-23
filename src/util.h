#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"

#define MAXSLOT 3
#define MAX 4000      // MASSIVELY increased to handle thousands of individual single-pixel spells
#define MAX_ORBS 20   // Max number of raw mana orbs orbiting
#define EMPTY -1
#define PIXEL_SIZE 4
#define screenWidth 800
#define screenHeight 450

// Canvas cell states
#define CELL_EMPTY 0
#define CELL_SOLID 1
#define CELL_MANA 2

typedef unsigned char Area;

// -------------------------------------------------------------------------------------------------
// BASE ELEMENTS (4 bits)
typedef struct {
    uint8_t fire  : 1;
    uint8_t earth : 1;
    uint8_t water : 1;
    uint8_t air   : 1;
    uint8_t _pad  : 4; 
} ElementBits;

// MATTER STATE (4 bits)
typedef struct {
    uint8_t solid  : 1;
    uint8_t liquid : 1;
    uint8_t gas    : 1;
    uint8_t plasma : 1;
    uint8_t _pad   : 4;
} MatterBits;

// SPATIAL & KINETIC PROPERTIES (16 bits)
typedef struct {
    uint16_t space_manipulation : 1; 
    uint16_t time_dilation      : 1; 
    uint16_t homing             : 1; 
    uint16_t piercing           : 1; 
    uint16_t bouncing           : 1; 
    uint16_t phasing            : 1; 
    uint16_t orbiting           : 1; 
    uint16_t sticking           : 1; 
    uint16_t gravity_dir        : 5; // 5 = Floating (Glows visually!)
    uint16_t _pad               : 5; 
} PhysicsBits;

// HIDDEN & CHEMICAL EFFECTS (16 bits)
typedef struct {
    uint16_t psychic      : 1; 
    uint16_t life_drain   : 1; 
    uint16_t healing      : 1; 
    uint16_t midas_touch  : 1; 
    uint16_t combustible  : 1; 
    uint16_t freezing     : 1; 
    uint16_t conductive   : 1; 
    uint16_t corrosive    : 1; 
    uint16_t luminescent  : 1; 
    uint16_t silencing    : 1; 
    uint16_t repelling    : 1; 
    uint16_t _pad         : 5;
} EffectBits;

// SHAPE & FORM (3 bits)
typedef enum {
    SHAPE_PROJECTILE = 0,
    SHAPE_WALL       = 1
} SpellShape;

// THE MASTER BLUEPRINT
typedef struct {
    union {
        struct {
            ElementBits elements; 
            MatterBits  matter;   
            PhysicsBits physics;  
            EffectBits  effects;  
        } traits;
        uint64_t raw_dna;         
    };
    SpellShape shape;
    float lifespan_seconds;      
} SpellBlueprint;
// -------------------------------------------------------------------------------------------------

// The raw mana orb system
typedef struct {
    Vector2 startPos; // Center of orbit
    float angle;
    bool active;
} ManaOrb;

// The active spell entities (Single pixels with advanced movement!)
typedef struct ElementType {
    Vector2 pos;            // Current drawn position
    Vector2 startPos;       // The "center" path it travels along
    Vector2 velocity;       // Forward momentum
    
    int castType;           // 0=Straight, 1=Sin, 2=Cos, 3=Rotate
    float currentAngle;     // Used for the wave math
    
    // --- THE FIX: JUST THE GROUP ID ---
    int castGroupId;        // Identifies which cast this pixel belongs to (acts as one group)
    
    float animationTimer;   
    bool isActive;          
    SpellBlueprint blueprint; 
} Element;

typedef struct {
    Element Elements[MAX];
    int count;
} MagicSlot;

typedef struct {
    Vector2 pos;
    float speed;
    int activeSlot; 
    SpellBlueprint hotbar[10];
    
    // UI & State Data
    bool isCrafting; 
    bool isSelectingMovement; 
    Vector2 dragCenter;
    int activeCastType; 
    
    int castCounter; 
} Player;

// -------------------- GLOBAL GAME STATE ----------------- //
extern int gridWidth;
extern int gridHeight;
extern Area *canvas;
extern ManaOrb manaOrbs[MAX_ORBS];

// -------------------- UTIL ---------------------- //
void initMagicSlot(MagicSlot* slot);
void castManaOrb(Vector2 mousePos);
void updateManaOrbs(Player* player);
void leakMana();
void activateMana(MagicSlot* slot, Player* player, Vector2 mousePos);
void updateSpellPhysics(MagicSlot* slot);
void updatePlayerMovement(Player* player);
Color getSpellColor(SpellBlueprint bp);
void DrawSpellTooltip(SpellBlueprint bp, Vector2 pos);
// ----------------------------------------------- //

// -------------------- UI ---------------------- //
void updateWorld();
void displayUI(Player* player);
void displayCraftingTable(Player* player, SpellBlueprint* draftSpell); // subject to change -> magiCraft
void magiCraft(Player player); // incomplete
void displayMovementWheel(Player* player);
// ----------------------------------------------- //

#endif // UTIL_H