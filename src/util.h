#ifndef UTIL_H
#define UTIL_H

#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PIXEL_SIZE 5
#define WIDTH (800 / PIXEL_SIZE)
#define HEIGHT (450 / PIXEL_SIZE)

typedef struct {
    float temp;      // Thermal
    float density;   // Mass
    float moisture;  // Fluidity
    Vector2 velocity; 
    bool permanent;  // If true, energy does not decay
} Cell;

typedef struct {
    float temp;
    float density;
    float moisture;
    Vector2 velocity;
    bool isPermanent;
} SpellDNA;

typedef struct {
    Vector2 pos;
    float speed;
    int activeSlot;
    SpellDNA hotbar[10];
    bool isCrafting;
} Player;

extern Cell grid[WIDTH * HEIGHT];
extern Cell prev_grid[WIDTH * HEIGHT];

void InitSimulation();
void UpdateSimulation(float dt);
void DrawSimulation();
void DrawInterface(Player *p, SpellDNA *draft);
void InjectEnergy(int x, int y, SpellDNA dna);

#endif