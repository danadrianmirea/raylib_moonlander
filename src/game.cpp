#include <vector>
#include <utility>
#include <string>
#include <cmath>  // For sqrtf
#include <random>

#include "raylib.h"
#include "globals.h"
#include "game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define INITIAL_GRAVITY 0.6f
#define INITIAL_VELOCITY_LIMIT 0.8f
#define MUSIC_VOLUME 0.2f

float Lander::thrust = 0.02f;
float Lander::rotationSpeed = 1.0f;
float Lander::fuelConsumption = 0.1f;
float Game::gravity = INITIAL_GRAVITY;
float Game::velocityLimit = INITIAL_VELOCITY_LIMIT;  // Non-const static member
bool Game::isMobile = false;

// Lander implementation
Lander::Lander(int screenWidth, int screenHeight) {
    // Load thrust sound
    thrustSound = LoadSound("data/thrust.mp3");
    if (thrustSound.stream.buffer == NULL) {
        TraceLog(LOG_ERROR, "Failed to load thrust sound: data/thrust.mp3");
    } else {
        TraceLog(LOG_INFO, "Successfully loaded thrust sound");
        SetSoundVolume(thrustSound, 1.0f);  // Set volume to 100%
    }

    landSound = LoadSound("data/land.mp3");
    if (landSound.stream.buffer == NULL) {
        TraceLog(LOG_ERROR, "Failed to load land sound: data/land.mp3");
    } else {
        TraceLog(LOG_INFO, "Successfully loaded land sound");
        SetSoundVolume(landSound, 1.0f);  // Set volume to 100%
    }

    crashSound = LoadSound("data/crash.mp3");
    if (crashSound.stream.buffer == NULL) {
        TraceLog(LOG_ERROR, "Failed to load crash sound: data/crash.mp3");
    } else {
        TraceLog(LOG_INFO, "Successfully loaded crash sound");
        SetSoundVolume(crashSound, 1.0f);  // Set volume to 100%
    }

    wasThrusting = false;
    wasRotating = false;
    Reset(screenWidth, screenHeight);
}

void Lander::Reset(int screenWidth, int screenHeight) {
    x = screenWidth / 2.0f;
    y = 50.0f;  // Start higher up
    velocityX = 0.0f;
    velocityY = 0.0f;
    angle = 0.0f;
    fuel = 100.0f;
    landed = false;
    crashed = false;
    width = 20.0f;
    height = 30.0f;
    
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
                TraceLog(LOG_INFO, "Started playing thrust sound");
            }
            wasThrusting = thrusting;
            wasRotating = isRotating;
        } else if ((wasThrusting || wasRotating) && thrustSound.stream.buffer != NULL) {
            StopSound(thrustSound);
            wasThrusting = false;
            wasRotating = false;
            TraceLog(LOG_INFO, "Stopped thrust sound");
        }

        // Update position
        x += velocityX;
        y += velocityY;

        // Check for landing or crash
        if (y + height >= gameScreenHeight - 50.0f) {
            if (landingPadX - 50.0f <= x + width/2.0f && x + width/2.0f <= landingPadX + 50.0f &&
                fabsf(velocityX) < Game::velocityLimit && fabsf(velocityY) < Game::velocityLimit) {
                float normalizedAngle = fmodf(angle + 180.0f, 360.0f) - 180.0f;
                if (fabsf(normalizedAngle) < 15.0f) {
                    landed = true;
                    landingTime = GetTime();
                    PlaySound(landSound);
                    TraceLog(LOG_INFO, "Land sound played");
                } else {
                    crashed = true;
                    PlaySound(crashSound);
                    TraceLog(LOG_INFO, "Crash sound played");
                }
            } else {
                crashed = true;
                PlaySound(crashSound);
                TraceLog(LOG_INFO, "Crash sound played");
            }
        }

        // Screen boundaries
        x = fmaxf(0.0f, fminf(gameScreenWidth - width, x));
        if (y < 0.0f) {
            y = 0.0f;
            velocityY = 0.0f;
        }
    }
}

