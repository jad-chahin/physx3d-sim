# physx3d-sim

`physx3d-sim` is a real-time 3D rigid-body physics sandbox written in modern C++ with OpenGL rendering.

## Requirements

- CMake `>= 3.20`
- A C++20-capable compiler
- OpenGL 3.3 capable GPU/driver
- Git (only needed to initialize the GLM submodule if you cloned without submodules)

## Build

From project root:

```powershell
git submodule update --init --recursive
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
- `Left Shift` hold for camera boost (rebindable in pause menu, fixed `4x` boost)
- `Mouse` look around
- Pause menu pages: click page tabs or use `1 / 2 / 3 / 4 / 5` to jump to `Display / Simulation / Camera / Interface / Controls`; `Tab` (or `Page Up / Page Down`) also cycles pages
- Pause menu navigation: `Up / Down` selects options on the current page
- Pause menu settings: use `Left / Right` to change values, `Enter` to apply the selected option
- Pause menu behavior: moving off an edited option with `Up / Down` discards unapplied changes
- Pause menu key rebinding: select a control, press `Enter`, then press the new key
- Pause menu controls page: `RESET CONTROLS` restores default bindings and rewrites `controls.cfg`
- Pause menu exit: click `EXIT TO HOME` (or press `X`) to close the app
- Keybind safety: duplicate key assignments are blocked (one key per action)
- Control bindings are saved to `controls.cfg` in the working directory
- Pause menu settings are saved to `settings.cfg` in the working directory

## Project Layout

- `src/sim` physics simulation core (world stepping, broadphase, collision)
- `src/render` rendering helpers and shaders
- `src/ui` HUD, pause menu, and target overlays
- `src/input` camera/input utilities
- `src/main.cpp` app entrypoint
- `src/sim/DefaultWorld.*` default world setup

## Development Notes

- Default world setup is currently code-driven in `src/sim/DefaultWorld.*`.
