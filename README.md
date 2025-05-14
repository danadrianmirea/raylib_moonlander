# Raylib Moonlander

A classic lunar lander game built with raylib where you pilot a spacecraft to land safely on the moon's surface. The game features increasing difficulty levels, realistic physics, and is playable on both desktop and web platforms.

## Game Features

- **Realistic Lunar Physics**: Gravity affects your lander as you try to navigate to the landing pad
- **Progressive Difficulty**: Increasing gravity and fuel consumption as you advance through levels
- **Dynamic Terrain**: Randomly generated lunar terrain for each level
- **Visual Effects**: Explosion animations, thruster effects, and a space background
- **Cross-Platform**: Works on desktop (Windows, macOS, Linux) and web browsers
- **Mobile Support**: Touch controls for playing on mobile devices

## Controls

### Desktop
- **Up Arrow**: Fire thrusters
- **Left/Right Arrows**: Rotate lander
- **P**: Pause game
- **R**: Reset game

### Mobile
- **Touch Screen**: Tap for thrust and directional controls

## Building the Project

### Desktop Build (CMake)

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure and build:
```bash
cmake ..
cmake --build . --config Release
```

The executable will be created in the `build` directory.

### Web Build (Emscripten)

To build for web platforms, run:
```bash
./build_web.sh
```

This will:
- Build the project using Emscripten
- Generate a web-compatible build
- Create a `web-build.zip` file ready for itch.io deployment

## Project Structure

- `src/`: Source code for the game (lander.cpp, game.cpp, etc.)
- `data/`: Game assets (textures, sounds, music)
- `Font/`: Font assets
- `build/`: Desktop build output
- `web-build/`: Web build output
- `CMakeLists.txt`: CMake build configuration
- `build_web.sh`: Web build script

## Gameplay Tips

- Land slowly and keep your spacecraft level for a successful landing
- Watch your fuel gauge - running out of fuel means certain crash
- Each level increases in difficulty with stronger gravity
- You have three lives - use them wisely!

## Technical Details

### Rendering Approach

The game uses a render-to-texture approach to ensure:
- Consistent visuals across different screen sizes
- Proper scaling on mobile devices
- Smooth resizing on desktop platforms

## License

This project is licensed under the terms specified in the `LICENSE.txt` file.