void Lander::Draw() {
    // Draw lander
    Vector2 center = { x + width/2.0f, y + height/2.0f };
    Vector2 points[3] = {
        { x + width/2.0f, y },
        { x + width, y + height },
        { x, y + height }
    };

    // Rotate points around center
    for (int i = 0; i < 3; i++) {
        float dx = points[i].x - center.x;
        float dy = points[i].y - center.y;
        float cosA = cosf(angle * DEG2RAD);
        float sinA = sinf(angle * DEG2RAD);
        points[i].x = center.x + dx * cosA - dy * sinA;
        points[i].y = center.y + dx * sinA + dy * cosA;
    }

    // Draw the lander with a thicker line
    DrawTriangleLines(points[0], points[1], points[2], WHITE);

    // Draw flame if thrusting
    if ((IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) && fuel > 0) {
        Vector2 flamePoints[3] = {
            { x + width/2.0f, y + height },
            { x + width/2.0f + 5.0f, y + height + 15.0f },
            { x + width/2.0f - 5.0f, y + height + 15.0f }
        };

        // Rotate flame points around center
        for (int i = 0; i < 3; i++) {
            float dx = flamePoints[i].x - center.x;
            float dy = flamePoints[i].y - center.y;
            float cosA = cosf(angle * DEG2RAD);
            float sinA = sinf(angle * DEG2RAD);
            flamePoints[i].x = center.x + dx * cosA - dy * sinA;
            flamePoints[i].y = center.y + dx * sinA + dy * cosA;
        }

        DrawTriangleLines(flamePoints[0], flamePoints[1], flamePoints[2], RED);
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

    font = LoadFontEx("Font/monogram.ttf", 64, 0, 0);
    
    // Load background music
    backgroundMusic = LoadMusicStream("data/music.mp3");
    if (backgroundMusic.stream.buffer == NULL) {
        TraceLog(LOG_ERROR, "Failed to load music file: data/music.mp3");
    } else {
        TraceLog(LOG_INFO, "Successfully loaded music file");
        SetMusicVolume(backgroundMusic, MUSIC_VOLUME);
    }

    this->width = width;
    this->height = height;
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
    
    // Create lander
    lander = new Lander(width, height);
}

void Game::Reset()
{
    lives = 3;
    level = 1;
    gravity = INITIAL_GRAVITY;  // Reset gravity to initial value
    lander->Reset(width, height);
    gameOver = false;
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
    if (!firstTimeGameStart && !musicStarted && backgroundMusic.stream.buffer != NULL) {
        PlayMusicStream(backgroundMusic);
        musicStarted = true;
        TraceLog(LOG_INFO, "Started playing background music");
    }
    
    if (musicStarted && backgroundMusic.stream.buffer != NULL) {
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
            }
        } else if (GetTime() - lander->GetLandingTime() > inputDelay && IsKeyPressed(KEY_ENTER)) {
            Game::gravity += gravityIncrease;
            level++;
            lander->Reset(width, height);
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

    // Draw terrain
    DrawRectangle(0, gameScreenHeight - 50, gameScreenWidth, 50, GRAY);
    DrawRectangle(lander->GetLandingPadX() - 50, gameScreenHeight - 50, 100, 5, GREEN);

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

void Game::DrawUI()
{
    float screenX = 0.0f;
    float screenY = 0.0f;

    DrawTextEx(font, "Moonlander", {300, 10}, 34, 2, yellow);

    if (exitWindowRequested)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawText("Are you sure you want to exit? [Y/N]", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
    }
    else if (firstTimeGameStart)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 80}, 0.76f, 20, BLACK);
        if (isMobile) {
            DrawText("Tap to play", screenX + (gameScreenWidth / 2 - 60), screenY + gameScreenHeight / 2 + 10, 20, yellow);
        } else {
#ifndef EMSCRIPTEN_BUILD            
            DrawText("Press Enter to play", screenX + (gameScreenWidth / 2 - 100), screenY + gameScreenHeight / 2 - 10, 20, yellow);
            DrawText("Alt+Enter: toggle fullscreen", screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 + 30, 20, yellow);
#else
            DrawText("Press Enter to play", screenX + (gameScreenWidth / 2 - 100), screenY + gameScreenHeight / 2 + 10, 20, yellow);
#endif
        }
    }
    else if (paused)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
