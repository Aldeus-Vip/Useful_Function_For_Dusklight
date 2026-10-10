// Swim speed multiplier (works without the Zora Armor).
//
// Game reference: daAlink_c::posMove() (src/d/actor/d_a_alink.cpp) turns mNormalSpeed into the
// horizontal velocity. While Link is in a swim proc we scale mNormalSpeed for the duration of
// posMove only, so the swim acceleration/animation logic keeps seeing vanilla values and the
// scaling never compounds across frames. Vertical movement (diving/surfacing) is unchanged.

#include "../config.hpp"
#include "../feature.hpp"
#include "../ui_helpers.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

namespace {

cfg::BoolVar s_enabled{"swimSpeedEnabled", false};
cfg::IntVar s_percent{"swimSpeedPercent", 150};
cfg::BoolVar s_withZora{"swimSpeedWithZora", false};
cfg::BoolVar s_wolf{"swimSpeedWolf", true};

daAlink_c* s_scaledLink = nullptr;
f32 s_originalSpeed = 0.0f;
f32 s_scaledSpeed = 0.0f;

bool isHumanSwimProc(u16 proc) {
    switch (proc) {
    case daAlink_c::PROC_SWIM_UP:
    case daAlink_c::PROC_SWIM_WAIT:
    case daAlink_c::PROC_SWIM_MOVE:
    case daAlink_c::PROC_SWIM_DIVE:
    case daAlink_c::PROC_SWIM_HOOKSHOT_SUBJECT:
    case daAlink_c::PROC_SWIM_HOOKSHOT_MOVE:
        return true;
    default:
        return false;
    }
}

bool isWolfSwimProc(u16 proc) {
    return proc == daAlink_c::PROC_WOLF_SWIM_UP || proc == daAlink_c::PROC_WOLF_SWIM_WAIT ||
           proc == daAlink_c::PROC_WOLF_SWIM_MOVE;
}

bool shouldScale(daAlink_c* link) {
    if (isWolfSwimProc(link->mProcID)) {
        return s_wolf.value;
    }
    if (!isHumanSwimProc(link->mProcID)) {
        return false;
    }
    // Iron Boots / heavy Magic Armor walk on the floor; leave that alone.
    if (link->checkBootsOrArmorHeavy()) {
        return false;
    }
    const bool zora = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(
                          COLLECT_CLOTHING) == dItemNo_WEAR_ZORA_e;
    return !zora || s_withZora.value;
}

DEFINE_HOOK(&daAlink_c::posMove, LinkPosMove);

HookAction onPosMovePre(ModContext*, void* args, void*, void*) {
    s_scaledLink = nullptr;
    if (!s_enabled.value || s_percent.value == 100) {
        return HOOK_CONTINUE;
    }
    auto link = mods::arg<daAlink_c*>(args, 0);
    if (!shouldScale(link)) {
        return HOOK_CONTINUE;
    }
    s_scaledLink = link;
    s_originalSpeed = link->mNormalSpeed;
    link->mNormalSpeed *= static_cast<f32>(s_percent.value) / 100.0f;
    s_scaledSpeed = link->mNormalSpeed;
    return HOOK_CONTINUE;
}

void onPosMovePost(ModContext*, void* args, void*, void*) {
    auto link = mods::arg<daAlink_c*>(args, 0);
    if (link != s_scaledLink) {
        return;
    }
    s_scaledLink = nullptr;
    // posMove may reset mNormalSpeed itself; keep the game's value then.
    if (link->mNormalSpeed == s_scaledSpeed) {
        link->mNormalSpeed = s_originalSpeed;
    }
}

ModResult initialize() {
    ModResult result;
    if ((result = cfg::registerVar(s_enabled)) != MOD_OK ||
        (result = cfg::registerVar(s_percent)) != MOD_OK ||
        (result = cfg::registerVar(s_withZora)) != MOD_OK ||
        (result = cfg::registerVar(s_wolf)) != MOD_OK)
    {
        return result;
    }
    if ((result = mods::hook::add_pre<LinkPosMove>(onPosMovePre)) != MOD_OK ||
        (result = mods::hook::add_post<LinkPosMove>(onPosMovePost)) != MOD_OK)
    {
        mods::log::error("swim speed: failed to install hooks ({})", static_cast<int>(result));
        return result;
    }
    return MOD_OK;
}

void buildUi(UiElementHandle pane) {
    svc_ui->pane_add_section(mod_ctx, pane, "Swim Speed");
    ui::addToggle(pane, s_enabled, "Enable", "Multiplies horizontal swimming speed.");
    ui::addPercent(pane, s_percent, "Speed", "100% = vanilla.", 100, 400, 25);
    ui::addToggle(pane, s_withZora, "Also with Zora Armor",
        "OFF: the Zora Armor keeps its own (already fast) speed.");
    ui::addToggle(pane, s_wolf, "Also as Wolf", "Apply to Wolf Link's swimming too.");
}

}  // namespace

const Feature features::swimSpeed{"Swim Speed", initialize, nullptr, buildUi};
