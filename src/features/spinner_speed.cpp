// Spinner speed multiplier.
//
// Game reference: daSpinner_c::posMove() (src/d/actor/d_a_spinner.cpp) moves the Spinner by
// speedF each frame, both on the ground and along rails. We scale speedF only for the duration
// of posMove, so acceleration, deceleration and ride time stay vanilla. The built-in
// "Fast Spinner" cheat (hold R, capped at 60) stacks with this multiplier.

#include "../config.hpp"
#include "../feature.hpp"
#include "../ui_helpers.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include "d/dolzel_rel.h"  // IWYU pragma: keep
#include "d/actor/d_a_spinner.h"
#include "m_Do/m_Do_controller_pad.h"

namespace {

cfg::BoolVar s_enabled{"spinnerSpeedEnabled", false};
cfg::IntVar s_percent{"spinnerSpeedPercent", 200};
cfg::BoolVar s_holdROnly{"spinnerSpeedHoldR", false};

// State carried from the pre-hook to the post-hook.
daSpinner_c* s_scaledSpinner = nullptr;
f32 s_originalSpeed = 0.0f;
f32 s_scaledSpeed = 0.0f;

DEFINE_HOOK(&daSpinner_c::posMove, SpinnerPosMove);

HookAction onPosMovePre(ModContext*, void* args, void*, void*) {
    s_scaledSpinner = nullptr;
    if (!s_enabled.value || s_percent.value == 100) {
        return HOOK_CONTINUE;
    }
    if (s_holdROnly.value && !mDoCPd_c::getHoldR(PAD_1)) {
        return HOOK_CONTINUE;
    }
    auto spinner = mods::arg<daSpinner_c*>(args, 0);
    s_scaledSpinner = spinner;
    s_originalSpeed = spinner->speedF;
    spinner->speedF *= static_cast<f32>(s_percent.value) / 100.0f;
    s_scaledSpeed = spinner->speedF;
    return HOOK_CONTINUE;
}

void onPosMovePost(ModContext*, void* args, void*, void*) {
    auto spinner = mods::arg<daSpinner_c*>(args, 0);
    if (spinner != s_scaledSpinner) {
        return;
    }
    s_scaledSpinner = nullptr;
    // posMove may assign speedF itself (e.g. during cutscenes); keep the game's value then.
    if (spinner->speedF == s_scaledSpeed) {
        spinner->speedF = s_originalSpeed;
    }
}

ModResult initialize() {
    ModResult result;
    if ((result = cfg::registerVar(s_enabled)) != MOD_OK ||
        (result = cfg::registerVar(s_percent)) != MOD_OK ||
        (result = cfg::registerVar(s_holdROnly)) != MOD_OK)
    {
        return result;
    }
    if ((result = mods::hook::add_pre<SpinnerPosMove>(onPosMovePre)) != MOD_OK ||
        (result = mods::hook::add_post<SpinnerPosMove>(onPosMovePost)) != MOD_OK)
    {
        mods::log::error("spinner speed: failed to install hooks ({})", static_cast<int>(result));
        return result;
    }
    return MOD_OK;
}

void buildUi(UiElementHandle pane) {
    svc_ui->pane_add_section(mod_ctx, pane, "Spinner Speed");
    ui::addToggle(pane, s_enabled, "Enable", "Multiplies the Spinner's movement speed.");
    ui::addPercent(pane, s_percent, "Speed", "100% = vanilla. Very high values may clip through walls.",
        100, 500, 25);
    ui::addToggle(pane, s_holdROnly, "Only while holding R",
        "Apply the multiplier only while R is held.");
}

}  // namespace

const Feature features::spinnerSpeed{"Spinner Speed", initialize, nullptr, buildUi};
