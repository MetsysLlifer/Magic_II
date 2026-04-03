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

// Explicit Global Forms (How it spawns)
typedef enum {
    FORM_PROJECTILE = 0, 
    FORM_MANIFEST = 1,   
    FORM_AURA = 2,       
    FORM_BEAM = 3        // Emergent extreme state
} SpellForm;

// Explicit Node Movements (How it travels)
typedef enum {
    MOVE_STRAIGHT = 0,
    MOVE_SIN = 1,
    MOVE_COS = 2,
    MOVE_ORBIT = 3
} MovementType;

typedef struct {
    float temp;      
    float density;   
    float moisture;  
    float cohesion;  
    float charge;    
    Vector2 velocity; 
    bool permanent;  
} Cell;

typedef struct {
    float temp;
    float density;
    float moisture;
    float cohesion;
    float charge;
    Vector2 velocity;  
    int form;          
    int movement;      // NEW: Inherited Kinetic Pattern
    bool isPermanent;
} SpellDNA;

typedef struct {
    Vector2 pos;       
    int parentId;      
    float temp;
    float density;
    float moisture;
    float cohesion;
    float charge;
    int movement;      // NEW: Per-Node Movement Assignment
    bool active;
} SpellNode;

typedef struct {
    SpellNode nodes[MAX_NODES];
    int count;
} SigilGraph;

typedef struct {
    Vector2 pos;
    Vector2 basePos; 
    Vector2 velocity;
    SpellDNA payload;
    int layer;
    float life;
    bool active;
    float animOffset; 
} Projectile;

typedef struct {
    Vector2 pos;
    float z;          
    float zVelocity;  
    bool isJumping;   
    float animTime;   

    float speed;
    float health;       
    float maxHealth;
    int activeSlot;
    SpellDNA hotbar[10];
    
    float chargeLevel; 
    bool isCharging;   

    bool isCrafting;
    bool showGuide;
    float visionBlend; 
    int castLayer; 

    SigilGraph sigil;
    int selectedNodeId;
    int selectedForm; // NEW: Explicitly tracks chosen global form
    Camera2D craftCamera;
} Player;

extern Cell grid[2][WIDTH * HEIGHT];
extern Cell prev_grid[2][WIDTH * HEIGHT];
extern Projectile projectiles[100];

void InitSimulation();
void UpdateSimulation(float dt, Player *p); 
void MovePlayer(Player *p, Vector2 delta, float dt); 
void DrawMaterialRealm(float alpha); 
void DrawEnergyRealm(float alpha);   
void DrawProjectiles(Player *p);
void DrawPlayerEntity(Player *p); 
void DrawInterface(Player *p, SpellDNA *draft);
void DrawGuideMenu(Player *p);

void CompileSigilGraph(Player *p, SpellDNA *draft);
void ExecuteSpell(Player *p, Vector2 target, SpellDNA dna, float chargeMultiplier);
void CastProjectile(Vector2 start, Vector2 target, int layer, SpellDNA dna);
void InjectEnergy(int x, int y, int z, SpellDNA dna);
void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna);
void InjectBeam(Vector2 start, Vector2 target, int z, SpellDNA dna);

#endif