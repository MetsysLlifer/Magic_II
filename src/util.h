#ifndef UTIL_H
#define UTIL_H

#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SCREEN_W 800
#define SCREEN_H 450
#define WORLD_W 2400
#define WORLD_H 1350

#define PIXEL_SIZE 5
#define WIDTH (WORLD_W / PIXEL_SIZE)
#define HEIGHT (WORLD_H / PIXEL_SIZE)

#define LAYER_GROUND 0
#define LAYER_AIR 1
#define FLOAT_OFFSET 15 
#define MAX_NODES 64

typedef enum { FORM_PROJECTILE = 0, FORM_MANIFEST = 1, FORM_AURA = 2, FORM_BEAM = 3 } SpellForm;
typedef enum { MOVE_STRAIGHT = 0, MOVE_SIN = 1, MOVE_COS = 2, MOVE_ORBIT = 3 } MovementType;
typedef enum { ITEM_SPELL = 0, ITEM_NPC = 1 } ItemType;
typedef enum { SPREAD_OFF = 0, SPREAD_INSTANT = 1, SPREAD_COLLISION = 2 } SpreadType;

typedef struct {
    float temp; float density; float moisture; float cohesion; float charge; Vector2 velocity; bool permanent;  
} Cell;

typedef struct {
    Vector2 pos;       
    int parentId;      
    float temp; float density; float moisture; float cohesion; float charge;
    int movement;      
    
    bool hasSpeed;   float speedMod; float easeTime; 
    bool hasDelay;   float delay; 
    bool hasDistort; float distortion; 
    bool hasRange;   float rangeMod; 
    bool hasSize;    float sizeMod; 
    bool hasSpread;  int spreadType;
    
    bool active;
    bool triggered;
} SpellNode;

typedef struct SigilGraph {
    SpellNode nodes[MAX_NODES];
    int count;
} SigilGraph;

typedef struct {
    float temp; float density; float moisture; float cohesion; float charge; Vector2 velocity;  
    int form; int movement; 
    float speedMod; float delay; float distortion; float rangeMod; float sizeMod; int spreadType;
    SigilGraph graph; 
} SpellDNA;

typedef struct { float mass; float aero; float hydro; float terrestrial; float intelligence; float hostility; } NPCDNA;
typedef struct { ItemType type; SpellDNA spell; NPCDNA npc; } HotbarSlot;

typedef struct {
    Vector2 pos; Vector2 basePos; Vector2 baseVelocity; Vector2 velocity; SpellDNA payload; 
    int rootId; float chargeMult; float lifeMult; // NEW: Lifespan Multiplier stored in flight
    int layer; float life; float maxLife; bool active; float animOffset; 
    float flightTime; 
} Projectile;

typedef struct { Vector2 pos; float z; Vector2 velocity; float zVelocity; NPCDNA dna; float health; float animTime; bool active; } NPC;

// NEW: Exposing Singularities globally for UI rendering
typedef struct {
    int index; Vector2 pos; float mass; float charge; int type; int linkedTo; float anim;
} Singularity;

typedef struct {
    Vector2 pos; float z; float zVelocity; bool isJumping; float animTime; float speed; float health; float maxHealth;
    int activeSlot;
    HotbarSlot hotbar[10]; 
    
    float chargeLevel;   // Intensity (LMB)
    float lifespanLevel; // Duration/Permanence (C key)
    bool isCharging;   
    bool isLifespanCharging;

    bool isCrafting; bool showGuide; float visionBlend; int castLayer; 
    bool friendlyFire; 

    int selectedNodeId; int draggingNodeId; int craftCategory; 
    bool editStates[10]; 

    Camera2D craftCamera;
    Camera2D worldCamera; 
} Player;

extern Cell grid[2][WIDTH * HEIGHT];
extern Cell prev_grid[2][WIDTH * HEIGHT];
extern Projectile projectiles[100];
extern NPC active_npcs[50];
extern Singularity sys_singularities[20];
extern int sys_sigCount;

void InitSimulation();
void UpdateSimulation(float dt, Player *p); 
void MovePlayer(Player *p, Vector2 delta, float dt); 
void ResetGame(Player *p); 

void DrawMaterialRealm(float alpha); 
void DrawEnergyRealm(float alpha);   
void DrawHazardRealm(float alpha);
void DrawSingularities(float alpha); // NEW: Visualizer for Black/White Holes
void DrawProjectiles(Player *p);
void DrawPlayerEntity(Player *p); 
void DrawInterface(Player *p, NPCDNA *draftNPC, Vector2 virtualMouse); 
void DrawGuideMenu(Player *p, bool *wantsRestart); 

void DrawNodeSigil(Vector2 center, SpellNode node, float scale, float animOffset);
void DrawCompositeSigil(Vector2 center, SigilGraph *graph, float scale, float rot, float animOffset);

void CompileSigilGraph(SpellDNA *dna);
void ExecuteSpell(Player *p, Vector2 target, SpellDNA *dna, float chargeMultiplier, float lifeMultiplier);
void CastDynamicProjectile(Vector2 start, Vector2 target, int layer, SpellDNA *dna, int rootId, float chargeMult, float lifeMult);
void InjectEnergy(int x, int y, int z, SpellDNA dna);
void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna);
void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna);
bool IsDescendant(SigilGraph *g, int child, int root);
float GetNodeMagnitude(SigilGraph *g, int child, int root, Vector2 *outDir, float *resonance);

void InitNPCs();
void UpdateNPCs(float dt, Player *p);
void DrawNPCs();
void DrawProceduralNPC(Vector2 pos, float z, NPCDNA dna, float alpha);
void SpawnNPC(Vector2 pos, NPCDNA dna);

#endif