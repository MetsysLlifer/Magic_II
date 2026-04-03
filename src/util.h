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
    float temp;      
    float density;   
    float moisture;  
    Vector2 velocity; 
} Cell;

typedef struct {
    Vector2 pos;
    float speed;
    int activeElement; 
} Player;

extern Cell grid[WIDTH * HEIGHT];
extern Cell prev_grid[WIDTH * HEIGHT];

void InitSimulation();
void UpdateSimulation(float dt);
void DrawSimulation();
void DrawInterface(Player *p); // Must be a pointer to update the HUD correctly
void InjectEnergy(int x, int y, Cell energy);

#endif