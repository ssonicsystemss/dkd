# dkd

**dkd (dualkickdeployment)** by sonicsystems: a hardstyle / rawstyle **kick + reverse bass from one note**, as a VST3 / CLAP
instrument. 

![dkd](docs/dkd.png)

## Download

**[dkd for Windows (VST3, 64-bit) - latest release](../../releases/latest)**

## What it does

One held note plays a kick on every beat of the host and reverse-bass swells between the kicks (1/4, 1/8 or 1/16
steps), locked to the host tempo. Everything on the top screen is about the timing of one beat: drag the four square
handles (TAIL, IN, OUT, RISE) and watch the kick lane and the bass lane; the plugin warns when the kick tail and the bass
overlap. Own sample: load your own kick on top of (or instead of) the synthesized one.

| Where | What |
|---|---|
| top screen | preset bar, timing diagram of one beat, numbers, output meter |
| bottom screen | pages BEAT, KICK, BASS, OUT, OWN; every parameter is a row: click or drag (Shift = fine), double-click = default, right-click = automation clip / freeze from RANDOM |
| data wheel (left) | turns the parameter touched last (a row or a handle) |
| pads (right) | TEST (hold = plays F), PTN (bass pattern), KICK and BASS (mute while you tweak; not saved) |
| keys | RANDOM, UNDO, PRESET -, PRESET + |

Plugin code `Sdkd`, bundle / CLAP id `com.sonicsystems.dkd`. Parameter IDs are identical to rutherford (SINGULARITY).

## Build

Windows, Visual Studio + CMake (JUCE 8 is downloaded by CMake):

    cmake -S src/DKD_kod_zrodlowy/dkd -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target DKD_VST3 DKDRender
    build\DKDRender_artefacts\DKDRender.exe demo.wav gui.png      # render test, must print RESULT: PASS

Code: `Source/Digi.*` (screens), `Source/PluginEditor.*` (the console), `Source/RutherfordEngine.h` (sound),
`src/FISSION_shared` + `src/SONICSYSTEMS_shared` (shared UI / own-sample code).
