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
#define MAX_PROJECTILES 100
#define MAX_NPCS 50
#define MAX_SINGULARITIES 20
#define MAX_PLAYERS 2

typedef enum { FORM_PROJECTILE = 0, FORM_MANIFEST = 1, FORM_AURA = 2, FORM_BEAM = 3 } SpellForm;
typedef enum { MOVE_STRAIGHT = 0, MOVE_SIN = 1, MOVE_COS = 2, MOVE_ORBIT = 3 } MovementType;
typedef enum { ITEM_SPELL = 0, ITEM_NPC = 1 } ItemType;
typedef enum { SPREAD_OFF = 0, SPREAD_INSTANT = 1, SPREAD_COLLISION = 2 } SpreadType;
typedef enum { SCALAR_TEMP = 0, SCALAR_DENSITY = 1, SCALAR_MOISTURE = 2, SCALAR_COHESION = 3, SCALAR_CHARGE = 4, SCALAR_COUNT = 5 } ScalarChannel;
typedef enum { COND_ALWAYS = 0, COND_FLIGHT_TIME_GT = 1, COND_SCALAR_GT = 2, COND_SCALAR_LT = 3 } NodeConditionType;
typedef enum {
    TOOL_NONE = 0,
    TOOL_BUILD = 1,
    TOOL_DIG = 2,
    TOOL_MOISTEN = 3,
    TOOL_DRY = 4,
    TOOL_HEAT = 5,
    TOOL_COOL = 6,
    TOOL_CONDUCT = 7,
    TOOL_SINGULARITY_BLACK = 8,
    TOOL_SINGULARITY_WHITE = 9
} ToolType;

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

    // Dynamic scalar architecture: node contributions are now channel-based.
    float scalarAdd[SCALAR_COUNT];
    float scalarScale[SCALAR_COUNT];

    // Logical operator gear: conditional activation and optional runtime detach.
    int conditionType;
    int conditionChannel;
    float conditionThreshold;
    bool detachOnCondition;

    // Toolcraft gear: spell nodes can create utility effects, not only combat effects.
    bool hasTool;
    int toolType;
    float toolPower;
    float toolRadius;
    bool toolPermanent;
    
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
    float channels[SCALAR_COUNT];
    int toolType;
    float toolPower;
    float toolRadius;
    bool toolPermanent;
    SigilGraph graph; 
} SpellDNA;

typedef struct { float mass; float aero; float hydro; float terrestrial; float intelligence; float hostility; } NPCDNA;
typedef struct { ItemType type; SpellDNA spell; NPCDNA npc; } HotbarSlot;

typedef struct {
    Vector2 pos; Vector2 basePos; Vector2 baseVelocity; Vector2 velocity; SpellDNA payload; 
    int rootId; float chargeMult; float lifeMult; // NEW: Lifespan Multiplier stored in flight
    int ownerId;
    int layer; float life; float maxLife; bool active; float animOffset; 
    float flightTime; 
} Projectile;

typedef struct { Vector2 pos; float z; Vector2 velocity; float zVelocity; NPCDNA dna; float health; float animTime; bool active; } NPC;

// NEW: Exposing Singularities globally for UI rendering
typedef struct {
    int index;
    Vector2 pos;
    float mass;
    float charge;
    int type;
    int linkedTo;
    float anim;
    float radius;
    float strength;
    float lifetime;
    Vector2 drift;
    bool active;
} Singularity;

typedef struct {
    Vector2 pos; float z; float zVelocity; bool isJumping; float animTime; float speed; float health; float maxHealth;
    int id;
    int activeSlot;
    HotbarSlot hotbar[10]; 
    
    float chargeLevel;   // Intensity (LMB)
    float lifespanLevel; // Duration/Permanence (C key)
    bool isCharging;   
    bool isLifespanCharging;

    bool isCrafting; bool showGuide; float visionBlend; int castLayer; 
    bool showCompendium;
    bool friendlyFire; 
    Vector2 aimDir;

    int selectedNodeId; int draggingNodeId; int craftCategory; 
    bool editStates[10]; 

    Camera2D craftCamera;
    Camera2D worldCamera; 
} Player;

extern Cell grid[2][WIDTH * HEIGHT];
extern Cell prev_grid[2][WIDTH * HEIGHT];
extern Projectile projectiles[MAX_PROJECTILES];
extern NPC active_npcs[MAX_NPCS];
extern Singularity sys_singularities[MAX_SINGULARITIES];
extern int sys_sigCount;

void InitDefaultSpellNode(SpellNode *node);

void InitSimulation();
void UpdateSimulation(float dt, Player players[], int playerCount); 
void MovePlayer(Player *p, Vector2 delta, float dt); 
void ResetGame(Player *p); 
bool SaveWorldState(const char *path, Player *p);
bool LoadWorldState(const char *path, Player *p);

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

void SyncNodeLegacyScalars(SpellNode *node);
void CompileSigilGraph(SpellDNA *dna);
void ExecuteSpell(Player *p, Vector2 target, SpellDNA *dna, float chargeMultiplier, float lifeMultiplier);
void CastDynamicProjectile(Vector2 start, Vector2 target, int layer, SpellDNA *dna, int rootId, float chargeMult, float lifeMult, int ownerId);
void InjectEnergy(int x, int y, int z, SpellDNA dna);
void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna);
void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna);
bool IsDescendant(SigilGraph *g, int child, int root);
float GetNodeMagnitude(SigilGraph *g, int child, int root, Vector2 *outDir, float *resonance);

void InitNPCs();
void UpdateNPCs(float dt, Player players[], int playerCount);
void DrawNPCs();
void DrawProceduralNPC(Vector2 pos, float z, NPCDNA dna, float alpha);
void SpawnNPC(Vector2 pos, NPCDNA dna);

#endif