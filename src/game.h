#pragma once

#include <string>
#include <raylib.h>

class Lander;

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

    int lives;
    int level;
    float thrust;
    float rotationSpeed;
    double inputDelay;

    Lander* lander;
    
    Music backgroundMusic;
    bool musicStarted;

    Texture2D backgroundTexture;
    Texture2D terrainTexture;
    Texture2D landingPadTexture;
    Texture2D explosionTexture;

    bool explosionActive;
    bool explosionCompleted;
    int explosionFramesCounter;
    int explosionCurrentFrame;
    int explosionCurrentLine;
    Rectangle explosionFrameRec;
    Vector2 explosionPosition;

    static constexpr float explosionScale = 1.0f;
    static constexpr float explosionPlaybackSpeed = 4.0f;
    static const int EXPLOSION_FRAMES_PER_LINE = 5; 
    static const int EXPLOSION_LINES = 5;           

    const float minTerrainHeight = 250;
    const float maxTerrainHeight = 50;

    static const int TERRAIN_POINTS = 40;
    Vector2 terrainPoints[TERRAIN_POINTS];
};
