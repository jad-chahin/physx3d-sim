# physx3d-sim

`physx3d-sim` is a real-time 3D rigid-body physics sandbox written in modern C++ with OpenGL rendering.

## Requirements

- CMake `>= 3.20`
- A C++20-capable compiler
- OpenGL 3.3 capable GPU/driver
- Git (for dependency fetches via CMake `FetchContent`)

## Build

From project root:

```powershell
cmake -S . -B cmake-build-debug
cmake --build .\cmake-build-debug --target physics3d -j 6
```

## Run

```powershell
.\cmake-build-debug\physics3d.exe
```

If you get a DLL error on Windows when launching from terminal, make sure runtime DLL locations are available in `PATH` (for example MinGW runtime and GLFW build output folders used by your environment).

## Controls (Defaults)

- `ESC` open/close pause menu (releases/captures mouse)
- `SPACE` freeze/unfreeze simulation
- `- / =` decrease/increase simulation speed
- `1` reset simulation speed to `1x`
- `W A S D` move camera on horizontal plane
- `Q / E` move camera down/up
- `Mouse` look around
- Pause menu controls section: press `1-0` to pick an action, then press a key to rebind it
- Control bindings are saved to `controls.cfg` in the working directory

## Project Layout

- `src/sim` physics simulation core (world stepping, broadphase, collision, joints)
- `src/render` rendering helpers and shaders
- `src/ui` on-screen debug overlay
- `src/input` camera/input utilities
- `src/main.cpp` app entrypoint and scene setup

## Development Notes

- Scene setup is currently code-driven in `main.cpp`.
