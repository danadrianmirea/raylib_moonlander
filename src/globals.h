#pragma once
#include <raylib.h>

//#define DEBUG
//#define DEBUG_COLLISION
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_FUEL_CONSUMPTION 0.1f
#define INITIAL_GRAVITY 1.0f
#define MAX_GRAVITY 2.0f
#define INITIAL_VELOCITY_LIMIT 0.8f
#define MUSIC_VOLUME 0.2f

extern Color black;
extern Color darkGreen;
extern Color grey;
extern Color yellow;
extern const int gameScreenWidth;
extern const int gameScreenHeight;
extern bool exitWindow;
extern bool exitWindowRequested;
extern bool fullscreen;
extern const int minimizeOffset;
extern float borderOffsetWidth;
extern float borderOffsetHeight;
extern const int offset;

