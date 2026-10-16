// Useful Functions for Dusklight: a set of small convenience features.
// Each feature lives in src/features/; this file wires them into the mod lifecycle and UI.

#include "config.hpp"
#include "feature.hpp"

#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"
#include "mods/svc/ui.h"

DEFINE_MOD();

IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

namespace {

const Feature* const kFeatures[] = {
    &features::magicArmor,
    &features::spinnerSpeed,
    &features::spinnerWater,
    &features::swimSpeed,
    &features::eponaSpurs,
};

ModResult buildPanel(ModContext*, UiElementHandle panel, void*, ModError*) {
    for (const Feature* feature : kFeatures) {
        feature->buildUi(panel);
    }
    return MOD_OK;
}

}  // namespace

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    for (const Feature* feature : kFeatures) {
        ModResult result = feature->initialize();
        if (result != MOD_OK) {
            return mods::set_error(error, result, "failed to initialize a feature");
        }
    }

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = buildPanel;
    svc_ui->register_mods_panel(mod_ctx, &panel);

    mods::log::info("initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    cfg::refreshAll();
    for (const Feature* feature : kFeatures) {
        if (feature->update != nullptr) {
            feature->update();
        }
    }
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    return MOD_OK;
}
}
