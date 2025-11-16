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

| Key    | Action     |
|--------|------------|
| W      | DPad Up    |
| A      | DPad Left  |
| S      | DPad Down  |
| D      | DPad Right |
| ;      | B          |
| '      | A          |
| Enter  | Start      |
| RShift | Select     |

| Key       | Action                        |
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
