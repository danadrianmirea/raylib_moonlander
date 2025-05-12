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
//#define DEBUG
#define INITIAL_GRAVITY 1.0f
#define MAX_GRAVITY 2.0f
#define INITIAL_VELOCITY_LIMIT 0.8f
#define MUSIC_VOLUME 0.2f
//#define DEBUG_COLLISION

float Lander::thrust = 0.02f;
float Lander::rotationSpeed = 1.0f;
float Lander::fuelConsumption = 0.05f;
float Game::gravity = INITIAL_GRAVITY;
float Game::velocityLimit = INITIAL_VELOCITY_LIMIT;  
bool Game::isMobile = false;
bool Game::maxGravityReached = false;

Lander::Lander(int screenWidth, int screenHeight) {
    
    thrustSound = LoadSound("data/thrust.mp3");
    if (thrustSound.stream.buffer == NULL) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load thrust sound: data/thrust.mp3");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded thrust sound");
        #endif
        SetSoundVolume(thrustSound, 1.0f);  
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
        SetSoundVolume(landSound, 1.0f);  
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
        SetSoundVolume(crashSound, 1.0f);  
    }

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
    landerY = 50.0f;  
    velocityX = 0.0f;
    velocityY = 0.0f;
    angle = 0.0f;
    fuel = 100.0f;
    landed = false;
    crashed = false;
    crashPosX = 0.0f;
    crashPosY = 0.0f;

    height = 60.0f;  

    if (texture.id != 0) {
        width = height * ((float)texture.width / texture.height);
    } else {
        width = 20.0f; 
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(100.0f, screenWidth - 100.0f);
    landingPadX = dis(gen);
    landingTime = 0.0;

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
        
        velocityY += Game::gravity * dt;

        bool isRotating = (rotatingLeft || rotatingRight) && fuel > 0;
        bool shouldPlayThrustSound = (thrusting || isRotating) && fuel > 0;

        if (thrusting && fuel > 0) {
            velocityX += sinf(angle * DEG2RAD) * thrust;
            velocityY -= cosf(angle * DEG2RAD) * thrust;
            fuel = fmaxf(0.0f, fuel - fuelConsumption);
        }

        if (isRotating) {
            if (rotatingLeft) {
                angle = fmodf(angle + rotationSpeed, 360.0f);
            }
            if (rotatingRight) {
                angle = fmodf(angle - rotationSpeed, 360.0f);
            }
            
            fuel = fmaxf(0.0f, fuel - (fuelConsumption * 0.5f));
        }

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

        landerX += velocityX;
        landerY += velocityY;

        landerX = fmaxf(0.0f, fminf(gameScreenWidth - width, landerX));
        if (landerY < 0.0f) {
            landerY = 0.0f;
            velocityY = 0.0f;
        }

        float scaledWidth = width * collisionScale;
        float scaledHeight = height * collisionScale;
        
        float collisionX = landerX; 
        float collisionY = landerY; 
        
        Rectangle collisionRect = { collisionX, collisionY, scaledWidth, scaledHeight };
        float collisionBottom = collisionY + scaledHeight;

        float centerX = collisionX + scaledWidth/2.0f;
        float centerY = collisionY + scaledHeight/2.0f;

        for (int i = 0; i < terrainPoints - 1; i++) {
            
            if (centerX >= terrain[i].x && centerX <= terrain[i+1].x) {
                
                float t = (centerX - terrain[i].x) / (terrain[i+1].x - terrain[i].x);
                float terrainHeight = terrain[i].y * (1 - t) + terrain[i+1].y * t;

                if (collisionBottom >= terrainHeight) {
                    
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

                            crashPosX = centerX;
                            crashPosY = centerY;
                        }
                    } else {
                        crashed = true;
                        PlaySound(crashSound);
                        #ifdef DEBUG
                        TraceLog(LOG_INFO, "Crash sound played - hit terrain");
                        #endif

                        crashPosX = centerX;
                        crashPosY = centerY;
                    }

                    landerY = terrainHeight - scaledHeight - (height - scaledHeight) / 2.0f;
                    break;
                }
            }
        }
    }
}

void Lander::Draw() {
    
    if (crashed) return;

    Vector2 center = { landerX + width/2.0f, landerY + height/2.0f };
    
    if (texture.id != 0) {
        
        Rectangle source = { 0, 0, (float)texture.width, (float)texture.height };
        Rectangle dest = { landerX + width/2.0f, landerY + height/2.0f, width, height };
        Vector2 origin = { width/2.0f, height/2.0f }; 

        DrawTexturePro(texture, source, dest, origin, angle, WHITE);
    }

    if (!crashed && ((IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) || (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))) && fuel > 0) {
        
        if (flameTexture.id != 0) {
            
            float flameHeight = height * 0.4f;  
            float aspectRatio = (float)flameTexture.width / flameTexture.height;
            float flameWidth = flameHeight * aspectRatio;

            Vector2 flamePos = { center.x, center.y + height/2.0f };
            const float flameOffset = 10.0f;
            
            float offsetDistance = -height/2.0f + flameOffset;  
            flamePos.x = center.x + sinf(angle * DEG2RAD) * offsetDistance;
            flamePos.y = center.y - cosf(angle * DEG2RAD) * offsetDistance;

            Rectangle flameSource = { 0, 0, (float)flameTexture.width, (float)flameTexture.height };
            Rectangle flameDest = { flamePos.x, flamePos.y, flameWidth, flameHeight };
            Vector2 flameOrigin = { flameWidth/2.0f, 0.0f }; 

            DrawTexturePro(flameTexture, flameSource, flameDest, flameOrigin, angle, WHITE);
        }
    }
}

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
    explosionCompleted = false;

    screenScale = MIN((float)GetScreenWidth() / gameScreenWidth, (float)GetScreenHeight() / gameScreenHeight);

    lives = 3;
    level = 1;
    thrust = 0.2f;
    rotationSpeed = 3.0f;
    fuelConsumption = 0.2f;
    velocityLimit = 0.8f;  
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
    gravity = INITIAL_GRAVITY;  
    explosionCompleted = false;
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
        HandleInput();
    }
}

