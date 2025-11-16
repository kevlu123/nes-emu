# Nintendo Entertainment System Emulator

## Build Prerequisites

- A compiler toolchain
- CMake
- Ninja

## Build

When building with MSVC, you must run these commands in a VS developer command prompt.

```
cmake -Bbuild -GNinja -DSDL_STATIC=ON -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

## Run

```
./build/src/nes [.nes file]
```

## Controls

| Controller Button | Player 1 Keyboard | Player 2 Keyboard |
|-------------------|-------------------|-------------------|
| DPad Up           | W                 | Up                |
| DPad Left         | A                 | Left              |
| DPad Down         | S                 | Down              |
| DPad Right        | D                 | Right             |
| B                 | ;                 | Numpad 1          |
| A                 | '                 | Numpad 2          |
| Start             | Enter             | Numpad Enter      |
| Select            | RShift            | Numpad Plus       |

| Shortcut  | Action                        |
|-----------|-------------------------------|
| Alt+Enter | Toggle fullscreen             |
| Ctrl+W    | Close ROM                     |
| Ctrl+R    | Reset                         |
| Ctrl+P    | Toggle pause                  |
| Ctrl+M    | Toggle mute                   |
| Ctrl+G    | Toggle grid                   |
| Ctrl+B    | Toggle background             |
| Ctrl+F    | Toggle sprites                |
| Ctrl+Y    | Toggle force greyscale        |
| Ctrl+Z    | Toggle sprite 0 hit indicator |
| F10       | Step frame                    |
| F11       | Step CPU                      |
| Shift+F10 | Step scanline                 |
| Shift+F11 | Step PPU                      |
| Hold Ctrl | Inspect pixel                 |
