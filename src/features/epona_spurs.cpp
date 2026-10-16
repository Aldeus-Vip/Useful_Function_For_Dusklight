// Infinite Epona spurs (dash/lash count).
//
// Game reference: daHorse_c::setLashCnt() (src/d/actor/d_a_horse.cpp) spends one of the 6 spurs
// per lash (m_lashCnt) and refills them over time (m_lashRecoverTime). A post-hook keeps the
// count full, so the meter (dMeter2Info_setHorseLifeCount, later in daHorse_c::execute) always
// shows 6 and the "no spurs left" refusal never happens.

#include "../config.hpp"
#include "../feature.hpp"
#include "../ui_helpers.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include "d/dolzel_rel.h"  // IWYU pragma: keep
#include "d/actor/d_a_horse.h"

namespace {

cfg::BoolVar s_enabled{"eponaInfiniteSpurs", false};

constexpr s16 kMaxSpurs = 6;

DEFINE_HOOK(&daHorse_c::setLashCnt, HorseSetLashCnt);

void onSetLashCntPost(ModContext*, void* args, void*, void*) {
    if (!s_enabled.value) {
        return;
    }
    auto horse = mods::arg<daHorse_c*>(args, 0);
    horse->m_lashCnt = kMaxSpurs;
    // Nothing to refill, so no refill timer (and no refill sounds).
    horse->m_lashRecoverTime = 0;
}

ModResult initialize() {
    ModResult result = cfg::registerVar(s_enabled);
    if (result != MOD_OK) {
        return result;
    }
    if ((result = mods::hook::add_post<HorseSetLashCnt>(onSetLashCntPost)) != MOD_OK) {
        mods::log::error("epona spurs: failed to install hook ({})", static_cast<int>(result));
        return result;
    }
    return MOD_OK;
}

void buildUi(UiElementHandle pane) {
    svc_ui->pane_add_section(mod_ctx, pane, "Epona");
    ui::addToggle(pane, s_enabled, "Infinite spurs",
        "Epona's spurs (dashes) never run out. The spur meter always stays full.");
}

}  // namespace

const Feature features::eponaSpurs{"Epona", initialize, nullptr, buildUi};
