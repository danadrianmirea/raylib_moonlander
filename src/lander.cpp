#include <cmath>
#include <random>
#include "raylib.h"
#include "rlgl.h"
#include "globals.h"
#include "lander.h"
#include "game.h"
#include "globals.h"

float Lander::thrust = 2.5f;
float Lander::rotationSpeed = 1.0f;
float Lander::fuelConsumption = Lander::initialFuelConsumption;

Lander::Lander(int screenWidth, int screenHeight) {
    thrustMusic = LoadMusicStream("data/thrust.mp3");
    if (thrustMusic.stream.buffer == NULL) {
        #ifdef DEBUG
        TraceLog(LOG_ERROR, "Failed to load thrust music: data/thrust.mp3");
        #endif
    } else {
        #ifdef DEBUG
        TraceLog(LOG_INFO, "Successfully loaded thrust music");
        #endif
        SetMusicVolume(thrustMusic, 0.33f);
        //SetMusicPitch(thrustMusic, 1.0f);
        // Enable looping of the thrust sound
        thrustMusic.looping = true;
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
        SetSoundVolume(crashSound, 0.33f);  
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
    // When resetting, we do want to completely stop the sound
    StopMusicStream(thrustMusic);
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
            velocityX += sinf(angle * DEG2RAD) * thrust * dt;
            velocityY -= cosf(angle * DEG2RAD) * thrust * dt;
            fuel = fmaxf(0.0f, fuel - fuelConsumption * dt);
        }

        if (isRotating) {
            if (rotatingLeft) {
                angle = fmodf(angle + rotationSpeed, 360.0f);
            }
            if (rotatingRight) {
                angle = fmodf(angle - rotationSpeed, 360.0f);
            }
            
            fuel = fmaxf(0.0f, fuel - (fuelConsumption * 0.5f * dt));
        }

        if (shouldPlayThrustSound && thrustMusic.stream.buffer != NULL) {
            if (!wasThrusting && !wasRotating) {
                // If the sound wasn't playing before, start it
                PlayMusicStream(thrustMusic);
                #ifdef DEBUG
                TraceLog(LOG_INFO, "Started playing thrust music");
                #endif
            } else {
                // If it was paused, resume it
                ResumeMusicStream(thrustMusic);
            }
            // Update the music stream to ensure continuous playback
            UpdateMusicStream(thrustMusic);
            wasThrusting = true;
            wasRotating = true;
        } else if ((wasThrusting || wasRotating) && thrustMusic.stream.buffer != NULL) {
            // Pause instead of stop to allow for smoother transitions
            PauseMusicStream(thrustMusic);
            wasThrusting = false;
            wasRotating = false;
            #ifdef DEBUG
            TraceLog(LOG_INFO, "Paused thrust music");
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
                            // Make sure thrust sound is completely stopped when landing
                            if (thrustMusic.stream.buffer != NULL) {
                                StopMusicStream(thrustMusic);
                                wasThrusting = false;
                                wasRotating = false;
                            }
                            PlaySound(landSound);
                            #ifdef DEBUG
                            TraceLog(LOG_INFO, "Land sound played");
                            #endif
                        } else {
                            crashed = true;
                            // Make sure thrust sound is completely stopped when crashing
                            if (thrustMusic.stream.buffer != NULL) {
                                StopMusicStream(thrustMusic);
                                wasThrusting = false;
                                wasRotating = false;
                            }
                            PlaySound(crashSound);
                            #ifdef DEBUG
                            TraceLog(LOG_INFO, "Crash sound played - wrong angle");
                            #endif

                            crashPosX = centerX;
                            crashPosY = centerY;
                        }
                    } else {
                        crashed = true;
                        // Make sure thrust sound is completely stopped when crashing
                        if (thrustMusic.stream.buffer != NULL) {
                            StopMusicStream(thrustMusic);
                            wasThrusting = false;
                            wasRotating = false;
                        }
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

    if (!crashed && wasThrusting && fuel > 0) {
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

void Lander::Cleanup() {
    UnloadMusicStream(thrustMusic);
    UnloadSound(landSound);
    UnloadSound(crashSound);
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
    if (flameTexture.id != 0) {
        UnloadTexture(flameTexture);
    }
} 