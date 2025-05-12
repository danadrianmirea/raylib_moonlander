#pragma once
#include <raylib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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

