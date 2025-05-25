#include <vector>
#include <utility>
#include <string>
#include <cmath>  // For sqrtf
#include <random>
#include <algorithm> // For std::min

#include "raylib.h"
#include "rlgl.h"
#include "globals.h"
#include "game.h"
#include "lander.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool Game::isMobile = false;
float Game::gravity = INITIAL_GRAVITY;
bool Game::maxGravityReached = false;
float Game::velocityLimit = INITIAL_VELOCITY_LIMIT;

// Debug variables for mobile thrust
bool debugThrustActive = false;
double debugLastThrustTime = 0.0;
int debugTouchCount = 0;
bool debugWasThrusting = false;
double thrustTimeout = 0.1; // Allow brief interruptions in touch events (100ms)

Game::Game(int width, int height)
{
    firstTimeGameStart = true;
    musicStarted = false;
    explosionActive = false;
    explosionCompleted = false;
    explosionFramesCounter = 0;
    explosionCurrentFrame = 0;
    explosionCurrentLine = 0;
    explosionPosition = { 0.0f, 0.0f };
    gameWon = false;

#ifdef __EMSCRIPTEN__
    
    isMobile = EM_ASM_INT({
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    });
#endif

    targetRenderTex = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(targetRenderTex.texture, TEXTURE_FILTER_BILINEAR); 

    font = LoadFontEx("Font/OpenSansRegular.ttf", 64, 0, 0);

    backgroundMusic = LoadMusicStream("data/music.mp3");
    if (backgroundMusic.stream.buffer == NULL) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load music file: data/music.mp3");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded music file");
        #endif
        SetMusicVolume(backgroundMusic, MUSIC_VOLUME);
    }

    backgroundTexture = LoadTexture("data/background.png");
    if (backgroundTexture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load background texture: data/background.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded background texture");
        #endif
    }

    terrainTexture = LoadTexture("data/moon_surface.png");
    if (terrainTexture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load terrain texture: data/moon_surface.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded terrain texture");
        #endif
        
        SetTextureWrap(terrainTexture, TEXTURE_WRAP_REPEAT);
    }

    landingPadTexture = LoadTexture("data/landing_pad.png");
    if (landingPadTexture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load landing pad texture: data/landing_pad.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded landing pad texture");
        #endif
    }

    explosionTexture = LoadTexture("data/explosion.png");
    if (explosionTexture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load explosion texture: data/explosion.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded explosion texture");
        #endif

        float frameWidth = (float)(explosionTexture.width / EXPLOSION_FRAMES_PER_LINE);
        float frameHeight = (float)(explosionTexture.height / EXPLOSION_LINES);
        explosionFrameRec = { 0, 0, frameWidth, frameHeight };
    }

    this->width = width;
    this->height = height;
    playingMusic = false;
    InitGame();
}

Game::~Game()
{
    if (lander != nullptr) {
        lander->Cleanup();
        delete lander;
    }
    UnloadRenderTexture(targetRenderTex);
    UnloadFont(font);
    UnloadMusicStream(backgroundMusic);
    UnloadTexture(backgroundTexture);
    UnloadTexture(terrainTexture);
    UnloadTexture(landingPadTexture);
    UnloadTexture(explosionTexture);
}

void Game::InitGame()
{
    isInExitMenu = false;
    paused = false;
    lostWindowFocus = false;
    gameOver = false;
    gameWon = false;
    explosionCompleted = false;

    screenScale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);

    lives = 3;
    level = 1;
    thrust = 0.2f;
    rotationSpeed = 3.0f;
    inputDelay = 0.3;
    playingMusic = true;

    lander = new Lander(width, height);
    Randomize(); 
    lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
}

void Game::Reset()
{
    lives = 3;
    level = 1;
    Game::gravity = INITIAL_GRAVITY;  
    Game::maxGravityReached = false;
    Game::velocityLimit = INITIAL_VELOCITY_LIMIT;
    explosionCompleted = false;
    gameWon = false;
    Lander::fuelConsumption = Lander::initialFuelConsumption;
    lander->Reset(width, height);
    Randomize(); 
    lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
    gameOver = false;
    playingMusic = true;
}

