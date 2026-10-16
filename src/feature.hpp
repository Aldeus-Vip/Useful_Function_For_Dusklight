#pragma once

#include "mods/svc/ui.h"

// Each convenience feature lives in its own file under src/features/ and exposes one of these.
// To add a feature: write a new file, define a Feature, and append it to kFeatures in mod.cpp.
struct Feature {
    const char* title;
    // Registers config vars and installs hooks. Called once from mod_initialize.
    ModResult (*initialize)();
    // Called every frame from mod_update, after config values are refreshed. May be null.
    void (*update)();
    // Adds this feature's controls to the mod panel.
    void (*buildUi)(UiElementHandle pane);
};

namespace features {
extern const Feature magicArmor;
extern const Feature spinnerSpeed;
extern const Feature spinnerWater;
extern const Feature swimSpeed;
extern const Feature eponaSpurs;
}  // namespace features
