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
    float GetCrashPosX() const { return crashPosX; }
    float GetCrashPosY() const { return crashPosY; }

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
    float crashPosX, crashPosY; // Position where crash occurred
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
    void DrawExplosion();
    void StartExplosion(float x, float y);
    std::string FormatWithLeadingZeroes(int number, int width);
    void Randomize();

    static bool isMobile;
    static float gravity;
    static bool maxGravityReached;
    static constexpr float gravityIncrease = 0.15f;
    static constexpr float fuelConsumptionIncrease = 0.01f;
    static float velocityLimit;

private:
    bool firstTimeGameStart;
    bool isInExitMenu;
    bool paused;
    bool lostWindowFocus;
    bool gameOver;
    bool playingMusic;

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
    Texture2D explosionTexture;

    // Explosion animation
    bool explosionActive;
    bool explosionCompleted;  // Tracks if explosion has been shown for current crash
    int explosionFramesCounter;
    int explosionCurrentFrame;
    int explosionCurrentLine;
    Rectangle explosionFrameRec;
    Vector2 explosionPosition;


    static constexpr float explosionScale = 1.0f; // Scale factor for explosion
    static constexpr float explosionPlaybackSpeed = 4.0f; // Speed of the explosion
    static const int EXPLOSION_FRAMES_PER_LINE = 5; // Adjust based on your sprite sheet
    static const int EXPLOSION_LINES = 5;           // Adjust based on your sprite sheet

    const float minTerrainHeight = 250;
    const float maxTerrainHeight = 50;

    // Terrain
    static const int TERRAIN_POINTS = 40;
    Vector2 terrainPoints[TERRAIN_POINTS];
};