#ifndef UTIL_H
#define UTIL_H

#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PIXEL_SIZE 5
#define WIDTH (800 / PIXEL_SIZE)
#define HEIGHT (450 / PIXEL_SIZE)

#define LAYER_GROUND 0
#define LAYER_AIR 1
#define FLOAT_OFFSET 15 
#define MAX_NODES 64

typedef enum { FORM_PROJECTILE = 0, FORM_MANIFEST = 1, FORM_AURA = 2, FORM_BEAM = 3 } SpellForm;
typedef enum { MOVE_STRAIGHT = 0, MOVE_SIN = 1, MOVE_COS = 2, MOVE_ORBIT = 3 } MovementType;
typedef enum { ITEM_SPELL = 0, ITEM_NPC = 1 } ItemType;

typedef struct {
    float temp; float density; float moisture; float cohesion; float charge; Vector2 velocity; bool permanent;  
} Cell;

typedef struct {
    Vector2 pos;       
    int parentId;      
    float temp; float density; float moisture; float cohesion; float charge;
    int movement;      
    bool active;
} SpellNode;

typedef struct {
    SpellNode nodes[MAX_NODES];
    int count;
} SigilGraph;

typedef struct {
    float temp; float density; float moisture; float cohesion; float charge; Vector2 velocity;  
    int form; int movement; bool isPermanent;
    SigilGraph graph; 
} SpellDNA;

typedef struct {
    float mass; float aero; float hydro; float terrestrial; float intelligence; float hostility;    
} NPCDNA;

typedef struct {
    ItemType type;
    SpellDNA spell;
    NPCDNA npc;
} HotbarSlot;

typedef struct {
    Vector2 pos; Vector2 basePos; Vector2 velocity; SpellDNA payload; int layer; float life; bool active; float animOffset; 
} Projectile;

typedef struct {
    Vector2 pos; float z; Vector2 velocity; float zVelocity; NPCDNA dna; float health; float animTime; bool active;
} NPC;

typedef struct {
    Vector2 pos; float z; float zVelocity; bool isJumping; float animTime; float speed; float health; float maxHealth;
    int activeSlot;
    HotbarSlot hotbar[10]; 
    
    float chargeLevel; bool isCharging;   
    bool isCrafting; bool showGuide; 
    
    float visionBlend; // 0.0 (Mat) -> 1.0 (Nrg) -> 2.0 (Haz)
    int castLayer; 

    int selectedNodeId;
    int draggingNodeId; 
    int craftCategory; 
    Camera2D craftCamera;
} Player;

extern Cell grid[2][WIDTH * HEIGHT];
extern Cell prev_grid[2][WIDTH * HEIGHT];
extern Projectile projectiles[100];
extern NPC active_npcs[50];

void InitSimulation();
void UpdateSimulation(float dt, Player *p); 
void MovePlayer(Player *p, Vector2 delta, float dt); 

void DrawMaterialRealm(float alpha); 
void DrawEnergyRealm(float alpha);   
void DrawHazardRealm(float alpha); // NEW: Danger Vision
void DrawProjectiles(Player *p);
void DrawPlayerEntity(Player *p); 
void DrawInterface(Player *p, NPCDNA *draftNPC);
void DrawGuideMenu(Player *p);

void CompileSigilGraph(SpellDNA *dna);
void ExecuteSpell(Player *p, Vector2 target, SpellDNA dna, float chargeMultiplier);
void CastProjectile(Vector2 start, Vector2 target, int layer, SpellDNA dna);
void InjectEnergy(int x, int y, int z, SpellDNA dna);
void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna);
void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna);

void InitNPCs();
void UpdateNPCs(float dt, Player *p);
void DrawNPCs();
void DrawProceduralNPC(Vector2 pos, float z, NPCDNA dna, float alpha);
void SpawnNPC(Vector2 pos, NPCDNA dna);

#endif