#ifndef EMSCRIPTEN_BUILD
        DrawText("Game paused, press P to continue", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
#else
        if (isMobile) {
            DrawText("Game paused, tap to continue", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
        } else {
            DrawText("Game paused, press P or ESC to continue", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
        }
#endif
    }
    else if (lostWindowFocus)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawText("Game paused, focus window to continue", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
    }
    else if (gameOver)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        if (isMobile) {
            DrawText("Game over, tap to play again", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
        } else {
            DrawText("Game over, press Enter to play again", screenX + (gameScreenWidth / 2 - 200), screenY + gameScreenHeight / 2, 20, yellow);
        }
    }
    else if (lander->IsLanded())
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawText("Landing Successful!", screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 - 15, 20, GREEN);
        DrawText("Press Enter for next level", screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 + 15, 20, WHITE);
    }
    else if (lander->IsCrashed() && lives > 0)
    {
        DrawRectangleRounded({screenX + (float)(gameScreenWidth / 2 - 250), screenY + (float)(gameScreenHeight / 2 - 20), 500, 60}, 0.76f, 20, BLACK);
        DrawText("Crashed! You lost a life!", screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 - 15, 20, RED);
        DrawText("Press Enter to try again", screenX + (gameScreenWidth / 2 - 120), screenY + gameScreenHeight / 2 + 15, 20, WHITE);
    }

    // Calculate right-aligned positions with padding
    int rightMargin = 70;  // 20px margin + 50px padding
    int lineHeight = 30;   // Height between lines
    int startY = 10;       // Starting Y position

    // Level
    const char* levelText = TextFormat("Level: %d", level);
    int levelWidth = MeasureText(levelText, 20);
    DrawTextEx(font, levelText, { (float)(gameScreenWidth - levelWidth - rightMargin), (float)startY }, 20, 2, WHITE);
    
    // Lives
    const char* livesText = TextFormat("Lives: %d", lives);
    int livesWidth = MeasureText(livesText, 20);
    DrawTextEx(font, livesText, { (float)(gameScreenWidth - livesWidth - rightMargin), (float)(startY + lineHeight) }, 20, 2, WHITE);
    
    // Fuel
    const char* fuelText = TextFormat("Fuel: %.1f", lander->GetFuel());
    int fuelWidth = MeasureText(fuelText, 20);
    DrawTextEx(font, fuelText, { (float)(gameScreenWidth - fuelWidth - rightMargin), (float)(startY + lineHeight * 2) }, 20, 2, WHITE);
    
    // Velocity
    const char* velocityText = TextFormat("Velocity X: %.1f Y: %.1f", lander->GetVelocityX(), lander->GetVelocityY());
    int velocityWidth = MeasureText(velocityText, 20);
    DrawTextEx(font, velocityText, { (float)(gameScreenWidth - velocityWidth - rightMargin), (float)(startY + lineHeight * 3) }, 20, 2, WHITE);
    
    // Angle
    const char* angleText = TextFormat("Angle: %.1f°", lander->GetAngle());
    int angleWidth = MeasureText(angleText, 20);
    DrawTextEx(font, angleText, { (float)(gameScreenWidth - angleWidth - rightMargin), (float)(startY + lineHeight * 4) }, 20, 2, WHITE);

    // Gravity
    const char* gravityText = TextFormat("Gravity: %.3f", Game::gravity);
    int gravityWidth = MeasureText(gravityText, 20);
    DrawTextEx(font, gravityText, { (float)(gameScreenWidth - gravityWidth - rightMargin), (float)(startY + lineHeight * 5) }, 20, 2, WHITE);
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
}

void Lander::Cleanup() {
    UnloadSound(thrustSound);
}