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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

//#define DEBUG  // Uncomment this line to enable debug logging

#define INITIAL_GRAVITY 1.0f
#define MAX_GRAVITY 2.0f
#define INITIAL_VELOCITY_LIMIT 0.8f
#define MUSIC_VOLUME 0.2f

//#define DEBUG_COLLISION

float Lander::thrust = 0.02f;
float Lander::rotationSpeed = 1.0f;
float Lander::fuelConsumption = 0.05f;
float Game::gravity = INITIAL_GRAVITY;
float Game::velocityLimit = INITIAL_VELOCITY_LIMIT;  // Non-const static member
bool Game::isMobile = false;
bool Game::maxGravityReached = false;

// Lander implementation
Lander::Lander(int screenWidth, int screenHeight) {
    // Load thrust sound
    thrustSound = LoadSound("data/thrust.mp3");
    if (thrustSound.stream.buffer == NULL) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load thrust sound: data/thrust.mp3");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded thrust sound");
        #endif
        SetSoundVolume(thrustSound, 1.0f);  // Set volume to 100%
    }

    landSound = LoadSound("data/land.mp3");
    if (landSound.stream.buffer == NULL) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load land sound: data/land.mp3");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded land sound");
        #endif
        SetSoundVolume(landSound, 1.0f);  // Set volume to 100%
    }

    crashSound = LoadSound("data/crash.mp3");
    if (crashSound.stream.buffer == NULL) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load crash sound: data/crash.mp3");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded crash sound");
        #endif
        SetSoundVolume(crashSound, 1.0f);  // Set volume to 100%
    }
    
    // Load lander texture
    texture = LoadTexture("data/lander.png");
    if (texture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load lander texture: data/lander.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded lander texture");
        #endif
    }
    
    // Load flame texture
    flameTexture = LoadTexture("data/blueflame.png");
    if (flameTexture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load flame texture: data/blueflame.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded flame texture");
        #endif
    }

    wasThrusting = false;
    wasRotating = false;
    Reset(screenWidth, screenHeight);
}

void Lander::Reset(int screenWidth, int screenHeight) {
    landerX = screenWidth / 2.0f;
    landerY = 50.0f;  // Start higher up
    velocityX = 0.0f;
    velocityY = 0.0f;
    angle = 0.0f;
    fuel = 100.0f;
    landed = false;
    crashed = false;
    
    // Set height and calculate width based on texture aspect ratio
    height = 60.0f;  // Keep the same height
    
    // Calculate width based on texture aspect ratio (256x384)
    // Original aspect ratio is 256/384 = 2/3
    // If texture wasn't loaded, use a default width
    if (texture.id != 0) {
        width = height * ((float)texture.width / texture.height);
    } else {
        width = 20.0f; // Default width if texture failed to load
    }
    
    // Random landing pad position
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(100.0f, screenWidth - 100.0f);
    landingPadX = dis(gen);
    landingTime = 0.0;
    
    // Stop thrust sound if it's playing
    StopSound(thrustSound);
    wasThrusting = false;
    wasRotating = false;
}

void Lander::SetTerrainReference(Vector2* terrain, int terrainPoints) {
    this->terrain = terrain;
    this->terrainPoints = terrainPoints;
}

