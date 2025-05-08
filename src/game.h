#pragma once

#include <string>
#include <raylib.h>

class Lander {
public:
    Lander(int screenWidth, int screenHeight);
    void Update(float dt, bool thrusting, bool rotatingLeft, bool rotatingRight);
    void Draw();
    void Reset(int screenWidth, int screenHeight);
    
    bool IsLanded() const { return landed; }
    bool IsCrashed() const { return crashed; }
    float GetFuel() const { return fuel; }
    float GetVelocityX() const { return velocityX; }
    float GetVelocityY() const { return velocityY; }
    float GetAngle() const { return angle; }
    float GetLandingPadX() const { return landingPadX; }
    double GetLandingTime() const { return landingTime; }
    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }

    // Game parameters
    static float thrust;
    static float rotationSpeed;
    static float fuelConsumption;

private:
    float x, y;
    float velocityX, velocityY;
    float angle;
    float fuel;
    bool landed;
    bool crashed;
    float width;
    float height;
    float landingPadX;
    double landingTime;
};

class Game
{
public:
    Game(int width, int height);
    ~Game();
    void InitGame();
    void Reset();
    void Update(float dt);
    void HandleInput();
    void UpdateUI();
    void Draw();
    void DrawUI();
    std::string FormatWithLeadingZeroes(int number, int width);
    void Randomize();

    static bool isMobile;
    static float gravity;

private:
    bool firstTimeGameStart;
    bool isInExitMenu;
    bool paused;
    bool lostWindowFocus;
    bool gameOver;

    float screenScale;
    RenderTexture2D targetRenderTex;
    Font font;

    int width;
    int height;

    // Game state
    int lives;
    int level;
    float thrust;
    float rotationSpeed;
    float fuelConsumption;
    double inputDelay;

    // Game objects
    Lander* lander;
};