void Game::Update(float dt)
{
    if (dt == 0)
    {
        return;
    }

    screenScale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);
    UpdateUI();

    // If game is paused, any tap will unpause
    if (isMobile && paused && IsGestureDetected(GESTURE_TAP)) {
        paused = false;
        return;
    }    

    bool running = (firstTimeGameStart == false && paused == false && lostWindowFocus == false && isInExitMenu == false && gameOver == false);

    if (!firstTimeGameStart && !musicStarted && backgroundMusic.stream.buffer != NULL && playingMusic) {
        PlayMusicStream(backgroundMusic);
        musicStarted = true;
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Started playing background music");
        #endif
    }
    
    if (musicStarted && backgroundMusic.stream.buffer != NULL && playingMusic) {
        UpdateMusicStream(backgroundMusic);

        if (paused || lostWindowFocus || isInExitMenu || gameOver) {
            PauseMusicStream(backgroundMusic);
        } else {
            ResumeMusicStream(backgroundMusic);
        }
    }

    if (running)
    {
        bool thrusting=false, rotatingLeft=false, rotatingRight=false;

        if(!isMobile) {         
            thrusting = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
            rotatingLeft = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
            rotatingRight = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);

            if(IsKeyPressed(KEY_M)) {
                playingMusic = !playingMusic;
                if(playingMusic) {
                    PlayMusicStream(backgroundMusic);
                } else {
                    PauseMusicStream(backgroundMusic);
                }
            }
        }
        else {
            const float tapRadiusMultiplier = 2.5f; // Make touch detection area larger than visual buttons
            
            // Track whether each control has been activated by any touch
            bool leftButtonPressed = false;
            bool rightButtonPressed = false;
            bool thrustButtonPressed = false;
            bool centerAreaTapped = false;
            
            // Touch state tracking to preserve thrust between frames
            static bool wasThrusting = false;
            static double lastThrustTime = 0.0;
            double currentTime = GetTime();
            
            // Check all possible touch points (most devices support up to 10)
            int touchCount = GetTouchPointCount();
            debugTouchCount = touchCount; // Update debug variable
            
            for (int i = 0; i < touchCount; i++) {
                Vector2 touchPosition = GetTouchPosition(i);
                float screenWidth = GetScreenWidth();
                float screenHeight = GetScreenHeight();
                
                // Convert touch position to game coordinates
                float gameY = (touchPosition.y - (screenHeight - (gameScreenHeight * screenScale)) * 0.5f) / screenScale;
                // Button dimensions
                float buttonRadius = 40.0f;
                Vector2 leftButtonPos = {buttonRadius * 1.5f, gameScreenHeight / 2.0f};
                Vector2 rightButtonPos = {gameScreenWidth - buttonRadius * 1.5f, gameScreenHeight / 2.0f};
                
                // Convert from game coordinates to screen coordinates
                Vector2 leftButtonScreenPos = {
                    (screenWidth - (gameScreenWidth * screenScale)) * 0.5f + leftButtonPos.x * screenScale,
                    (screenHeight - (gameScreenHeight * screenScale)) * 0.5f + leftButtonPos.y * screenScale
                };
                
                Vector2 rightButtonScreenPos = {
                    (screenWidth - (gameScreenWidth * screenScale)) * 0.5f + rightButtonPos.x * screenScale,
                    (screenHeight - (gameScreenHeight * screenScale)) * 0.5f + rightButtonPos.y * screenScale
                };
                
                // Check if touch is within left button (with expanded touch area)
                if (CheckCollisionPointCircle(touchPosition, leftButtonScreenPos, buttonRadius * screenScale * tapRadiusMultiplier)) {
                    leftButtonPressed = true;
                }
                // Check if touch is within right button (with expanded touch area)
                else if (CheckCollisionPointCircle(touchPosition, rightButtonScreenPos, buttonRadius * screenScale * tapRadiusMultiplier)) {
                    rightButtonPressed = true;
                }
                // Check if center area is tapped (for thrust or continuing)
                else if (gameY > 100) {
                    // If the game is running normally and not in a special state, apply thrust
                    if (!firstTimeGameStart && !paused && !lostWindowFocus && !isInExitMenu && 
                        !gameOver && !lander->IsLanded() && !lander->IsCrashed()) {
                        thrustButtonPressed = true;
                        lastThrustTime = currentTime; // Update the last thrust time
                        debugLastThrustTime = lastThrustTime; // Update debug variable
                    }
                    
                    // Track center taps for handling special states
                    if (IsGestureDetected(GESTURE_TAP)) {
                        centerAreaTapped = true;
                    }
                }
                // Toggle pause if touch is in title bar area and it's a tap (not hold)
                else if (gameY <= 100 && IsGestureDetected(GESTURE_TAP) && !paused) {
                    // Only toggle pause if we're in normal gameplay
                    if (!firstTimeGameStart && !lostWindowFocus && !isInExitMenu && 
                        !gameOver && !lander->IsLanded() && !lander->IsCrashed()) {
                        paused = true;
                    }
                    // Break after toggling pause to avoid processing other touches during this frame
                    break;
                }
            }
            
            // Apply touch preservation for smoother thrust
            if (!thrustButtonPressed && wasThrusting && (currentTime - lastThrustTime < thrustTimeout)) {
                // Preserve thrust for a short time to handle brief gaps in touch detection
                thrustButtonPressed = true;
            }
            
            wasThrusting = thrustButtonPressed;
            
            // Update debug variables
            debugThrustActive = thrustButtonPressed;
            debugWasThrusting = wasThrusting;
            
            // Apply the control inputs for ship movement
            rotatingRight = leftButtonPressed;   // Rotate left in game is right button (directional confusion in code)
            rotatingLeft = rightButtonPressed;   // Rotate right in game is left button (directional confusion in code)
            thrusting = thrustButtonPressed;
            
            // Handle special states with center area taps
            if (centerAreaTapped) {
                // Start game from title screen
                if (firstTimeGameStart) {
                    firstTimeGameStart = false;
                }
                // Restart after game over or winning the game
                else if (gameOver || gameWon) {
                    Reset();
                }
                // Try again after crashing
                else if (lander->IsCrashed() && !gameOver) {
                    if (lives <= 1) {  
                        gameOver = true;
                    } else {
                        lives--;
                        lander->Reset(width, height);
                        Randomize(); 
                        lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
                        explosionCompleted = false; 
                    }
                }
            }
        }

        lander->Update(dt, thrusting, rotatingLeft, rotatingRight);

        if (lander->IsLanded() || lander->IsCrashed()) {
            if (lander->IsCrashed()) {  
                if (!explosionActive && !explosionCompleted) {
                    StartExplosion(lander->GetCrashPosX(), lander->GetCrashPosY());
                    explosionCompleted = true; 
                }
                
                if (lives <= 1) {  
                    gameOver = true;
                } else if (IsKeyPressed(KEY_ENTER) || (isMobile && IsGestureDetected(GESTURE_TAP))) {
                    lives--;
                    lander->Reset(width, height);
                    Randomize(); 
                    lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
                    explosionCompleted = false; 
                }
            } else if (GetTime() - lander->GetLandingTime() > inputDelay && 
                       (IsKeyPressed(KEY_ENTER) || (isMobile && IsGestureDetected(GESTURE_TAP)))) {
                
                // Check if player completed the final level (10 for mobile, 15 for desktop)
                int winLevel = isMobile ? 10 : 15;
                if (level >= winLevel) {
                    gameWon = true;
                    return;
                }
                
                Game::gravity += gravityIncrease;
                if(Game::gravity > MAX_GRAVITY) {
                    Game::gravity = MAX_GRAVITY;
                    Game::maxGravityReached = true;
                }
                if(Game::maxGravityReached) {
                    Lander::fuelConsumption += Game::fuelConsumptionIncrease;
                    if(Lander::fuelConsumption > MAX_FUEL_CONSUMPTION) {
                        Lander::fuelConsumption = MAX_FUEL_CONSUMPTION;
                    }
                }
                level++;
                lander->Reset(width, height);
                Randomize(); 
                lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
            }
        }
    }
}

