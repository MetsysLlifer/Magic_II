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
