#ifndef UTIL_H
#define UTIL_H

#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PIXEL_SIZE 5
#define WIDTH (800 / PIXEL_SIZE)
#define HEIGHT (450 / PIXEL_SIZE)

// Universal Field Variables
typedef struct {
    float temp;      // Thermal Energy
    float density;   // Mass per volume
    float moisture;  // Wetness/Fluidity
    float cohesion;  // How strongly atoms bind (100 = Solid, 0 = Gas)
    float charge;    // Electrical potential
    Vector2 velocity; // Kinetic movement across XY plane
    bool permanent;  // Does not naturally decay
} Cell;

typedef struct {
    float temp;
    float density;
    float moisture;
    float cohesion;
    float charge;
    Vector2 velocity;
    bool isPermanent;
} SpellDNA;

typedef struct {
    Vector2 pos;
    float speed;
    int activeSlot;
    SpellDNA hotbar[10];
    
    // UI States
    bool isCrafting;
    bool showGuide;
    bool energyVision; // Toggle between Material (Naked Eye) and Energy Realm
} Player;

extern Cell grid[WIDTH * HEIGHT];
extern Cell prev_grid[WIDTH * HEIGHT];

void InitSimulation();
void UpdateSimulation(float dt);
void DrawMaterialRealm();
void DrawEnergyRealm();
void DrawInterface(Player *p, SpellDNA *draft);
void DrawGuideMenu(Player *p);
void InjectEnergy(int x, int y, SpellDNA dna);

#endif