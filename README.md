# Raylib Game Template

A modern, cross-platform game template built with raylib, supporting both desktop and web builds. This template serves as a foundation for creating games that can be deployed to multiple platforms, including itch.io.

## Features

- **Cross-Platform Support**: Build for desktop and web platforms
- **Responsive Design**: Dynamic resizing support for desktop, web, and mobile web
- **Render to Texture**: Advanced rendering approach for consistent visuals across platforms
- **Automated Builds**: Automatic zip generation for easy itch.io deployment
- **CMake Integration**: Modern build system for desktop platforms
- **Emscripten Support**: Web builds via Emscripten

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

To build for web platforms, simply run:
```bash
./build_web.sh
```

This will:
- Build the project using Emscripten
- Generate a web-compatible build
- Create a `web-build.zip` file ready for itch.io deployment

## Project Structure

- `src/`: Source code directory
- `lib/`: Library dependencies
- `Font/`: Font assets
- `build/`: Desktop build output
- `web-build/`: Web build output
- `CMakeLists.txt`: CMake build configuration
- `build_web.sh`: Web build script
- `custom_shell.html`: Custom HTML shell for web builds

## Deployment

### itch.io Deployment

The build system automatically generates zip files:
- Desktop builds: Located in the `build` directory
- Web builds: `web-build.zip` is created after running `build_web.sh`

These zip files can be directly uploaded to itch.io for distribution.

## Technical Details

### Render to Texture Approach

The template uses a render-to-texture approach to ensure:
- Consistent visuals across different screen sizes
- Proper scaling on mobile devices
- Smooth resizing on desktop platforms

### Dynamic Resizing

The game automatically handles:
- Window resizing on desktop
- Browser window resizing
- Mobile device orientation changes
- Different screen resolutions

## License

This project is licensed under the terms specified in the `LICENSE.txt` file.