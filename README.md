# Raven

Raven is an open source internal for **Hello Neighbor: Hide and Seek**, using our internal overlay **https://github.com/MalwareMania/RIO**
It was created for educational and learning purposes, and for fun

## Features

- Fly
- Adjustable fly speed
- Speed boost
- Jump power
- Move 7 steps (clip through walls)
- ImGui interface

## Controls

| Key | Action |
|---|---|
| `Insert` | Show or hide the Raven menu |
| `Delete` | Unlock or lock the menu cursor |
| `5` | Toggle fly |
| `6` | Toggle speed boost |
| `7` | Toggle high jump |
| `8` | Move 7 steps |
| `WASD` | Fly all directions |
| `Space` | Fly upward |
| `Ctrl` | Fly downward |
| `Shift` | Speed up the fly speed |
| `End` | Restore all values and unload Raven |

Raven hasn't been tested in **Hello Neighbor** yet, however Raven has been confirmed working for the Hide and seek version.

## Usage

The usage here is to help build the source, there will be an already built dll and injector in the Releases tab for no extra work.

1. Build the solution using `Release | x64`.
2. Launch **Hello Neighbor: Hide and Seek**.
3. Keep `Raven.dll` and `Raven-Injector.exe` in the same directory.
4. Run `Raven-Injector.exe`.
5. Press `Insert` to open the menu.
6. Press `Delete` to unlock the cursor.
7. Wait until the player pointer is ready after entering a level.

Raven can be injected from the main menu It waits until the player becomes available.

## Building

### Requirements

- Windows 10 or Windows 11
- Visual Studio 2022
- Desktop development with C++
- Windows SDK
- x64 build target

Open `Raven.sln`, select:

```text
Release | x64
```

Then choose:

```text
Build → Build Solution
```

You can also run:

```bat
build-all.cmd
```

Compiled files are placed in:

```text
x64\Release
```

## Project Structure

```text
Raven/
├── deps/
│   ├── imgui/
│   └── minhook/
├── src/
│   ├── main.cpp
│   ├── cheat/
│   │   ├── cheat.cpp
│   │   └── cheat.h
│   ├── gui/
│   │   ├── gui.cpp
│   │   └── gui.h
│   └── offsets/
│       └── offsets.h
├── Raven.vcxproj
└── Raven.vcxproj.filters

Raven-Injector/
Raven.sln
build-all.cmd
```

The confirmed binary offsets for Hello Neighbor hide and seek are in: 

```text
Raven/src/offsets/offsets.h
```

A game update may change offset values, however Hello neighbor hide and seek is unlikely to be updated

Only x64 builds are supported.

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui)
- [MinHook](https://github.com/TsudaKageyu/minhook)
- [RIO](https://github.com/MalwareMania/RIO)
- DirectX 11
- Windows API

## Credits

- Dear ImGui contributors
- MinHook contributors
- The original RIO imgui internal overlay
- Spectral and Sailz (spectral0914 and sailz11) on discord.