void Lander::Update(float dt, bool thrusting, bool rotatingLeft, bool rotatingRight) {
    if (!landed && !crashed) {
        // Apply gravity
        velocityY += Game::gravity * dt;

        bool isRotating = (rotatingLeft || rotatingRight) && fuel > 0;
        bool shouldPlayThrustSound = (thrusting || isRotating) && fuel > 0;

        // Apply thrust if fuel available
        if (thrusting && fuel > 0) {
            velocityX += sinf(angle * DEG2RAD) * thrust;
            velocityY -= cosf(angle * DEG2RAD) * thrust;
            fuel = fmaxf(0.0f, fuel - fuelConsumption);
        }

        // Handle rotation - consume fuel for rotation too
        if (isRotating) {
            if (rotatingLeft) {
                angle = fmodf(angle + rotationSpeed, 360.0f);
            }
            if (rotatingRight) {
                angle = fmodf(angle - rotationSpeed, 360.0f);
            }
            // Consume fuel for rotation, but at a lower rate than thrust
            fuel = fmaxf(0.0f, fuel - (fuelConsumption * 0.5f));
        }

        // Handle sound effects
        if (shouldPlayThrustSound && thrustSound.stream.buffer != NULL) {
            if (!wasThrusting && !wasRotating) {
                PlaySound(thrustSound);
                #ifdef DEBUG
                TraceLog(LOG_INFO, "Started playing thrust sound");
                #endif
            }
            wasThrusting = thrusting;
            wasRotating = isRotating;
        } else if ((wasThrusting || wasRotating) && thrustSound.stream.buffer != NULL) {
            StopSound(thrustSound);
            wasThrusting = false;
            wasRotating = false;
            #ifdef DEBUG
            TraceLog(LOG_INFO, "Stopped thrust sound");
            #endif
        }

        // Update position
        landerX += velocityX;
        landerY += velocityY;

        // Screen boundaries
        landerX = fmaxf(0.0f, fminf(gameScreenWidth - width, landerX));
        if (landerY < 0.0f) {
            landerY = 0.0f;
            velocityY = 0.0f;
        }

        // Create a scaled-down collision rectangle
        float scaledWidth = width * collisionScale;
        float scaledHeight = height * collisionScale;
        // Center the collision box
        float collisionX = landerX; // - scaledWidth/2.0f;
        float collisionY = landerY; // - scaledHeight/2.0f;
        
        Rectangle collisionRect = { collisionX, collisionY, scaledWidth, scaledHeight };
        float collisionBottom = collisionY + scaledHeight;
        
        // Center point of the collision box for trajectory checks
        float centerX = collisionX + scaledWidth/2.0f;
        
        // Check if we've reached the terrain height
        for (int i = 0; i < terrainPoints - 1; i++) {
            // Check if lander is in this segment horizontally
            if (centerX >= terrain[i].x && centerX <= terrain[i+1].x) {
                // Calculate terrain height at this x position using linear interpolation
                float t = (centerX - terrain[i].x) / (terrain[i+1].x - terrain[i].x);
                float terrainHeight = terrain[i].y * (1 - t) + terrain[i+1].y * t;
                
                // If lander has hit the terrain
                if (collisionBottom >= terrainHeight) {
                    // Check if it's on the landing pad
                    if (fabsf(centerX - landingPadX) <= 50.0f &&
                        fabsf(terrainHeight - (gameScreenHeight - 50.0f)) < 1.0f &&
                        fabsf(velocityX) < Game::velocityLimit && 
                        fabsf(velocityY) < Game::velocityLimit) {
                        
                        float normalizedAngle = fmodf(angle + 180.0f, 360.0f) - 180.0f;
                        if (fabsf(normalizedAngle) < 15.0f) {
                            landed = true;
                            landingTime = GetTime();
                            PlaySound(landSound);
                            #ifdef DEBUG
                            TraceLog(LOG_INFO, "Land sound played");
                            #endif
                        } else {
                            crashed = true;
                            PlaySound(crashSound);
                            #ifdef DEBUG
                            TraceLog(LOG_INFO, "Crash sound played - wrong angle");
                            #endif
                        }
                    } else {
                        crashed = true;
                        PlaySound(crashSound);
                        #ifdef DEBUG
                        TraceLog(LOG_INFO, "Crash sound played - hit terrain");
                        #endif
                    }
                    
                    // Position lander on the terrain surface, adjusting for the collision box offset
                    landerY = terrainHeight - scaledHeight - (height - scaledHeight) / 2.0f;
                    break;
                }
            }
        }
    }
}