void Game::UpdateUI()
{
#ifndef EMSCRIPTEN_BUILD
    if (WindowShouldClose() || (IsKeyPressed(KEY_ESCAPE) && exitWindowRequested == false))
    {
        exitWindowRequested = true;
        isInExitMenu = true;
        return;
    }

    if (IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))
    {
        if (fullscreen)
        {
            fullscreen = false;
            ToggleBorderlessWindowed();
        }
        else
        {
            fullscreen = true;
            ToggleBorderlessWindowed();
        }
    }
#endif

    if(firstTimeGameStart ) {
        if(isMobile) {
            if(IsGestureDetected(GESTURE_TAP)) {
                firstTimeGameStart = false;
            }
        }
        else if(IsKeyDown(KEY_ENTER)) {
            firstTimeGameStart = false;
        }
    }

    if (exitWindowRequested)
    {
        if (IsKeyPressed(KEY_Y))
        {
            exitWindow = true;
        }
        else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            exitWindowRequested = false;
            isInExitMenu = false;
        }
    }

    if (IsWindowFocused() == false)
    {
        lostWindowFocus = true;
    }
    else
    {
        lostWindowFocus = false;
    }

#ifndef EMSCRIPTEN_BUILD
    if (exitWindowRequested == false && lostWindowFocus == false && gameOver == false && IsKeyPressed(KEY_P))
#else
    if (exitWindowRequested == false && lostWindowFocus == false && gameOver == false && 
        (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)))
#endif
    {
        paused = !paused;
    }

    if (gameOver && (IsKeyPressed(KEY_ENTER) || (isMobile && IsGestureDetected(GESTURE_TAP)))) {
        Reset();
    }
    
    if (gameWon && (IsKeyPressed(KEY_ENTER) || (isMobile && IsGestureDetected(GESTURE_TAP)))) {
        Reset();
    }
}

