# dkd - sonicsystems dkd (dualkickdeployment)

Hardstyle kick + reverse bass from one note (rutherford's engine) in a two-screen
music-device editor. The two screens are "early digital": a pixel font, phosphor green, segmented bars.
Plugin identity: `dkd.vst3`, plugin code `Sdkd`, bundle / CLAP id `com.sonicsystems.dkd`. Parameter IDs are identical to rutherford.

| Where | What |
|---|---|
| top screen | preset bar (< name > = factory presets, USER = save / load your own, UNDO), the timing diagram of one beat: kick lane + bass lane, drag the four square handles TAIL / IN / OUT / RISE; numbers, overlap warning, output meter |
| bottom screen | tabs BEAT, KICK, BASS, OUT, OWN; every parameter is a row: click or drag (Shift = fine), double-click = default, right-click = automation clip / freeze from RANDOM |
| data wheel (left) | turns the parameter touched last (a row or a handle); drag up / down, scroll, Shift = fine; the ring of lamps shows its value |
| pads (right) | TEST (hold = plays F), PTN (bass pattern 1/4 > 1/8 > 1/16), KICK and BASS (mute while you tweak; not saved) |
| keys | RANDOM, UNDO, PRESET -, PRESET + |

Code: `Source/Digi.*` (screens), `Source/PluginEditor.*` (the console), `Source/RutherfordEngine.h` (sound).