void Lander::Draw() {
    // Calculate the center of the lander for rotation
    Vector2 center = { landerX + width/2.0f, landerY + height/2.0f };
    
    if (texture.id != 0) {
        // Draw the lander texture with rotation
        Rectangle source = { 0, 0, (float)texture.width, (float)texture.height };
        Rectangle dest = { landerX + width/2.0f, landerY + height/2.0f, width, height };
        Vector2 origin = { width/2.0f, height/2.0f }; // Origin at the center for rotation
        
        // Draw the texture rotated around its center
        DrawTexturePro(texture, source, dest, origin, angle, WHITE);
                      
        // Draw collision box (for debugging)
        #ifdef DEBUG_COLLISION
        float scaledWidth = width * collisionScale;
        float scaledHeight = height * collisionScale;
        float collisionX = landerX;
        float collisionY = landerY;
        
        DrawRectangleLines(collisionX, collisionY, scaledWidth, scaledHeight, BLUE);
        DrawCircle(landerX, landerY, 2, RED);
        DrawCircle(center.x, center.y, 2, BLUE);
        #endif
    }

    
    // Draw flame if thrusting or rotating
    if (((IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) || (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))) && fuel > 0) {
        // Calculate flame position at the bottom center of the lander
        if (flameTexture.id != 0) {
            // Calculate flame size while preserving aspect ratio
            float flameHeight = height * 0.4f;  // Make flame about 70% of lander height
            float aspectRatio = (float)flameTexture.width / flameTexture.height;
            float flameWidth = flameHeight * aspectRatio;
            
            // Position flame at the bottom of the lander
            Vector2 flamePos = { center.x, center.y + height/2.0f };
            const float flameOffset = 10.0f;
            // Calculate offset position based on angle
            float offsetDistance = -height/2.0f + flameOffset;  // Distance from center to bottom of lander
            flamePos.x = center.x + sinf(angle * DEG2RAD) * offsetDistance;
            flamePos.y = center.y - cosf(angle * DEG2RAD) * offsetDistance;
            
            // Define flame rectangle
            Rectangle flameSource = { 0, 0, (float)flameTexture.width, (float)flameTexture.height };
            Rectangle flameDest = { flamePos.x, flamePos.y, flameWidth, flameHeight };
            Vector2 flameOrigin = { flameWidth/2.0f, 0.0f }; // Origin at top-center of flame
            
            // Draw the flame texture with the same rotation as the lander
            DrawTexturePro(flameTexture, flameSource, flameDest, flameOrigin, angle, WHITE);
        }
    }
}

// Game implementation
Game::Game(int width, int height)
{
    firstTimeGameStart = true;
    musicStarted = false;

#ifdef __EMSCRIPTEN__
    // Check if we're running on a mobile device
    isMobile = EM_ASM_INT({
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    });
#endif

    targetRenderTex = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    SetTextureFilter(targetRenderTex.texture, TEXTURE_FILTER_BILINEAR); // Texture scale filter to use

    font = LoadFontEx("Font/OpenSansRegular.ttf", 64, 0, 0);
    
    // Load background music
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

    // Load background texture
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
    
    // Load terrain texture
    terrainTexture = LoadTexture("data/moon_surface.png");
    if (terrainTexture.id == 0) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load terrain texture: data/moon_surface.png");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded terrain texture");
        #endif
        // Make sure the texture is set to repeat for proper tiling
        SetTextureWrap(terrainTexture, TEXTURE_WRAP_REPEAT);
    }

    // Load landing pad texture
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
}

void Game::InitGame()
{
    isInExitMenu = false;
    paused = false;
    lostWindowFocus = false;
    gameOver = false;
    

    screenScale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);

    // Initialize game state
    lives = 3;
    level = 1;
    thrust = 0.2f;
    rotationSpeed = 3.0f;
    fuelConsumption = 0.2f;
    velocityLimit = 0.8f;  // Initialize velocity limit
    inputDelay = 0.3;
    playingMusic = true;
    
    // Create lander
    lander = new Lander(width, height);
    Randomize(); // Generate terrain
    lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
}

void Game::Reset()
{
    lives = 3;
    level = 1;
    gravity = INITIAL_GRAVITY;  // Reset gravity to initial value
    lander->Reset(width, height);
    Randomize(); // Regenerate terrain
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

    bool running = (firstTimeGameStart == false && paused == false && lostWindowFocus == false && isInExitMenu == false && gameOver == false);

    // Handle music playback
    if (!firstTimeGameStart && !musicStarted && backgroundMusic.stream.buffer != NULL && playingMusic) {
        PlayMusicStream(backgroundMusic);
        musicStarted = true;
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Started playing background music");
        #endif
    }
    
    if (musicStarted && backgroundMusic.stream.buffer != NULL && playingMusic) {
        UpdateMusicStream(backgroundMusic);
        
        // Pause/resume music based on game state
        if (paused || lostWindowFocus || isInExitMenu || gameOver) {
            PauseMusicStream(backgroundMusic);
        } else {
            ResumeMusicStream(backgroundMusic);
        }
    }

    if (running)
    {
        HandleInput();
    }
}