void Game::Draw()
{    
    BeginTextureMode(targetRenderTex);
    ClearBackground(BLACK);

    DrawTexturePro(backgroundTexture, (Rectangle){0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height}, (Rectangle){0, 0, (float)gameScreenWidth, (float)gameScreenHeight}, (Vector2){0, 0}, 0.0f, WHITE);
    DrawTerrain();
    
    float landingPadX = lander->GetLandingPadX();
    float padY = gameScreenHeight - 50;
    float padWidth = 100;
    float padHeight = 5;

    if (landingPadTexture.id != 0) {
        float aspectRatio = (float)landingPadTexture.width / landingPadTexture.height;
        float drawHeight = 100.0f; 
        float drawWidth = drawHeight * aspectRatio;
        Rectangle source = { 0, 0, (float)landingPadTexture.width, (float)landingPadTexture.height };
        Rectangle dest = { landingPadX - drawWidth/2, padY - drawHeight/2 + 5.0f, drawWidth, drawHeight };
        DrawTexturePro(landingPadTexture, source, dest, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        Color padColorLeft = (Color){ 150, 150, 150, 255 };   
        Color padColorRight = (Color){ 200, 200, 200, 255 };  
        DrawRectangleGradientH(landingPadX - padWidth/2, padY, padWidth, padHeight, padColorLeft, padColorRight);
        DrawRectangleLines(landingPadX - padWidth/2, padY, padWidth, padHeight, GREEN);
        DrawLine(landingPadX - padWidth/2, padY + padHeight, landingPadX - padWidth/2 + 10, padY + 15, GREEN);
        DrawLine(landingPadX + padWidth/2, padY + padHeight, landingPadX + padWidth/2 - 10, padY + 15, GREEN);
    }

    lander->Draw();
    DrawExplosion();
    DrawUI();
    
    // Draw mobile controls - always show them on mobile
    if (isMobile) {
        // Button dimensions
        float buttonRadius = 40.0f;
        Vector2 leftButtonPos = {buttonRadius * 1.5f, gameScreenHeight / 2.0f};
        Vector2 rightButtonPos = {gameScreenWidth - buttonRadius * 1.5f, gameScreenHeight / 2.0f};
        
        // Draw a subtle indicator for the pause area
        if (!firstTimeGameStart && !paused && !lostWindowFocus && !isInExitMenu && 
            !gameOver && !lander->IsLanded() && !lander->IsCrashed()) {
            DrawRectangle(0, 0, gameScreenWidth, 100, ColorAlpha(DARKGRAY, 0.1f));
            const char* pauseIndicator = "Tap here to pause";
            Vector2 pauseSize = MeasureTextEx(font, pauseIndicator, 20, 1);
            DrawTextEx(font, pauseIndicator, {(gameScreenWidth / 2) - (pauseSize.x / 2), 70}, 20, 1, ColorAlpha(WHITE, 0.5f));
        }
        
        // Draw rotation buttons
        DrawCircle(leftButtonPos.x, leftButtonPos.y, buttonRadius, Fade(DARKGRAY, 0.6f));
        DrawCircle(rightButtonPos.x, rightButtonPos.y, buttonRadius, Fade(DARKGRAY, 0.6f));
        
        // Draw arrows inside buttons
        // Left button arrow - pointing LEFT (counter-clockwise order)
        Vector2 leftArrowPoints[3] = {
            {leftButtonPos.x - buttonRadius * 0.3f, leftButtonPos.y}, // Tip (left)
            {leftButtonPos.x + buttonRadius * 0.3f, leftButtonPos.y + buttonRadius * 0.5f},  // Bottom-right
            {leftButtonPos.x + buttonRadius * 0.3f, leftButtonPos.y - buttonRadius * 0.5f} // Top-right
        };
        DrawTriangle(leftArrowPoints[0], leftArrowPoints[1], leftArrowPoints[2], WHITE);
        
        // Right button arrow - pointing RIGHT (counter-clockwise order)
        Vector2 rightArrowPoints[3] = {
            {rightButtonPos.x - buttonRadius * 0.3f, rightButtonPos.y - buttonRadius * 0.5f},  // Top-left
            {rightButtonPos.x - buttonRadius * 0.3f, rightButtonPos.y + buttonRadius * 0.5f}, // Bottom-left
            {rightButtonPos.x + buttonRadius * 0.3f, rightButtonPos.y} // Tip (right)
        };
        DrawTriangle(rightArrowPoints[0], rightArrowPoints[1], rightArrowPoints[2], WHITE);
        
        // Add a help text for thrust
        const char* thrustHelp = "Tap screen for thrust";
        Vector2 thrustHelpSize = MeasureTextEx(font, thrustHelp, 25, 1);
        DrawTextEx(font, thrustHelp, { (float)(gameScreenWidth / 2 - thrustHelpSize.x / 2), (float)(gameScreenHeight - 30) }, 25, 1, WHITE);
    }
    
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(targetRenderTex.texture, 
        (Rectangle){0.0f, 0.0f, (float)targetRenderTex.texture.width, (float)-targetRenderTex.texture.height},
        (Rectangle){(GetScreenWidth() - ((float)gameScreenWidth * screenScale)) * 0.5f, 
                   (GetScreenHeight() - ((float)gameScreenHeight * screenScale)) * 0.5f, 
                   (float)gameScreenWidth * screenScale, 
                   (float)gameScreenHeight * screenScale},
        (Vector2){0, 0}, 0.0f, WHITE);
    EndDrawing();
}

void Game::DrawTerrain()
{
    SetTextureFilter(terrainTexture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(terrainTexture, TEXTURE_WRAP_REPEAT);

    for (int i = 0; i < TERRAIN_POINTS - 1; ++i) {
        float segmentWidth = terrainPoints[i+1].x - terrainPoints[i].x;
        if (segmentWidth < 1.0f) continue; 
        
        const int subdivisions = 20;
        
        for (int j = 0; j < subdivisions; j++) {
            float t1 = (float)j / subdivisions;
            float t2 = (float)(j + 1) / subdivisions;

            float x1 = terrainPoints[i].x + t1 * segmentWidth;
            float x2 = terrainPoints[i].x + t2 * segmentWidth;
            float y1 = terrainPoints[i].y + t1 * (terrainPoints[i+1].y - terrainPoints[i].y);
            float y2 = terrainPoints[i].y + t2 * (terrainPoints[i+1].y - terrainPoints[i].y);

            float textureVisiblePortion = 0.99f;  

            float offsetScale = 0.0001f;  
            float globalOffsetX = offsetScale * (i * 10 + j);  
            float globalOffsetY = offsetScale * i * 5;  

            Rectangle source;

            source.width = (float)terrainTexture.width * textureVisiblePortion;
            source.height = (float)terrainTexture.height * textureVisiblePortion;

            source.x = fmodf(globalOffsetX, (float)(terrainTexture.width - source.width));
            source.y = fmodf(globalOffsetY, (float)(terrainTexture.height - source.height));

            float yTop = std::min(y1, y2);
            float height = gameScreenHeight - yTop;
            Rectangle dest = { x1, yTop, x2 - x1, height };

            DrawTexturePro(terrainTexture, source, dest, (Vector2){0, 0}, 0, WHITE);
        }
    }
    
    const unsigned char outlineColor = 128;
    for (int i = 0; i < TERRAIN_POINTS - 1; ++i) {
        DrawLineEx(terrainPoints[i], terrainPoints[i+1], 1.0f, {outlineColor, outlineColor, outlineColor, 255});
    }
}

void Game::DrawUI()
{
    float screenX = 0.0f;
    float screenY = 0.0f;

    DrawTextEx(font, "Houston Control", {400, 10}, 34, 2, WHITE);

    // Show fuel warnings when appropriate (only during active gameplay)
    if (!firstTimeGameStart && !paused && !lostWindowFocus && !isInExitMenu && 
        !gameOver && !gameWon && !lander->IsLanded() && !lander->IsCrashed()) {
        float fuelPercentage = lander->GetFuel();
        
        // Create a pulsing effect for the warnings
        float alpha = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.3f + 0.4f; // Pulse between 0.4 and 1.0 alpha
        
        if (fuelPercentage <= 0.0f) {
            // Out of fuel warning
            const char* outOfFuelText = "Out of Fuel!";
            Vector2 textSize = MeasureTextEx(font, outOfFuelText, 28, 2);
            float boxWidth = textSize.x + 40;
            float boxHeight = textSize.y + 20;
            float boxX = gameScreenWidth / 2 - boxWidth / 2;
            float boxY = gameScreenHeight / 2 - 110;
            
            // Draw background box
            DrawRectangle(boxX, boxY, boxWidth, boxHeight, ColorAlpha(BLACK, 0.7f));
            DrawRectangleLines(boxX, boxY, boxWidth, boxHeight, ColorAlpha(RED, alpha));
            
            DrawTextEx(font, outOfFuelText, 
                     {(float)(gameScreenWidth / 2 - textSize.x / 2), boxY + 10}, 
                     28, 2, RED);
        } else if (fuelPercentage < 35.0f) {
            // Low fuel warning
            const char* lowFuelText = "Warning! Low Fuel";
            Vector2 textSize = MeasureTextEx(font, lowFuelText, 28, 2);
            float boxWidth = textSize.x + 40;
            float boxHeight = textSize.y + 20;
            float boxX = gameScreenWidth / 2 - boxWidth / 2;
            float boxY = gameScreenHeight / 2 - 110;
            
            // Draw background box
            DrawRectangle(boxX, boxY, boxWidth, boxHeight, ColorAlpha(BLACK, 0.7f));
            DrawRectangleLines(boxX, boxY, boxWidth, boxHeight, ColorAlpha(YELLOW, alpha));
            
            DrawTextEx(font, lowFuelText, 
                     {(float)(gameScreenWidth / 2 - textSize.x / 2), boxY + 10}, 
                     28, 2, YELLOW);
        }
    }

    if (exitWindowRequested)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 70}, 0.76f, 20, BLACK);
        const char* exitText = "Are you sure you want to exit? [Y/N]";
        Vector2 exitSize = MeasureTextEx(font, exitText, 25, 2);
        DrawTextEx(font, exitText, {screenX + (gameScreenWidth / 2 - exitSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
    }
    else if (firstTimeGameStart)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 350), screenY + (float)(gameScreenHeight / 2 - 200), 650, 430}, 0.76f, 20, BLACK);
        const char* welcomeText = "Welcome to Houston Control";
        Vector2 welcomeSize = MeasureTextEx(font, welcomeText, 30, 2);
        DrawTextEx(font, welcomeText, {screenX + (gameScreenWidth / 2 - welcomeSize.x/2), screenY + gameScreenHeight / 2 - 180}, 30, 2, GREEN);
        
        const char* objective1 = "The objective is to land on the landing pad while";
        const char* objective2 = "carefully managing landing speed and angle.";
        const char* objective3 = "";
        if(!isMobile) {
            objective3 = "Try to get to level 15 to beat the game.";
        } else {
            objective3 = "Try to get to level 10 to beat the game.";
        }
        const char* objective4 = "Each level you will face tougher gravity";
        const char* objective5 = "and fuel restrictions.";
        
        DrawTextEx(font, objective1, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 - 140}, 25, 2, WHITE);
        DrawTextEx(font, objective2, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 - 110}, 25, 2, WHITE);
        DrawTextEx(font, objective3, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 - 80}, 25, 2, WHITE);
        DrawTextEx(font, objective4, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 - 50}, 25, 2, WHITE);
        DrawTextEx(font, objective5, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 - 20}, 25, 2, WHITE);

        if (isMobile) {
            const char* controls1 = "Controls: Tap screen for thrust, tap top area to pause";
            const char* controls2 = "Tap left/right buttons to rotate";
            DrawTextEx(font, controls1, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 20}, 25, 2, yellow);
            DrawTextEx(font, controls2, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 50}, 25, 2, yellow);
            
            const char* tapText = "Tap to play";
            Vector2 tapTextSize = MeasureTextEx(font, tapText, 25, 2);
            DrawTextEx(font, tapText, {screenX + (gameScreenWidth / 2 - tapTextSize.x/2), screenY + gameScreenHeight / 2 + 90}, 25, 2, GREEN);

            const char* noteText = "For best experience, play the desktop version";
            Vector2 noteTextSize = MeasureTextEx(font, noteText, 25, 2);
            DrawTextEx(font, noteText, {screenX + (gameScreenWidth / 2 - noteTextSize.x/2), screenY + gameScreenHeight / 2 + 130}, 25, 2, WHITE);
        } else {
#ifndef EMSCRIPTEN_BUILD            
            const char* controls1 = "Controls: Arrow Up/W for thrust";
            const char* controls2 = "Arrow Left/A and Right/D to rotate";
            const char* controls3 = "M to toggle music, P to pause, ESC to exit";
            DrawTextEx(font, controls1, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 20}, 25, 2, yellow);
            DrawTextEx(font, controls2, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 50}, 25, 2, yellow);
            DrawTextEx(font, controls3, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 80}, 25, 2, yellow);
            
            const char* enterText = "Press Enter to play";
            Vector2 enterTextSize = MeasureTextEx(font, enterText, 25, 2);
            DrawTextEx(font, enterText, {screenX + (gameScreenWidth / 2 - enterTextSize.x/2), screenY + gameScreenHeight / 2 + 130}, 25, 2, GREEN);
            
            const char* fullscreenText = "Alt+Enter: toggle fullscreen";
            Vector2 fullscreenTextSize = MeasureTextEx(font, fullscreenText, 25, 2);
            DrawTextEx(font, fullscreenText, {screenX + (gameScreenWidth / 2 - fullscreenTextSize.x/2), screenY + gameScreenHeight / 2 + 170}, 25, 2, WHITE);
