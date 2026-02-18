# Build On Windows

## Prerequisites
- Windows 10/11
- Visual Studio 2022 with "Desktop development with C++"
- CMake 3.22+

## Build (PowerShell)
From the project root:

```powershell
cmake --preset windows-vs2022-release
cmake --build --preset windows-release
```

## Output
The app executable will be in:

```text
build-win/SpeakNSpellSynth_artefacts/Release/SAM-style Voice Synthesizer.exe
```

## Notes
- The app uses Node.js at runtime for SAM rendering.
- Install Node.js and ensure one of these exists:
  - `C:\Program Files\nodejs\node.exe` (in PATH)
  - or `node` available in PATH
- Keep `Source/sam_bridge.js` and `third_party/*.js` with the app when packaging.
