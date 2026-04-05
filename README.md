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

## New Engine Notes

This build now includes:

- Dynamic scalar spell nodes with conditional logic gears.
- Toolcraft actions (build, dig, moisten, heat, singularity seeding).
- Persistent black/white hole lifecycle with entanglement pairing.
- Ecosystem-reactive NPC steering (hazards + singularity-aware path intent).
- Main menu flow: My World, Multiplayer, Others.
- Local save persistence (`magic_world/savegame.bin`) with periodic autosave.
- Multiplayer foundations: UDP internet host/client.

## Main Menu

At launch, the game opens into:

- My World: loads local world progress if present.
- Multiplayer: host or join an online session.
- Others: quick compendium, utility actions, and save management.

In-game shortcuts:

- `F5`: save current My World progress.
- `F9`: return to main menu.
- `F11`: toggle fullscreen.

## Multiplayer

### Internet (UDP)

Run one instance as host:

```bash
./metsys_engine --host
```

Run another instance as client:

```bash
./metsys_engine --join 127.0.0.1
```

Replace `127.0.0.1` with the host machine IP for LAN testing.

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
