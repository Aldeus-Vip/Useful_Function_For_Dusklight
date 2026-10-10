// Magic Armor toggles, each switchable with a button combo:
//
// - Heavy mode: daAlink_c::checkMagicArmorHeavy() (src/d/actor/d_a_alink.cpp) decides whether the
//   armor acts like Iron Boots. A post-hook forces it to TRUE.
// - Rupee saver: daAlink_c::checkMagicArmorNoDamage() gates both the armor's damage protection and
//   all of its rupee drain (the passive drain in daAlink_c::execute and the per-hit cost in
//   daAlink_c::setDamagePoint). A post-hook forces it to FALSE, so Link takes normal damage and
//   no rupees are spent.

#include "../config.hpp"
#include "../feature.hpp"
#include "../ui_helpers.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdio>

namespace {

cfg::BoolVar s_heavyOn{"armorHeavyOn", false};
cfg::IntVar s_heavyHotkey{"armorHeavyHotkey", 1};  // R + A
cfg::BoolVar s_keepProtection{"armorKeepProtection", true};

cfg::BoolVar s_saverOn{"armorRupeeSaverOn", false};
cfg::IntVar s_saverHotkey{"armorRupeeSaverHotkey", 6};  // R + X

struct Hotkey {
    const char* label;
    u32 hold;     // buttons that must be held
    u32 trigger;  // button that must be pressed this frame
};

// The saved config value is the index, so only append new entries. Index 0 disables the hotkey
// (toggle from the mod panel only).
constexpr Hotkey kHotkeys[] = {
    {"Disabled (panel only)", 0, 0},
    {"R + A", PAD_TRIGGER_R, PAD_BUTTON_A},
    {"L + A", PAD_TRIGGER_L, PAD_BUTTON_A},
    {"Z + A", PAD_TRIGGER_Z, PAD_BUTTON_A},
    {"L + R + D-pad Down", PAD_TRIGGER_L | PAD_TRIGGER_R, PAD_BUTTON_DOWN},
    {"R + D-pad Down", PAD_TRIGGER_R, PAD_BUTTON_DOWN},
    {"R + X", PAD_TRIGGER_R, PAD_BUTTON_X},
    {"L + X", PAD_TRIGGER_L, PAD_BUTTON_X},
    {"Z + X", PAD_TRIGGER_Z, PAD_BUTTON_X},
};
constexpr size_t kHotkeyCount = sizeof(kHotkeys) / sizeof(kHotkeys[0]);

const char* const kHotkeyLabels[] = {
    kHotkeys[0].label, kHotkeys[1].label, kHotkeys[2].label,
    kHotkeys[3].label, kHotkeys[4].label, kHotkeys[5].label,
    kHotkeys[6].label, kHotkeys[7].label, kHotkeys[8].label,
};
static_assert(sizeof(kHotkeyLabels) / sizeof(kHotkeyLabels[0]) == kHotkeyCount);

// A mode the player can flip with its own button combo.
struct Toggle {
    cfg::BoolVar& on;
    cfg::IntVar& hotkey;
    const char* name;
    bool pending;
};

Toggle s_toggles[] = {
    {s_heavyOn, s_heavyHotkey, "Heavy mode", false},
    {s_saverOn, s_saverHotkey, "Rupee saver", false},
};

// Header-inline equivalent of daAlink_c::checkMagicArmorWearAbility(), avoiding a call into a
// game symbol that may be missing from some platforms' link stubs.
bool isWearingMagicArmor(const daAlink_c* link) {
    return !link->checkWolf() &&
           g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getSelectEquip(
               COLLECT_CLOTHING) == dItemNo_ARMOR_e &&
           !link->checkNoResetFlg2(daPy_py_c::FLG2_UNK_80000);
}

// True while daAlink_c::checkMagicArmorNoDamage() runs. In the vanilla rule the armor stops
// protecting while heavy; with "keep protection" we leave that query alone.
bool s_inNoDamageCheck = false;

DEFINE_HOOK(&daAlink_c::checkMagicArmorHeavy, CheckMagicArmorHeavy);
DEFINE_HOOK(&daAlink_c::checkMagicArmorNoDamage, CheckMagicArmorNoDamage);

void onCheckHeavyPost(ModContext*, void* args, void* retval, void*) {
    if (!s_heavyOn.value || retval == nullptr) {
        return;
    }
    if (s_inNoDamageCheck && s_keepProtection.value) {
        return;
    }
    auto link = mods::arg<const daAlink_c*>(args, 0);
    if (isWearingMagicArmor(link)) {
        *static_cast<BOOL*>(retval) = TRUE;
    }
}

HookAction onNoDamagePre(ModContext*, void*, void*, void*) {
    s_inNoDamageCheck = true;
    return HOOK_CONTINUE;
}

void onNoDamagePost(ModContext*, void*, void* retval, void*) {
    s_inNoDamageCheck = false;
    if (s_saverOn.value && retval != nullptr) {
        *static_cast<BOOL*>(retval) = FALSE;
    }
}

// Hotkey detection runs right after the game reads the pad (mDoCPd_c::read), so the combo's
// trigger button can be removed before any game code sees it (otherwise e.g. A would also
// perform an action). The button stays hidden from the game until it is released, the same way
// Dusklight suppresses pad buttons for its own menus.
u32 s_suppressedButtons = 0;

DEFINE_HOOK(&mDoCPd_c::read, PadRead);

void onPadReadPost(ModContext*, void*, void*, void*) {
    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);

