# Useful Functions for Dusklight

A native (C++) mod for [Dusklight](https://github.com/TwilitRealm/dusklight) (Twilight Princess PC port) that bundles
small convenience features.

## Features

| Feature | What it does | Options |
|---|---|---|
| Magic Armor heavy toggle | Toggle the Magic Armor's "heavy" state (Iron Boots behavior) even when you have rupees | On/off, toggle button, keep protection while heavy |
| Spinner speed | Multiply the Spinner's movement speed | Enable, speed (100–500%), only while holding R |
| Spinner water run | Ride the Spinner across water instead of sinking | Enable |
| Swim speed | Swim faster without the Zora Armor | Enable, speed (100–400%), also with Zora Armor, also as Wolf |

All options are in the in-game **Mods** window → **Useful Functions** panel and are saved to `config.json`.

### Magic Armor heavy toggle

- Press the toggle button (default: **L + R + D-pad Down**) to switch heavy mode on/off. A toast shows the new state.
- Available buttons: L + R + Down, R + Down, L + Down, Z + Down, R + Up, or Disabled (panel only).
- **Keep protection while heavy**
  - On (default): the armor still blocks damage using rupees while heavy.
  - Off: vanilla rule; the armor stops protecting while heavy.
- Has no effect unless the Magic Armor is equipped.

### Spinner speed

- Scales the Spinner's speed only during its movement step (`daSpinner_c::posMove`). Acceleration, deceleration and
  ride time stay vanilla.
- Stacks with Dusklight's built-in **Fast Spinner** cheat (hold R, capped at 60).
- Very high values may clip through walls.

### Spinner water run

- Keeps the Spinner on the water surface instead of letting it sink (`daSpinner_c::posMove` normally deletes it once
  it is more than 50 units under water).
- Never applies to lava.
- When the Spinner runs out of speed, it stops as usual and Link drops into the water. Combine with Spinner speed or
  the Fast Spinner cheat to keep going.

### Swim speed

- Scales horizontal swimming speed only. Diving and surfacing speeds are unchanged.
- Not applied while walking underwater with the Iron Boots or a heavy Magic Armor.
- The Zora Armor is already fast, so it is excluded by default (configurable).

## Building

```sh
cmake -B build
cmake --build build
```

- Output: `build/mods/useful_functions.dusk`.
- Copy it into the mods folder:
  - Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
  - Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
  - macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`
- A local build only works on your own platform. For distribution, use the `mod-combined` artifact from GitHub
  Actions, which supports all platforms.
- The target Dusklight version is `DUSKLIGHT_VERSION` in `CMakeLists.txt` (currently `v2.0.3`).

## Project layout (adding features)

```
src/
├── mod.cpp            Lifecycle and UI panel; features are listed in kFeatures
├── feature.hpp        Feature interface (initialize / update / buildUi)
├── config.*           Thin ConfigService wrapper (values cached once per frame)
├── ui_helpers.*       Toggle, number, dropdown and toast helpers
└── features/
    ├── magic_armor.cpp
    ├── spinner_speed.cpp
    ├── spinner_water.cpp
    └── swim_speed.cpp
```

To add a new feature:

1. Create a file in `src/features/` that defines a `Feature`.
2. Add an `extern` declaration to `feature.hpp`.
3. Append it to `kFeatures` in `mod.cpp`.
4. Add the file to `SOURCES` in `CMakeLists.txt`.