void Game::HandleInput()
{
    float dt = GetFrameTime();

    if(!isMobile) { // desktop and web controls
        // Get input states
        bool thrusting = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
        bool rotatingLeft = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
        bool rotatingRight = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);

        // Update lander
        lander->Update(dt, thrusting, rotatingLeft, rotatingRight);

        if(IsKeyPressed(KEY_M)) {
          playingMusic = !playingMusic;
          if(playingMusic) {
            PlayMusicStream(backgroundMusic);
          } else {
            PauseMusicStream(backgroundMusic);
          }
        }
    } 
    else // mobile controls
    {
        if(IsGestureDetected(GESTURE_DRAG) || IsGestureDetected(GESTURE_HOLD)) {
            // Get touch position in screen coordinates
            Vector2 touchPosition = GetTouchPosition(0);
            
            // Convert screen coordinates to game coordinates
            float gameX = (touchPosition.x - (GetScreenWidth() - (gameScreenWidth * screenScale)) * 0.5f) / screenScale;
            float gameY = (touchPosition.y - (GetScreenHeight() - (gameScreenHeight * screenScale)) * 0.5f) / screenScale;
            
            // Calculate thrust and rotation based on touch position
            Vector2 landerCenter = { lander->GetX() + lander->GetWidth()/2.0f, lander->GetY() + lander->GetHeight()/2.0f };
            Vector2 direction = { gameX - landerCenter.x, gameY - landerCenter.y };
            
            // Normalize the direction vector
            float length = sqrtf(direction.x * direction.x + direction.y * direction.y);
            if(length > 0) {
                direction.x /= length;
                direction.y /= length;
                
                // Calculate angle for rotation
                float targetAngle = atan2f(direction.x, -direction.y) * RAD2DEG;
                float currentAngle = lander->GetAngle();
                float angleDiff = fmodf(targetAngle - currentAngle + 180.0f, 360.0f) - 180.0f;
                
                bool rotatingLeft = angleDiff > 5.0f;
                bool rotatingRight = angleDiff < -5.0f;
                bool thrusting = length > 50.0f; // Only thrust if touch is far enough from lander
                
                lander->Update(dt, thrusting, rotatingLeft, rotatingRight);
            }
        }
    }

    // Handle landing/crash
    if (lander->IsLanded() || lander->IsCrashed()) {
        if (lander->IsCrashed()) {
            if (lives <= 1) {  // If this crash would end the game
                gameOver = true;
            } else if (IsKeyPressed(KEY_ENTER)) {
                lives--;
                lander->Reset(width, height);
                Randomize(); // Generate new terrain
                lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
            }
        } else if (GetTime() - lander->GetLandingTime() > inputDelay && IsKeyPressed(KEY_ENTER)) {
            Game::gravity += gravityIncrease;
            if(Game::gravity > MAX_GRAVITY) {
                Game::gravity = MAX_GRAVITY;
                Game::maxGravityReached = true;
            }
            if(Game::maxGravityReached) {
              fuelConsumption += fuelConsumptionIncrease;
            }
            level++;
            lander->Reset(width, height);
            Randomize(); // Generate new terrain
            lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
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
    if (exitWindowRequested == false && lostWindowFocus == false && gameOver == false && (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)))
#endif
    {
        paused = !paused;
    }

    // Handle game over restart
    if (gameOver && IsKeyPressed(KEY_ENTER)) {
        Reset();
    }
}

