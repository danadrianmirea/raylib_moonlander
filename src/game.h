#pragma once

#include <string>
#include <raylib.h>

class Lander {
public:
    Lander(int screenWidth, int screenHeight);
    void Update(float dt, bool thrusting, bool rotatingLeft, bool rotatingRight);
    void Draw();
    void Reset(int screenWidth, int screenHeight);
    void Cleanup();
    void SetTerrainReference(Vector2* terrain, int terrainPoints);
    
    bool IsLanded() const { return landed; }
    bool IsCrashed() const { return crashed; }
    float GetFuel() const { return fuel; }
    float GetVelocityX() const { return velocityX; }
    float GetVelocityY() const { return velocityY; }
    float GetAngle() const { return angle; }
    float GetLandingPadX() const { return landingPadX; }
    double GetLandingTime() const { return landingTime; }
    float GetX() const { return landerX; }
    float GetY() const { return landerY; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }

    // Game parameters
    static float thrust;
    static float rotationSpeed;
    static float fuelConsumption;
    static constexpr float collisionScale = 0.8f; // Scale factor for collision box

private:
    float landerX, landerY;
    float velocityX, velocityY;
    float angle;
    float fuel;
    bool landed;
    bool crashed;
    float width;
    float height;
    float landingPadX;
    double landingTime;
    Sound thrustSound;
    Sound landSound;
    Sound crashSound;
    bool wasThrusting;
    bool wasRotating;
    Vector2* terrain;
    int terrainPoints;
    Texture2D texture; // Lander texture
    Texture2D flameTexture; // Flame texture
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
    void DrawTerrain();
    void DrawUI();
    std::string FormatWithLeadingZeroes(int number, int width);
    void Randomize();

    static bool isMobile;
    static float gravity;
    static constexpr float gravityIncrease = 0.15f;
    static float velocityLimit;

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
    
    // Audio
    Music backgroundMusic;
    bool musicStarted;

    // Graphics
    Texture2D backgroundTexture;
    Texture2D terrainTexture;
    Texture2D landingPadTexture;

    const float minTerrainHeight = 250;
    const float maxTerrainHeight = 50;

    // Terrain
    static const int TERRAIN_POINTS = 40;
    Vector2 terrainPoints[TERRAIN_POINTS];
};