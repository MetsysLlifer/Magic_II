# Magic_II

## Build (macOS)

This project `makefile` is currently set up for macOS + Homebrew raylib.

Install dependencies:

```bash
brew install raylib
```

Build and run:

```bash
make
make run
```

## Build (Android, separate path)

Android support is provided in a separate build pipeline so desktop and mobile configs do not conflict.

Prerequisites:

- Android SDK + NDK installed.
- `cmake` installed (`ninja` is optional).

`makefile.android` can auto-detect NDK on macOS (`~/Library/Android/sdk/ndk/...`).
If auto-detect fails, set `ANDROID_NDK_HOME` or `ANDROID_NDK_ROOT`.

Use the dedicated Android makefile:

```bash
make -f makefile.android android-build
```

Optional build variables:

```bash
make -f makefile.android android-build ANDROID_ABI=arm64-v8a ANDROID_PLATFORM=android-24
```

This generates an Android native library build in `build-android/`.
For packaging/signing APKs, integrate the output with your Android app project or raylib Android template.
See full steps in `ANDROID_GUIDE.md`.

## In-Game Quick Guide

### Singleplayer (My World)

1. Start the game (`make run`).
2. In main menu, pick a world slot.
3. Press **MY WORLD**.
4. Play and edit spells.
5. Save with `F5` (desktop) or rely on autosave.
6. Pause with `ESC` and use:
	- `RETURN MENU`
	- `ABOUT`
	- `MORE`
	- `REVIVE`

### Multiplayer

1. In main menu, press **MULTIPLAYER**.
2. Pick world slot.
3. Host uses **HOST SESSION**.
4. Other players use **JOIN SESSION** and enter host IP.
5. Host is authoritative for restart/world sync events.
6. Clients use **REVIVE** instead of restart.

### Android touch controls

When built with Android target:

- Left pad: movement
- CAST button: hold/release cast
- LIFE button: lifespan charge
- JUMP button: jump
- Top buttons: PAUSE, CRAFT, LAYER, MENU

## New Engine Notes

This build now includes:

- Dynamic scalar spell nodes with conditional logic gears.
- Toolcraft actions (build, dig, moisten, heat, singularity seeding).
- Persistent black/white hole lifecycle with entanglement pairing.
- Ecosystem-reactive NPC steering (hazards + singularity-aware path intent).
- Main menu flow: My World, Multiplayer, Others.
- World-slot saves (`magic_world/world_1.bin` ... `world_3.bin`).
- Local player-profile persistence per world (`magic_world/player_world_#.bin`) to keep loadout/items on that machine.
- Multiplayer foundations: UDP host/client with multiple joiners (up to `MAX_PLAYERS`).
- Expanded pause menu actions: Resume, Return Menu, About, More, Restart/Revive.

## Main Menu

At launch, the game opens into:

- My World: loads the selected world slot locally.
- Multiplayer: asks which world slot to run, then host or join.
- Others: quick compendium, utility actions, and save management.

In-game shortcuts:

- `F5`: save current My World progress.
- `F9`: return to main menu.
- `F11`: toggle fullscreen.
- `ESC`: open pause menu (Return Menu / About / More / Revive).

## Multiplayer

### Internet (UDP)

Before hosting or joining, select a world slot from the multiplayer menu.

Run one instance as host:

```bash
./metsys_engine --host
```

Run another instance as client:

```bash
./metsys_engine --join 127.0.0.1
```

Replace `127.0.0.1` with the host machine IP for LAN testing.

Notes:

- Host can restart the world from pause; clients cannot restart and should use revive.
- Each machine keeps local player profile items/loadout per selected world slot.

## Common issue: `raylib.h` / `raygui.h` not found

If VS Code or compiler shows header-not-found errors, verify these files exist:

- macOS (Homebrew):
	- `/opt/homebrew/include/raylib.h`
	- `/usr/local/include/raygui.h` (or `/opt/homebrew/include/raygui.h`)
- Linux (typical):
	- `/usr/include/raylib.h`
	- `/usr/include/raygui.h`
- Windows (MSYS2/MinGW typical):
	- `C:/msys64/mingw64/include/raylib.h`
	- `C:/msys64/mingw64/include/raygui.h`

`raygui` is header-only in this project. If missing, download it from:

https://github.com/raysan5/raygui/blob/master/src/raygui.h

## VS Code IntelliSense fix (all OS)

If code builds but editor still shows red squiggles, add include paths in `.vscode/settings.json`:

```jsonc
{
	"C_Cpp.default.includePath": [
		"${workspaceFolder}/src",
		"/opt/homebrew/include",
		"/usr/local/include",
		"/usr/include",
		"C:/msys64/mingw64/include"
	]
}
```

Use paths that match your OS installation.

## Linux / Windows note

The current `makefile` links macOS frameworks (`Cocoa`, `OpenGL`, `IOKit`), so it is macOS-specific.
For Linux/Windows builds, adjust include/library/linker flags to your local raylib toolchain.

## CMake note

A cross-platform `CMakeLists.txt` is included for desktop and Android builds.
On Android, it builds a shared library (`main`) and enables Android platform guards.

## Android Icon (PNG) Placement

Put launcher PNG files in your Android app project under:

- `android/app/src/main/res/mipmap-mdpi/ic_launcher.png`
- `android/app/src/main/res/mipmap-hdpi/ic_launcher.png`
- `android/app/src/main/res/mipmap-xhdpi/ic_launcher.png`
- `android/app/src/main/res/mipmap-xxhdpi/ic_launcher.png`
- `android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png`

Optional round icon:

- `android/app/src/main/res/mipmap-mdpi/ic_launcher_round.png`
- `android/app/src/main/res/mipmap-hdpi/ic_launcher_round.png`
- `android/app/src/main/res/mipmap-xhdpi/ic_launcher_round.png`
- `android/app/src/main/res/mipmap-xxhdpi/ic_launcher_round.png`
- `android/app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.png`