#else
            const char* controls1 = "Controls: Arrow Up/W for thrust";
            const char* controls2 = "Arrow Left/A and Right/D to rotate";
            const char* controls3 = "M to toggle music, P or ESC to pause";
            DrawTextEx(font, controls1, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 20}, 25, 2, yellow);
            DrawTextEx(font, controls2, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 50}, 25, 2, yellow);
            DrawTextEx(font, controls3, {screenX + (gameScreenWidth / 2 - 275), screenY + gameScreenHeight / 2 + 80}, 25, 2, yellow);
            
            const char* enterText = "Press Enter to play";
            Vector2 enterTextSize = MeasureTextEx(font, enterText, 25, 2);
            DrawTextEx(font, enterText, {screenX + (gameScreenWidth / 2 - enterTextSize.x/2), screenY + gameScreenHeight / 2 + 130}, 25, 2, GREEN);
#endif
        }
    }
    else if (paused)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 70}, 0.76f, 20, BLACK);
#ifndef EMSCRIPTEN_BUILD
        const char* pausedText = "Game paused, press P to continue";
        Vector2 pausedSize = MeasureTextEx(font, pausedText, 25, 2);
        DrawTextEx(font, pausedText, {screenX + (gameScreenWidth / 2 - pausedSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
#else
        if (isMobile) {
            const char* pausedText = "Game paused, tap to continue";
            Vector2 pausedSize = MeasureTextEx(font, pausedText, 25, 2);
            DrawTextEx(font, pausedText, {screenX + (gameScreenWidth / 2 - pausedSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
        } else {
            const char* pausedText = "Game paused, press P or ESC to continue";
            Vector2 pausedSize = MeasureTextEx(font, pausedText, 25, 2);
            DrawTextEx(font, pausedText, {screenX + (gameScreenWidth / 2 - pausedSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
        }
#endif
    }
    else if (lostWindowFocus)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 70}, 0.76f, 20, BLACK);
        const char* focusText = "Game paused, focus window to continue";
        Vector2 focusSize = MeasureTextEx(font, focusText, 25, 2);
        DrawTextEx(font, focusText, {screenX + (gameScreenWidth / 2 - focusSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
    }
    else if (gameOver)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 70}, 0.76f, 20, BLACK);
        if (isMobile) {
            const char* gameOverText = "Game over, tap to play again";
            Vector2 gameOverSize = MeasureTextEx(font, gameOverText, 25, 2);
            DrawTextEx(font, gameOverText, {screenX + (gameScreenWidth / 2 - gameOverSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
        } else {
            const char* gameOverText = "Game over, press Enter to play again";
            Vector2 gameOverSize = MeasureTextEx(font, gameOverText, 25, 2);
            DrawTextEx(font, gameOverText, {screenX + (gameScreenWidth / 2 - gameOverSize.x/2), screenY + gameScreenHeight / 2}, 25, 2, yellow);
        }
    }
    else if (gameWon)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 70}, 0.76f, 20, BLACK);
        if (isMobile) {
            const char* congratsText = "Congratulations! You completed all 10 levels!";
            Vector2 congratsSize = MeasureTextEx(font, congratsText, 25, 2);
            DrawTextEx(font, congratsText, {screenX + (gameScreenWidth / 2 - congratsSize.x/2), screenY + gameScreenHeight / 2 - 15}, 25, 2, GREEN);
        } else {
            const char* congratsText = "Congratulations! You completed all 15 levels!";
            Vector2 congratsSize = MeasureTextEx(font, congratsText, 25, 2);
            DrawTextEx(font, congratsText, {screenX + (gameScreenWidth / 2 - congratsSize.x/2), screenY + gameScreenHeight / 2 - 15}, 25, 2, GREEN);
        }
        if (isMobile) {
            const char* playAgainText = "Tap to play again";
            Vector2 playAgainSize = MeasureTextEx(font, playAgainText, 25, 2);
            DrawTextEx(font, playAgainText, {screenX + (gameScreenWidth / 2 - playAgainSize.x/2), screenY + gameScreenHeight / 2 + 15}, 25, 2, WHITE);
        } else {
            const char* playAgainText = "Press Enter to play again";
            Vector2 playAgainSize = MeasureTextEx(font, playAgainText, 25, 2);
            DrawTextEx(font, playAgainText, {screenX + (gameScreenWidth / 2 - playAgainSize.x/2), screenY + gameScreenHeight / 2 + 15}, 25, 2, WHITE);
        }
    }
    else if (lander->IsLanded())
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 70}, 0.76f, 20, BLACK);
        const char* landingText = "Landing Successful!";
        Vector2 landingTextSize = MeasureTextEx(font, landingText, 25, 2);
        DrawTextEx(font, landingText, {screenX + (gameScreenWidth / 2 - landingTextSize.x/2), screenY + gameScreenHeight / 2 - 15}, 25, 2, GREEN);
        if (isMobile) {
            const char* nextLevelText = "Tap for next level";
            Vector2 nextLevelSize = MeasureTextEx(font, nextLevelText, 25, 2);
            DrawTextEx(font, nextLevelText, {screenX + (gameScreenWidth / 2 - nextLevelSize.x/2), screenY + gameScreenHeight / 2 + 15}, 25, 2, WHITE);
        } else {
            const char* nextLevelText = "Press Enter for next level";
            Vector2 nextLevelSize = MeasureTextEx(font, nextLevelText, 25, 2);
            DrawTextEx(font, nextLevelText, {screenX + (gameScreenWidth / 2 - nextLevelSize.x/2), screenY + gameScreenHeight / 2 + 15}, 25, 2, WHITE);
        }
    }
    else if (lander->IsCrashed() && lives > 0)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 25), 500, 85}, 0.76f, 20, BLACK);
        std::string crashReason = GetCrashReason();
        const char* crashText = TextFormat("Crashed! %s", crashReason.c_str());
        Vector2 textSize = MeasureTextEx(font, crashText, 25, 2);
        DrawTextEx(font, crashText, {screenX + (gameScreenWidth / 2 - textSize.x/2), screenY + gameScreenHeight / 2 - 5}, 25, 2, RED);
        if (isMobile) {
            const char* tryAgainText = "Tap to try again";
            Vector2 tryAgainSize = MeasureTextEx(font, tryAgainText, 25, 2);
            DrawTextEx(font, tryAgainText, {screenX + (gameScreenWidth / 2 - tryAgainSize.x/2), screenY + gameScreenHeight / 2 + 25}, 25, 2, WHITE);
        } else {
            const char* tryAgainText = "Press Enter to try again";
            Vector2 tryAgainSize = MeasureTextEx(font, tryAgainText, 25, 2);
            DrawTextEx(font, tryAgainText, {screenX + (gameScreenWidth / 2 - tryAgainSize.x/2), screenY + gameScreenHeight / 2 + 25}, 25, 2, WHITE);
        }
    }

    int rightMargin = 20;
    int lineHeight = 30;   
    int startY = 10;       

    const char* levelText = TextFormat("Level: %d", level);
    Vector2 levelSize = MeasureTextEx(font, levelText, 25, 2);
    DrawTextEx(font, levelText, { (float)(gameScreenWidth - levelSize.x - rightMargin), (float)startY }, 25, 2, WHITE);

    const char* livesText = TextFormat("Lives: %d", lives);
    Vector2 livesSize = MeasureTextEx(font, livesText, 25, 2);
    DrawTextEx(font, livesText, { (float)(gameScreenWidth - livesSize.x - rightMargin), (float)(startY + lineHeight) }, 25, 2, WHITE);

    const char* fuelText = TextFormat("Fuel: %.1f", lander->GetFuel());
    Vector2 fuelSize = MeasureTextEx(font, fuelText, 25, 2);
    Color fuelColor = WHITE;
    // Check if we're actively using fuel (thrusting or rotating)
    bool isUsingFuel = false;
    if (!isMobile) {
        isUsingFuel = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A) || 
                      IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) && lander->GetFuel() > 0;
    } else {
        // For mobile, check if any touch is active in the thrust or rotation areas
        int touchCount = GetTouchPointCount();
        for (int i = 0; i < touchCount; i++) {
            Vector2 touchPosition = GetTouchPosition(i);
            float screenWidth = GetScreenWidth();
            float screenHeight = GetScreenHeight();
            float gameY = (touchPosition.y - (screenHeight - (gameScreenHeight * screenScale)) * 0.5f) / screenScale;
            
            // Check if touch is in thrust area (center of screen) or rotation buttons
            float buttonRadius = 40.0f;
            Vector2 leftButtonPos = {buttonRadius * 1.5f, gameScreenHeight / 2.0f};
            Vector2 rightButtonPos = {gameScreenWidth - buttonRadius * 1.5f, gameScreenHeight / 2.0f};
            
            Vector2 leftButtonScreenPos = {
                (screenWidth - (gameScreenWidth * screenScale)) * 0.5f + leftButtonPos.x * screenScale,
                (screenHeight - (gameScreenHeight * screenScale)) * 0.5f + leftButtonPos.y * screenScale
            };
            
            Vector2 rightButtonScreenPos = {
                (screenWidth - (gameScreenWidth * screenScale)) * 0.5f + rightButtonPos.x * screenScale,
                (screenHeight - (gameScreenHeight * screenScale)) * 0.5f + rightButtonPos.y * screenScale
            };
            
            if (gameY > 100 || // Thrust area
                CheckCollisionPointCircle(touchPosition, leftButtonScreenPos, buttonRadius * screenScale * 2.5f) ||
                CheckCollisionPointCircle(touchPosition, rightButtonScreenPos, buttonRadius * screenScale * 2.5f)) {
                isUsingFuel = true;
                break;
            }
        }
    }
    
    if (isUsingFuel && lander->GetFuel() > 0) {
        fuelColor = RED;
    }
    DrawTextEx(font, fuelText, { (float)(gameScreenWidth - fuelSize.x - rightMargin), (float)(startY + lineHeight * 2) }, 25, 2, fuelColor);

    const char* fuelConsumptionText = TextFormat("Fuel Use: %.3f", Lander::fuelConsumption);
    Vector2 fuelConsumptionSize = MeasureTextEx(font, fuelConsumptionText, 25, 2);
    DrawTextEx(font, fuelConsumptionText, { (float)(gameScreenWidth - fuelConsumptionSize.x - rightMargin), (float)(startY + lineHeight * 3) }, 25, 2, WHITE);

    const char* velocityText = TextFormat("Velocity X: %.1f Y: %.1f", lander->GetVelocityX(), lander->GetVelocityY());
    Vector2 velocitySize = MeasureTextEx(font, velocityText, 25, 2);
    Color velocityColor = WHITE;
    if (fabsf(lander->GetVelocityX()) >= Game::velocityLimit || fabsf(lander->GetVelocityY()) >= Game::velocityLimit) {
        velocityColor = RED;
    }
    DrawTextEx(font, velocityText, { (float)(gameScreenWidth - velocitySize.x - rightMargin), (float)(startY + lineHeight * 4) }, 25, 2, velocityColor);

    const char* angleText = TextFormat("Angle: %.1f", lander->GetAngle());
    Vector2 angleSize = MeasureTextEx(font, angleText, 25, 2);
    Color angleColor = WHITE;
    float normalizedAngle = fmodf(lander->GetAngle() + 180.0f, 360.0f) - 180.0f;
    if (fabsf(normalizedAngle) >= 15.0f) {
        angleColor = RED;
    }
    DrawTextEx(font, angleText, { (float)(gameScreenWidth - angleSize.x - rightMargin), (float)(startY + lineHeight * 5) }, 25, 2, angleColor);

    const char* gravityText = TextFormat("Gravity: %.3f", Game::gravity);
    Vector2 gravitySize = MeasureTextEx(font, gravityText, 25, 2);
    DrawTextEx(font, gravityText, { (float)(gameScreenWidth - gravitySize.x - rightMargin), (float)(startY + lineHeight * 6) }, 25, 2, WHITE);

    if (!isMobile) {
        const char* musicText = TextFormat("Press M to toggle music %s", playingMusic ? "(ON)" : "(OFF)");
        Vector2 musicTextSize = MeasureTextEx(font, musicText, 25, 1);
        DrawTextEx(font, musicText, { (float)(gameScreenWidth / 2 - musicTextSize.x / 2), (float)(gameScreenHeight - 30) }, 25, 1, WHITE);
    }
}