void Game::Draw()
{
    // render everything to a texture
    BeginTextureMode(targetRenderTex);
    ClearBackground(BLACK);
    DrawTexturePro(backgroundTexture, (Rectangle){0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height}, (Rectangle){0, 0, (float)gameScreenWidth, (float)gameScreenHeight}, (Vector2){0, 0}, 0.0f, WHITE);

    DrawTerrain();

    // Draw landing pad with a metallic look
    float landingPadX = lander->GetLandingPadX();
    float padY = gameScreenHeight - 50;
    float padWidth = 100;
    float padHeight = 5;
    
    // Draw landing pad using texture
    if (landingPadTexture.id != 0) {
        // Calculate landing pad dimensions, maintaining aspect ratio
        float aspectRatio = (float)landingPadTexture.width / landingPadTexture.height;
        float drawHeight = 100.0f; // Adjust this value for desired pad height
        float drawWidth = drawHeight * aspectRatio;
        

        Rectangle source = { 0, 0, (float)landingPadTexture.width, (float)landingPadTexture.height };
        Rectangle dest = { landingPadX - drawWidth/2, padY - drawHeight/2 + 5.0f, drawWidth, drawHeight };
        
        // Draw the landing pad texture
        DrawTexturePro(landingPadTexture, source, dest, (Vector2){0, 0}, 0.0f, WHITE);
        
        #ifdef DEBUG_COLLISION
        // Draw a line representing landing pad collision surface for debugging
        DrawLine(landingPadX - 50, padY, landingPadX + 50, padY, RED);
        #endif
    } else {
        // Fallback to drawing rectangles if texture failed to load
        // Draw main landing pad with a gradient
        Color padColorLeft = (Color){ 150, 150, 150, 255 };   // Light gray
        Color padColorRight = (Color){ 200, 200, 200, 255 };  // Lighter gray
        DrawRectangleGradientH(landingPadX - padWidth/2, padY, padWidth, padHeight, padColorLeft, padColorRight);
        
        // Draw landing pad borders for better visibility
        DrawRectangleLines(landingPadX - padWidth/2, padY, padWidth, padHeight, GREEN);
        
        // Draw landing pad leg supports
        DrawLine(landingPadX - padWidth/2, padY + padHeight, landingPadX - padWidth/2 + 10, padY + 15, GREEN);
        DrawLine(landingPadX + padWidth/2, padY + padHeight, landingPadX + padWidth/2 - 10, padY + 15, GREEN);
    }

    // Draw lander
    lander->Draw();

    DrawUI();

    EndTextureMode();

    // render the scaled frame texture to the screen
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
    
    // Improved approach using multiple smaller quads for each segment
    // This gives better texture mapping and less blockiness
    for (int i = 0; i < TERRAIN_POINTS - 1; ++i) {
        float segmentWidth = terrainPoints[i+1].x - terrainPoints[i].x;
        if (segmentWidth < 1.0f) continue; // Skip very small segments
        
        const int subdivisions = 20;
        float subWidth = segmentWidth / subdivisions;
        
        for (int j = 0; j < subdivisions; j++) {
            float t1 = (float)j / subdivisions;
            float t2 = (float)(j + 1) / subdivisions;
            
            // Interpolate positions between the terrain points
            float x1 = terrainPoints[i].x + t1 * segmentWidth;
            float x2 = terrainPoints[i].x + t2 * segmentWidth;
            float y1 = terrainPoints[i].y + t1 * (terrainPoints[i+1].y - terrainPoints[i].y);
            float y2 = terrainPoints[i].y + t2 * (terrainPoints[i+1].y - terrainPoints[i].y);
            
            // Calculate texture coordinates for better detail visibility
            // Simple approach: use most of the texture for each segment with minimal tiling
            
            // For more visible crater details and less repetition:
            // 1. Use most of the texture (90%)
            // 2. Apply minimal movement to avoid obvious pattern repeats
            float textureVisiblePortion = 0.99f;  // Show 90% of the texture for more detail
            
            // Very slight movement across the terrain to maintain continuity but avoid obvious repetition
            float offsetScale = 0.0001f;  // Very small value to minimize tiling
            float globalOffsetX = offsetScale * (i * 10 + j);  // Based on segment position
            float globalOffsetY = offsetScale * i * 5;  // Slight vertical variation
            
            // Calculate source rectangle for texture mapping
            Rectangle source;
            
            // Set simple source rectangle - use most of the texture with minimal offsets
            source.width = (float)terrainTexture.width * textureVisiblePortion;
            source.height = (float)terrainTexture.height * textureVisiblePortion;
            
            // Apply very minimal movement through the texture
            source.x = fmodf(globalOffsetX, (float)(terrainTexture.width - source.width));
            source.y = fmodf(globalOffsetY, (float)(terrainTexture.height - source.height));
            
            // Destination rectangle for the quad
            float yTop = std::min(y1, y2);
            float height = gameScreenHeight - yTop;
            Rectangle dest = { x1, yTop, x2 - x1, height };
            
            // Draw the quad with the moon texture
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

    DrawTextEx(font, "Moonlander", {400, 10}, 24, 2, WHITE);

    if (exitWindowRequested)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawTextEx(font, "Are you sure you want to exit? [Y/N]", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
    }
    else if (firstTimeGameStart)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 80}, 0.76f, 20, BLACK);
        if (isMobile) {
            DrawTextEx(font, "Tap to play", {screenX + (gameScreenWidth / 2 - 60), screenY + gameScreenHeight / 2 + 10}, 20, 2, yellow);
        } else {
#ifndef EMSCRIPTEN_BUILD            
            DrawTextEx(font, "Press Enter to play", {screenX + (gameScreenWidth / 2 - 100), screenY + gameScreenHeight / 2 - 10}, 20, 2, yellow);
            DrawTextEx(font, "Alt+Enter: toggle fullscreen", {screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 + 30}, 20, 2, yellow);
#else
            DrawTextEx(font, "Press Enter to play", {screenX + (gameScreenWidth / 2 - 100), screenY + gameScreenHeight / 2 + 10}, 20, 2, yellow);
#endif
        }
    }
    else if (paused)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
