#include <raylib.h>
#include "globals.h"

Color black = Color{0, 0, 0, 255};
Color darkGreen = Color{20, 160, 133, 255};
Color grey = Color{ 29, 29, 27, 255 };
Color yellow = Color{ 243, 216, 63, 255 };
int windowWidth = 1920;
int windowHeight = 1080;
const int gameScreenWidth = 960;
const int gameScreenHeight = 540;
bool exitWindowRequested = false;
bool exitWindow = false;
bool fullscreen = false;
const int minimizeOffset = 50;
float borderOffsetWidth = 20.0;
float borderOffsetHeight = 50.0f;
const int offset = 110;