void Game::Randomize()
{    
    float segmentWidth = (float)gameScreenWidth / (TERRAIN_POINTS - 1);
    float minHeight = gameScreenHeight - minTerrainHeight;
    float maxHeight = gameScreenHeight - maxTerrainHeight;
    float landingPadCenter = lander->GetLandingPadX();
    float landingPadHalfWidth = 50.0f;
    float landingPadHeight = gameScreenHeight - 50;

    for (int i = 0; i < TERRAIN_POINTS; i++) {
        float x = i * segmentWidth;
        float y;

        if (x >= landingPadCenter - landingPadHalfWidth - segmentWidth && 
            x <= landingPadCenter + landingPadHalfWidth + segmentWidth) {
            
            if (x < landingPadCenter - landingPadHalfWidth) {
                
                float t = (landingPadCenter - landingPadHalfWidth - x) / segmentWidth;
                y = landingPadHeight - (t * t * 10);
            } else if (x > landingPadCenter + landingPadHalfWidth) {
                
                float t = (x - (landingPadCenter + landingPadHalfWidth)) / segmentWidth;
                y = landingPadHeight - (t * t * 10);
            } else {
                
                y = landingPadHeight;
            }
        } else {
            
            y = GetRandomValue(minHeight, maxHeight);
        }
        
        terrainPoints[i] = (Vector2){ x, y };
    }

    Vector2 smoothedPoints[TERRAIN_POINTS];
    for (int i = 0; i < TERRAIN_POINTS; i++) {
        smoothedPoints[i] = terrainPoints[i];
    }

    for (int i = 1; i < TERRAIN_POINTS - 1; i++) {
        float x = i * segmentWidth;

        if (x >= landingPadCenter - landingPadHalfWidth - segmentWidth && 
            x <= landingPadCenter + landingPadHalfWidth + segmentWidth) {
            continue;
        }

        smoothedPoints[i].y = (terrainPoints[i-1].y + terrainPoints[i].y + terrainPoints[i+1].y) / 3.0f;
    }

    for (int i = 1; i < TERRAIN_POINTS - 1; i++) {
        float x = i * segmentWidth;

        if (x >= landingPadCenter - landingPadHalfWidth - segmentWidth && 
            x <= landingPadCenter + landingPadHalfWidth + segmentWidth) {
            continue;
        }

        terrainPoints[i].y = (smoothedPoints[i-1].y + smoothedPoints[i].y + smoothedPoints[i+1].y) / 3.0f;
    }
}

