#pragma once

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

    
    static float thrust;
    static float rotationSpeed;
    static float fuelConsumption;
    static constexpr float collisionScale = 0.8f;
    static constexpr float initialFuelConsumption = 10.0f;

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
    float crashPosX, crashPosY;
    Music thrustMusic;
    Sound landSound;
    Sound crashSound;
    bool wasThrusting;
    bool wasRotating;
    Vector2* terrain;
    int terrainPoints;
    Texture2D texture;
    Texture2D flameTexture;
}; 