#ifndef EMSCRIPTEN_BUILD
        DrawTextEx(font, "Game paused, press P to continue", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
#else
        if (isMobile) {
            DrawTextEx(font, "Game paused, tap to continue", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
        } else {
            DrawTextEx(font, "Game paused, press P or ESC to continue", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
        }
#endif
    }
    else if (lostWindowFocus)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawTextEx(font, "Game paused, focus window to continue", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
    }
    else if (gameOver)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        if (isMobile) {
            DrawTextEx(font, "Game over, tap to play again", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
        } else {
            DrawTextEx(font, "Game over, press Enter to play again", {screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2}, 20, 2, yellow);
        }
    }
    else if (lander->IsLanded())
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawTextEx(font, "Landing Successful!", {screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 - 15}, 20, 2, GREEN);
        DrawTextEx(font, "Press Enter for next level", {screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 + 15}, 20, 2, WHITE);
    }
    else if (lander->IsCrashed() && lives > 0)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawTextEx(font, "Crashed! You lost a life!", {screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 - 15}, 20, 2, RED);
        DrawTextEx(font, "Press Enter to try again", {screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 + 15}, 20, 2, WHITE);
    }

    // Calculate right-aligned positions with padding
    int rightMargin = 20;
    int lineHeight = 30;   // Height between lines
    int startY = 10;       // Starting Y position

    // Level
    const char* levelText = TextFormat("Level: %d", level);
    Vector2 levelSize = MeasureTextEx(font, levelText, 20, 2);
    DrawTextEx(font, levelText, { (float)(gameScreenWidth - levelSize.x - rightMargin), (float)startY }, 20, 2, WHITE);
    
    // Lives
    const char* livesText = TextFormat("Lives: %d", lives);
    Vector2 livesSize = MeasureTextEx(font, livesText, 20, 2);
    DrawTextEx(font, livesText, { (float)(gameScreenWidth - livesSize.x - rightMargin), (float)(startY + lineHeight) }, 20, 2, WHITE);
    
    // Fuel
    const char* fuelText = TextFormat("Fuel: %.1f", lander->GetFuel());
    Vector2 fuelSize = MeasureTextEx(font, fuelText, 20, 2);
    DrawTextEx(font, fuelText, { (float)(gameScreenWidth - fuelSize.x - rightMargin), (float)(startY + lineHeight * 2) }, 20, 2, WHITE);
    
    // Velocity
    const char* velocityText = TextFormat("Velocity X: %.1f Y: %.1f", lander->GetVelocityX(), lander->GetVelocityY());
    Vector2 velocitySize = MeasureTextEx(font, velocityText, 20, 2);
    DrawTextEx(font, velocityText, { (float)(gameScreenWidth - velocitySize.x - rightMargin), (float)(startY + lineHeight * 3) }, 20, 2, WHITE);
    
    // Angle
    const char* angleText = TextFormat("Angle: %.1f", lander->GetAngle());
    Vector2 angleSize = MeasureTextEx(font, angleText, 20, 2);
    DrawTextEx(font, angleText, { (float)(gameScreenWidth - angleSize.x - rightMargin), (float)(startY + lineHeight * 4) }, 20, 2, WHITE);

    // Gravity
    const char* gravityText = TextFormat("Gravity: %.3f", Game::gravity);
    Vector2 gravitySize = MeasureTextEx(font, gravityText, 20, 2);
    DrawTextEx(font, gravityText, { (float)(gameScreenWidth - gravitySize.x - rightMargin), (float)(startY + lineHeight * 5) }, 20, 2, WHITE);
}

