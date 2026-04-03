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

typedef enum {
    FORM_PROJECTILE = 0,
    FORM_MANIFEST = 1,   
    FORM_AURA = 2        
} SpellForm;

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
    int form;          
    bool isPermanent;
} SpellDNA;

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    SpellDNA payload;
    int layer;
    float life;
    bool active;
} Projectile;

typedef struct {
    Vector2 pos;
    float speed;
    float health;       
    float maxHealth;
    int activeSlot;
    SpellDNA hotbar[10];
    
    float chargeLevel; 
    bool isCharging;   

    bool isCrafting;
    bool showGuide;
    
    float visionBlend; // NEW: 0.0 (Material) to 1.0 (Energy)
    int castLayer; 
} Player;

extern Cell grid[2][WIDTH * HEIGHT];
extern Cell prev_grid[2][WIDTH * HEIGHT];
extern Projectile projectiles[100];

void InitSimulation();
void UpdateSimulation(float dt, Player *p); 
void MovePlayer(Player *p, Vector2 delta); 
void DrawMaterialRealm(float alpha); // NEW: Accepts opacity multiplier
void DrawEnergyRealm(float alpha);   // NEW: Accepts opacity multiplier
void DrawProjectiles();
void DrawInterface(Player *p, SpellDNA *draft);
void DrawGuideMenu(Player *p);

void ExecuteSpell(Player *p, Vector2 target, SpellDNA dna, float chargeMultiplier);
void CastProjectile(Vector2 start, Vector2 target, int layer, SpellDNA dna);
void InjectEnergy(int x, int y, int z, SpellDNA dna);
void InjectEnergyArea(int cx, int cy, int z, int radius, SpellDNA dna);

#endif