void Game::HandleInput()
{
    float dt = GetFrameTime();

    if(!isMobile) { 
        
        bool thrusting = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
        bool rotatingLeft = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
        bool rotatingRight = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);

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
    else 
    {
        if(IsGestureDetected(GESTURE_DRAG) || IsGestureDetected(GESTURE_HOLD)) {
            
            Vector2 touchPosition = GetTouchPosition(0);

            float gameX = (touchPosition.x - (GetScreenWidth() - (gameScreenWidth * screenScale)) * 0.5f) / screenScale;
            float gameY = (touchPosition.y - (GetScreenHeight() - (gameScreenHeight * screenScale)) * 0.5f) / screenScale;

            Vector2 landerCenter = { lander->GetX() + lander->GetWidth()/2.0f, lander->GetY() + lander->GetHeight()/2.0f };
            Vector2 direction = { gameX - landerCenter.x, gameY - landerCenter.y };

            float length = sqrtf(direction.x * direction.x + direction.y * direction.y);
            if(length > 0) {
                direction.x /= length;
                direction.y /= length;

                float targetAngle = atan2f(direction.x, -direction.y) * RAD2DEG;
                float currentAngle = lander->GetAngle();
                float angleDiff = fmodf(targetAngle - currentAngle + 180.0f, 360.0f) - 180.0f;
                
                bool rotatingLeft = angleDiff > 5.0f;
                
                bool rotatingRight = angleDiff < -5.0f;
                bool thrusting = length > 50.0f; 
                
                lander->Update(dt, thrusting, rotatingLeft, rotatingRight);
            }
        }
    }

    if (lander->IsLanded() || lander->IsCrashed()) {
        if (lander->IsCrashed()) {
            
            if (!explosionActive && !explosionCompleted) {
                StartExplosion(lander->GetCrashPosX(), lander->GetCrashPosY());
                explosionCompleted = true; 
            }
            
            if (lives <= 1) {  
                gameOver = true;
            } else if (IsKeyPressed(KEY_ENTER)) {
                lives--;
                lander->Reset(width, height);
                Randomize(); 
                lander->SetTerrainReference(terrainPoints, TERRAIN_POINTS);
                explosionCompleted = false; 
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
            Randomize(); 
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

    if (gameOver && IsKeyPressed(KEY_ENTER)) {
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
        float subWidth = segmentWidth / subdivisions;
        
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

    int rightMargin = 20;
    int lineHeight = 30;   
    int startY = 10;       

    const char* levelText = TextFormat("Level: %d", level);
    Vector2 levelSize = MeasureTextEx(font, levelText, 20, 2);
    DrawTextEx(font, levelText, { (float)(gameScreenWidth - levelSize.x - rightMargin), (float)startY }, 20, 2, WHITE);

    const char* livesText = TextFormat("Lives: %d", lives);
    Vector2 livesSize = MeasureTextEx(font, livesText, 20, 2);
    DrawTextEx(font, livesText, { (float)(gameScreenWidth - livesSize.x - rightMargin), (float)(startY + lineHeight) }, 20, 2, WHITE);

    const char* fuelText = TextFormat("Fuel: %.1f", lander->GetFuel());
    Vector2 fuelSize = MeasureTextEx(font, fuelText, 20, 2);
    DrawTextEx(font, fuelText, { (float)(gameScreenWidth - fuelSize.x - rightMargin), (float)(startY + lineHeight * 2) }, 20, 2, WHITE);

    const char* fuelConsumptionText = TextFormat("Fuel Use: %.3f", Lander::fuelConsumption);
    Vector2 fuelConsumptionSize = MeasureTextEx(font, fuelConsumptionText, 20, 2);
    DrawTextEx(font, fuelConsumptionText, { (float)(gameScreenWidth - fuelConsumptionSize.x - rightMargin), (float)(startY + lineHeight * 3) }, 20, 2, WHITE);

    const char* velocityText = TextFormat("Velocity X: %.1f Y: %.1f", lander->GetVelocityX(), lander->GetVelocityY());
    Vector2 velocitySize = MeasureTextEx(font, velocityText, 20, 2);
    DrawTextEx(font, velocityText, { (float)(gameScreenWidth - velocitySize.x - rightMargin), (float)(startY + lineHeight * 4) }, 20, 2, WHITE);

    const char* angleText = TextFormat("Angle: %.1f", lander->GetAngle());
    Vector2 angleSize = MeasureTextEx(font, angleText, 20, 2);
    DrawTextEx(font, angleText, { (float)(gameScreenWidth - angleSize.x - rightMargin), (float)(startY + lineHeight * 5) }, 20, 2, WHITE);

    const char* gravityText = TextFormat("Gravity: %.3f", Game::gravity);
    Vector2 gravitySize = MeasureTextEx(font, gravityText, 20, 2);
    DrawTextEx(font, gravityText, { (float)(gameScreenWidth - gravitySize.x - rightMargin), (float)(startY + lineHeight * 6) }, 20, 2, WHITE);

    const char* musicText = TextFormat("Press M to toggle music %s", playingMusic ? "(ON)" : "(OFF)");
    Vector2 musicTextSize = MeasureTextEx(font, musicText, 24, 1);
    DrawTextEx(font, musicText, { (float)(gameScreenWidth / 2 - musicTextSize.x / 2), (float)(gameScreenHeight - 30) }, 24, 1, WHITE);
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