std::string Game::FormatWithLeadingZeroes(int number, int width)
{
    std::string numberText = std::to_string(number);
    int leadingZeros = width - numberText.length();
    numberText = std::string(leadingZeros, '0') + numberText;
    return numberText;
}

void Game::Randomize()
{
    // Generate random terrain
    float segmentWidth = (float)gameScreenWidth / (TERRAIN_POINTS - 1);
    float minHeight = gameScreenHeight - minTerrainHeight;
    float maxHeight = gameScreenHeight - maxTerrainHeight;
    float landingPadCenter = lander->GetLandingPadX();
    float landingPadHalfWidth = 50.0f;
    float landingPadHeight = gameScreenHeight - 50;
    
    // Step 1: Create initial terrain points (flat landing pad area and random terrain elsewhere)
    for (int i = 0; i < TERRAIN_POINTS; i++) {
        float x = i * segmentWidth;
        float y;
        
        // Determine if this point is in the landing pad area
        if (x >= landingPadCenter - landingPadHalfWidth - segmentWidth && 
            x <= landingPadCenter + landingPadHalfWidth + segmentWidth) {
            // Make it flat for the landing pad (with a slight taper at edges)
            if (x < landingPadCenter - landingPadHalfWidth) {
                // Left edge taper
                float t = (landingPadCenter - landingPadHalfWidth - x) / segmentWidth;
                y = landingPadHeight - (t * t * 10);
            } else if (x > landingPadCenter + landingPadHalfWidth) {
                // Right edge taper
                float t = (x - (landingPadCenter + landingPadHalfWidth)) / segmentWidth;
                y = landingPadHeight - (t * t * 10);
            } else {
                // Flat landing pad
                y = landingPadHeight;
            }
        } else {
            // Random height for normal terrain
            y = GetRandomValue(minHeight, maxHeight);
        }
        
        terrainPoints[i] = (Vector2){ x, y };
    }
    
    // Step 2: Apply smoothing to non-landing pad areas
    // Create a temporary copy of terrain points for smoothing
    Vector2 smoothedPoints[TERRAIN_POINTS];
    for (int i = 0; i < TERRAIN_POINTS; i++) {
        smoothedPoints[i] = terrainPoints[i];
    }
    
    // Apply smoothing (moving average) to make terrain less pointy
    // Skip landing pad area to preserve its flatness
    for (int i = 1; i < TERRAIN_POINTS - 1; i++) {
        float x = i * segmentWidth;
        
        // Skip smoothing for the landing pad area to preserve its flatness
        if (x >= landingPadCenter - landingPadHalfWidth - segmentWidth && 
            x <= landingPadCenter + landingPadHalfWidth + segmentWidth) {
            continue;
        }
        
        // Apply a simple moving average to smooth the terrain
        // Use a 3-point window (current point and its neighbors)
        smoothedPoints[i].y = (terrainPoints[i-1].y + terrainPoints[i].y + terrainPoints[i+1].y) / 3.0f;
    }
    
    // Apply a second pass of smoothing for even smoother terrain
    for (int i = 1; i < TERRAIN_POINTS - 1; i++) {
        float x = i * segmentWidth;
        
        // Skip smoothing for the landing pad area
        if (x >= landingPadCenter - landingPadHalfWidth - segmentWidth && 
            x <= landingPadCenter + landingPadHalfWidth + segmentWidth) {
            continue;
        }
        
        // Apply smoothing again
        terrainPoints[i].y = (smoothedPoints[i-1].y + smoothedPoints[i].y + smoothedPoints[i+1].y) / 3.0f;
    }
}

void Lander::Cleanup() {
    UnloadSound(thrustSound);
    UnloadSound(landSound);
    UnloadSound(crashSound);
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
    if (flameTexture.id != 0) {
        UnloadTexture(flameTexture);
    }
}