    if (s_suppressedButtons != 0) {
        if ((pad.mButtonFlags & s_suppressedButtons) == 0) {
            s_suppressedButtons = 0;
        } else {
            pad.mButtonFlags &= ~s_suppressedButtons;
            pad.mPressedButtonFlags &= ~s_suppressedButtons;
        }
    }

    // Only react in-game (a player actor exists), not on the title screen or file select.
    if (g_dComIfG_gameInfo.play.getPlayer(0) == nullptr) {
        return;
    }
    // Match every toggle against the unmodified input before suppressing anything.
    const u32 held = pad.mButtonFlags;
    const u32 pressed = pad.mPressedButtonFlags;
    for (Toggle& toggle : s_toggles) {
        const int64_t index = toggle.hotkey.value;
        if (index <= 0 || index >= static_cast<int64_t>(kHotkeyCount)) {
            continue;
        }
        const Hotkey& key = kHotkeys[index];
        if ((held & key.hold) == key.hold && (pressed & key.trigger) != 0) {
            toggle.pending = true;
            s_suppressedButtons |= key.trigger;
        }
    }
    pad.mButtonFlags &= ~s_suppressedButtons;
    pad.mPressedButtonFlags &= ~s_suppressedButtons;
}

ModResult initialize() {
    ModResult result;
    if ((result = cfg::registerVar(s_heavyOn)) != MOD_OK ||
        (result = cfg::registerVar(s_heavyHotkey)) != MOD_OK ||
        (result = cfg::registerVar(s_keepProtection)) != MOD_OK ||
        (result = cfg::registerVar(s_saverOn)) != MOD_OK ||
        (result = cfg::registerVar(s_saverHotkey)) != MOD_OK)
    {
        return result;
    }
    if ((result = mods::hook::add_post<CheckMagicArmorHeavy>(onCheckHeavyPost)) != MOD_OK ||
        (result = mods::hook::add_pre<CheckMagicArmorNoDamage>(onNoDamagePre)) != MOD_OK ||
        (result = mods::hook::add_post<CheckMagicArmorNoDamage>(onNoDamagePost)) != MOD_OK ||
        (result = mods::hook::add_post<PadRead>(onPadReadPost)) != MOD_OK)
    {
        mods::log::error("magic armor: failed to install hooks ({})", static_cast<int>(result));
        return result;
    }
    return MOD_OK;
}

void update() {
    for (Toggle& toggle : s_toggles) {
        if (!toggle.pending) {
            continue;
        }
        toggle.pending = false;
        cfg::set(toggle.on, !toggle.on.value);
        char body[64];
        snprintf(body, sizeof(body), "%s: %s", toggle.name, toggle.on.value ? "ON" : "OFF");
        ui::toast("Magic Armor", body);
    }
}

void buildUi(UiElementHandle pane) {
    svc_ui->pane_add_section(mod_ctx, pane, "Magic Armor");
    ui::addToggle(pane, s_heavyOn, "Heavy mode",
        "Makes the Magic Armor heavy (like Iron Boots) even when you have rupees. "
        "Only has an effect while the Magic Armor is equipped.");
    ui::addDropdown(pane, s_heavyHotkey, "Heavy mode button",
        "Button combo that toggles heavy mode in-game. The last button of the combo is not passed "
        "to the game.",
        kHotkeyLabels, kHotkeyCount);
    ui::addToggle(pane, s_keepProtection, "Keep protection while heavy",
        "ON: the armor still blocks damage (using rupees) while heavy.<br/>"
        "OFF: vanilla rule, the armor stops protecting while heavy.");
    ui::addToggle(pane, s_saverOn, "Rupee saver",
        "Turns off the armor's damage protection and stops all rupee drain. Link takes normal "
        "damage. Overrides \"Keep protection while heavy\".");
    ui::addDropdown(pane, s_saverHotkey, "Rupee saver button",
        "Button combo that toggles the rupee saver in-game. The last button of the combo is not "
        "passed to the game.",
        kHotkeyLabels, kHotkeyCount);
}

}  // namespace

const Feature features::magicArmor{"Magic Armor", initialize, update, buildUi};
