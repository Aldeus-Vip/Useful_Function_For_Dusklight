// Spinner water run: ride the Spinner across water surfaces instead of sinking.
//
// Game reference: daSpinner_c::posMove() (src/d/actor/d_a_spinner.cpp) deletes the Spinner once
// it is more than 50 units under water (or touches lava). A post-hook keeps the Spinner on the
// water surface instead: it snaps the height to the surface, treats it as ground and cancels the
// water deletion. Lava is never affected. The Spinner still stops normally when it runs out of
// speed, dropping Link into the water.

#include "../config.hpp"
#include "../feature.hpp"
#include "../ui_helpers.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include "d/dolzel_rel.h"  // IWYU pragma: keep
#include "d/actor/d_a_spinner.h"
#include "d/d_com_inf_game.h"

namespace {

cfg::BoolVar s_enabled{"spinnerWaterRun", false};

// Polygon attribute used for lava in daSpinner_c::posMove's delete check.
constexpr int kLavaAttribute = 6;
// How far below the surface we still pull the Spinner back up (larger values would teleport it
// up from deep water).
constexpr f32 kMaxSnapDepth = 200.0f;

daSpinner_c* s_spinner = nullptr;
u8 s_deleteFlgBefore = 0;

DEFINE_HOOK(&daSpinner_c::posMove, SpinnerPosMoveWater);

HookAction onPosMovePre(ModContext*, void* args, void*, void*) {
    auto spinner = mods::arg<daSpinner_c*>(args, 0);
    s_spinner = spinner;
    s_deleteFlgBefore = spinner->mDeleteFlg;
    return HOOK_CONTINUE;
}

void onPosMovePost(ModContext*, void* args, void* retval, void*) {
    auto spinner = mods::arg<daSpinner_c*>(args, 0);
    if (!s_enabled.value || spinner != s_spinner) {
        return;
    }
    s_spinner = nullptr;

    // Ground contact is not excluded: on a gentle slope into water the Spinner stays grounded on
    // the lake bed while it goes under, and vanilla deletes it once it is 50 units deep.
    dBgS_LinkAcch& acch = spinner->mAcch;
    if (!acch.ChkWaterHit()) {
        return;
    }
    if (g_dComIfG_gameInfo.play.mBgs.GetPolyAtt0(acch.m_wtr) == kLavaAttribute) {
        return;
    }

    const f32 waterY = acch.m_wtr.GetHeight();
    const f32 depth = waterY - spinner->current.pos.y;
    if (waterY <= acch.GetGroundH() || depth < 0.0f || depth > kMaxSnapDepth) {
        return;
    }

    // Stand on the surface.
    spinner->current.pos.y = waterY;
    spinner->speed.y = 0.0f;
    spinner->mJumpFlg = false;
    acch.SetGroundHit();

    // Undo the "sank in water" deletion from this frame only.
    if (!s_deleteFlgBefore && spinner->mDeleteFlg) {
        spinner->mDeleteFlg = false;
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
    }
}

ModResult initialize() {
    ModResult result = cfg::registerVar(s_enabled);
    if (result != MOD_OK) {
        return result;
    }
    if ((result = mods::hook::add_pre<SpinnerPosMoveWater>(onPosMovePre)) != MOD_OK ||
        (result = mods::hook::add_post<SpinnerPosMoveWater>(onPosMovePost)) != MOD_OK)
    {
        mods::log::error("spinner water run: failed to install hooks ({})", static_cast<int>(result));
        return result;
    }
    return MOD_OK;
}

void buildUi(UiElementHandle pane) {
    svc_ui->pane_add_section(mod_ctx, pane, "Spinner Water Run");
    ui::addToggle(pane, s_enabled, "Enable",
        "Ride the Spinner across water instead of sinking. Does not work on lava. "
        "When the Spinner runs out of speed, Link drops into the water.");
}

}  // namespace

const Feature features::spinnerWater{"Spinner Water Run", initialize, nullptr, buildUi};