void Game::DrawExplosion()
{
    if (!explosionActive) return;

    explosionFramesCounter++;
    
    if (explosionFramesCounter > explosionPlaybackSpeed) {  
        explosionCurrentFrame++;
        
        if (explosionCurrentFrame >= EXPLOSION_FRAMES_PER_LINE) {
            explosionCurrentFrame = 0;
            explosionCurrentLine++;
            
            if (explosionCurrentLine >= EXPLOSION_LINES) {
                
                explosionCurrentLine = 0;
                explosionActive = false;
                return;
            }
        }
        
        explosionFramesCounter = 0;
    }

    float frameWidth = (float)(explosionTexture.width / EXPLOSION_FRAMES_PER_LINE);
    float frameHeight = (float)(explosionTexture.height / EXPLOSION_LINES);
    explosionFrameRec.x = frameWidth * explosionCurrentFrame;
    explosionFrameRec.y = frameHeight * explosionCurrentLine;

    float scaledWidth = frameWidth * explosionScale;
    float scaledHeight = frameHeight * explosionScale;

    Rectangle destRect = {
        explosionPosition.x,
        explosionPosition.y,
        scaledWidth,
        scaledHeight
    };

    DrawTexturePro(explosionTexture, explosionFrameRec, destRect, (Vector2){0, 0}, 0.0f, WHITE);
}

void Game::StartExplosion(float x, float y)
{
    
    explosionActive = true;
    explosionCurrentFrame = 0;
    explosionCurrentLine = 0;
    explosionFramesCounter = 0;

    float frameWidth = (float)(explosionTexture.width / EXPLOSION_FRAMES_PER_LINE);
    float frameHeight = (float)(explosionTexture.height / EXPLOSION_LINES);

    float scaledWidth = frameWidth * explosionScale;
    float scaledHeight = frameHeight * explosionScale;

    explosionPosition.x = x - scaledWidth/2.0f;
    explosionPosition.y = y - scaledHeight/2.0f;
}

std::string Game::GetCrashReason() const {
    if (!lander->IsCrashed()) {
        return "";
    }
    
    float vx = fabs(lander->GetVelocityX());
    float vy = fabs(lander->GetVelocityY());
    float normalizedAngle = fmodf(lander->GetAngle() + 180.0f, 360.0f) - 180.0f;
    bool badAngle = fabs(normalizedAngle) >= 15.0f;
    bool highVelocityX = vx >= Game::velocityLimit;
    bool highVelocityY = vy >= Game::velocityLimit;
    
    // Check if we're near the landing pad
    float centerX = lander->GetX() + lander->GetWidth() * Lander::collisionScale / 2.0f;
    bool nearPad = fabs(centerX - lander->GetLandingPadX()) <= 50.0f;
    
    if (!nearPad) {
        return "Missed the landing pad!";
    } else if (badAngle && (highVelocityX || highVelocityY)) {
        return "Bad angle and too fast!";
    } else if (badAngle) {
        return "Bad landing angle!";
    } else if (highVelocityX && highVelocityY) {
        return "Too fast - both horizontal and vertical!";
    } else if (highVelocityX) {
        return "Too fast - horizontal velocity!";
    } else if (highVelocityY) {
        return "Too fast - vertical velocity!";
    } else {
        return "Something went wrong!";
    }
}