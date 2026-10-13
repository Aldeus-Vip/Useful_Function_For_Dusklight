// Magic Armor toggles, each switchable with a button combo:
//
// - Heavy mode: daAlink_c::checkMagicArmorHeavy() (src/d/actor/d_a_alink.cpp) decides whether the
//   armor acts like Iron Boots. A post-hook forces it to TRUE.
// - Rupee saver: daAlink_c::checkMagicArmorNoDamage() gates both the armor's damage protection and
//   all of its rupee drain (the passive drain in daAlink_c::execute and the per-hit cost in
//   daAlink_c::setDamagePoint). A post-hook forces it to FALSE, so Link takes normal damage and
//   no rupees are spent.
// - Breathe while heavy: daAlink_c::checkOxygenTimer() (src/d/actor/d_a_alink_swim.inc) hides the
//   oxygen gauge for the Zora Armor by calling offOxygenTimer(). A pre-hook does the same while
//   the Magic Armor is heavy and skips the original.
// - Look and sound: the mod drives the armor's brk animation and sounds from the actual state
//   instead of the rupee count (see "Look and sound" below).

#include "../config.hpp"
#include "../feature.hpp"
#include "../ui_helpers.hpp"

#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_controller_pad.h"
#include "Z2AudioLib/Z2SeMgr.h"

#include <cstdio>

namespace {

cfg::BoolVar s_heavyOn{"armorHeavyOn", false};
cfg::IntVar s_heavyHotkey{"armorHeavyHotkey", 1};  // R + A
cfg::BoolVar s_keepProtection{"armorKeepProtection", true};

cfg::BoolVar s_breatheWhileHeavy{"armorBreatheWhileHeavy", true};

cfg::BoolVar s_showToast{"armorShowToast", false};

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
    // Never force heavy on horseback (or on a boar): it throws Link off.
    if (isWearingMagicArmor(link) && !link->checkReinRide()) {
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

DEFINE_HOOK(&daAlink_c::checkOxygenTimer, CheckOxygenTimer);

HookAction onCheckOxygenTimerPre(ModContext*, void* args, void*, void*) {
    if (!s_breatheWhileHeavy.value) {
        return HOOK_CONTINUE;
    }
    auto link = mods::arg<daAlink_c*>(args, 0);
    // Goes through the heavy hook above, so both forced and vanilla (0 rupees) heavy count.
    if (!link->checkMagicArmorHeavy()) {
        return HOOK_CONTINUE;
    }
    link->offOxygenTimer();
    return HOOK_SKIP_ORIGINAL;
}

// ---------------------------------------------------------------------------------------------
// Look and sound
//
// Vanilla (daAlink_c::execute) switches the armor's brk animation by rupee count, using
// field_0x2fd7 as "current brk": rupees 0 -> power_down (dark) + TURNOFF sound, rupees back ->
// power_up_a (glow while recovering, last frame = normal look) + RECOVER sound.
//
// We take this over and derive it from the real state instead:
//   heavy                 -> dark (power_down)
//   protected (no damage) -> power_up_a looped back and forth, so the recover glow stays on
//   otherwise             -> normal look (last frame of power_up_a)
// Sounds play on changes: heavy ON / protection OFF -> TURNOFF, heavy OFF / protection ON ->
// RECOVER.
//
// Interplay with the vanilla block:
// - Before execute we keep field_0x2fd7 in line with the rupee count, so the vanilla block only
//   fires when the rupee count itself crosses 0 (instead of every frame against our look).
// - setMagicArmorBrk is post-hooked to know which brk is really active, whoever set it.
// - While we track the armor, its two sounds are only allowed from us (no double sounds when
//   the vanilla block does fire).

enum class ArmorLook { Normal, Glow, Dark };

constexpr int kBrkPowerDown = 0;
constexpr int kBrkPowerUp = 1;
// Z2CreatureLink link states the vanilla code sets for the armor (affects Link's sounds).
constexpr u8 kLinkStateArmorOff = 5;
constexpr u8 kLinkStateArmorOn = 4;

struct LookState {
    bool tracking = false;
    int activeBrk = -1;         // last setMagicArmorBrk status, from the hook below
    bool playingOwnSound = false;
    J3DModel* model = nullptr;  // detects the game rebuilding Link's model (and its brk)
    bool heavy = false;
    bool protectedNow = false;
    ArmorLook look = ArmorLook::Normal;
    int glowFrame = 0;
};
LookState s_look;

void setBrkFrame(daAlink_c* link, f32 frame) {
    link->mMagicArmorBodyBrk->setFrame(frame);
    link->mMagicArmorHeadBrk->setFrame(frame);
}

f32 brkFrameMax(daAlink_c* link) {
    return static_cast<f32>(link->mMagicArmorBodyBrk->getFrameMax());
}

// animate: play the transition (dark fading in / recover glow) instead of jumping to the end.
void applyLook(daAlink_c* link, ArmorLook look, bool animate) {
    switch (look) {
    case ArmorLook::Dark:
        link->setMagicArmorBrk(kBrkPowerDown);
        if (!animate) {
            setBrkFrame(link, brkFrameMax(link));
        }
        break;
    case ArmorLook::Glow:
        if (s_look.activeBrk != kBrkPowerUp) {
            link->setMagicArmorBrk(kBrkPowerUp);
        }
        s_look.glowFrame = 0;
        break;
    case ArmorLook::Normal:
        if (s_look.activeBrk != kBrkPowerUp || (s_look.look == ArmorLook::Dark && animate)) {
            link->setMagicArmorBrk(kBrkPowerUp);  // plays the recover glow once, ends normal
        }
        if (!animate || s_look.look == ArmorLook::Glow) {
            setBrkFrame(link, brkFrameMax(link));
        }
        break;
    }
    s_look.look = look;
}

// Back-and-forth over power_up_a so the glow loops without jumping.
void updateGlow(daAlink_c* link) {
    const int max = static_cast<int>(brkFrameMax(link));
    if (max <= 0) {
        return;
    }
    const int period = max * 2;
    s_look.glowFrame = (s_look.glowFrame + 1) % period;
    const int frame = s_look.glowFrame <= max ? s_look.glowFrame : period - s_look.glowFrame;
    setBrkFrame(link, static_cast<f32>(frame));
}

void playArmorSound(daAlink_c* link, bool powerOn) {
    s_look.playingOwnSound = true;
    link->seStartOnlyReverb(powerOn ? Z2SE_AL_M_ARMER_RECOVER : Z2SE_AL_M_ARMER_TURNOFF);
    s_look.playingOwnSound = false;
}

DEFINE_HOOK(&daAlink_c::setMagicArmorBrk, SetMagicArmorBrk);

void onSetMagicArmorBrkPost(ModContext*, void* args, void*, void*) {
    s_look.activeBrk = mods::arg<int>(args, 1);
}

// Virtual, so hooked by name.
DEFINE_HOOK_SYMBOL("daAlink_c::seStartOnlyReverb", void(daAlink_c*, u32), SeStartOnlyReverb);

HookAction onSeStartOnlyReverbPre(ModContext*, void* args, void*, void*) {
    const u32 sound = mods::arg<u32>(args, 1);
    if (s_look.tracking && !s_look.playingOwnSound &&
        (sound == Z2SE_AL_M_ARMER_RECOVER || sound == Z2SE_AL_M_ARMER_TURNOFF))
    {
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&daAlink_c::execute, LinkExecute);

HookAction onLinkExecutePre(ModContext*, void* args, void*, void*) {
    auto link = mods::arg<daAlink_c*>(args, 0);
    if (s_look.tracking && isWearingMagicArmor(link)) {
        const u16 rupees = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
        link->field_0x2fd7 = rupees != 0 ? kBrkPowerUp : kBrkPowerDown;
    }
    return HOOK_CONTINUE;
}

void onLinkExecutePost(ModContext*, void* args, void*, void*) {
    auto link = mods::arg<daAlink_c*>(args, 0);
    if (!isWearingMagicArmor(link) || link->mClothesChangeWaitTimer != 0 ||
        link->mMagicArmorBodyBrk == nullptr || link->mMagicArmorHeadBrk == nullptr)
    {
        s_look.tracking = false;
        return;
    }

    const bool heavy = link->checkMagicArmorHeavy();
    const bool protectedNow = link->checkMagicArmorNoDamage();
    const ArmorLook look = heavy ? ArmorLook::Dark : protectedNow ? ArmorLook::Glow : ArmorLook::Normal;

    if (!s_look.tracking || s_look.model != link->mpLinkModel) {
        // Just put the armor on, or the game rebuilt the model: match silently.
        s_look.tracking = true;
        s_look.model = link->mpLinkModel;
        s_look.heavy = heavy;
        s_look.protectedNow = protectedNow;
        applyLook(link, look, false);
        link->mZ2Link.setLinkState(heavy ? kLinkStateArmorOff : kLinkStateArmorOn);
    } else if (heavy != s_look.heavy || protectedNow != s_look.protectedNow) {
        bool powerOn;
        if (heavy != s_look.heavy) {
            powerOn = !heavy;
        } else {
            powerOn = protectedNow;
        }
        playArmorSound(link, powerOn);
        link->mZ2Link.setLinkState(heavy ? kLinkStateArmorOff : kLinkStateArmorOn);
        s_look.heavy = heavy;
        s_look.protectedNow = protectedNow;
        if (look != s_look.look) {
            applyLook(link, look, true);
        }
    } else if (look == ArmorLook::Dark && s_look.activeBrk != kBrkPowerDown) {
        // The vanilla block switched the brk under us (rupee count crossed 0): put it back.
        applyLook(link, look, false);
    } else if (look != ArmorLook::Dark && s_look.activeBrk != kBrkPowerUp) {
        applyLook(link, look, false);
    }

    if (s_look.look == ArmorLook::Glow) {
        updateGlow(link);
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

    // Only react while Link wears the Magic Armor and is not riding Epona (or a boar), so the
    // combo keeps its normal meaning everywhere else (e.g. R + A dismounts from the horse). This
    // also excludes the title screen and file select, where there is no player actor.
    auto link = static_cast<daAlink_c*>(g_dComIfG_gameInfo.play.getPlayer(0));
    if (link == nullptr || !isWearingMagicArmor(link) || link->checkReinRide()) {
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
        (result = cfg::registerVar(s_breatheWhileHeavy)) != MOD_OK ||
        (result = cfg::registerVar(s_showToast)) != MOD_OK ||
        (result = cfg::registerVar(s_saverOn)) != MOD_OK ||
        (result = cfg::registerVar(s_saverHotkey)) != MOD_OK)
    {
        return result;
    }
    if ((result = mods::hook::add_post<CheckMagicArmorHeavy>(onCheckHeavyPost)) != MOD_OK ||
        (result = mods::hook::add_pre<CheckMagicArmorNoDamage>(onNoDamagePre)) != MOD_OK ||
        (result = mods::hook::add_post<CheckMagicArmorNoDamage>(onNoDamagePost)) != MOD_OK ||
        (result = mods::hook::add_pre<CheckOxygenTimer>(onCheckOxygenTimerPre)) != MOD_OK ||
        (result = mods::hook::add_post<SetMagicArmorBrk>(onSetMagicArmorBrkPost)) != MOD_OK ||
        (result = mods::hook::add_pre<SeStartOnlyReverb>(onSeStartOnlyReverbPre)) != MOD_OK ||
        (result = mods::hook::add_pre<LinkExecute>(onLinkExecutePre)) != MOD_OK ||
        (result = mods::hook::add_post<LinkExecute>(onLinkExecutePost)) != MOD_OK ||
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
        if (!s_showToast.value) {
            continue;  // the armor's look and sound already show the change
        }
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
        "Button combo that toggles heavy mode in-game. Only works while the Magic Armor is "
        "equipped and Link is not riding. The last button of the combo is not passed to the game.",
        kHotkeyLabels, kHotkeyCount);
    ui::addToggle(pane, s_keepProtection, "Keep protection while heavy",
        "ON: the armor still blocks damage (using rupees) while heavy.<br/>"
        "OFF: vanilla rule, the armor stops protecting while heavy.");
    ui::addToggle(pane, s_showToast, "Show notifications",
        "Show a notification when a mode is toggled. The armor's look and sound change either way.");
    ui::addToggle(pane, s_breatheWhileHeavy, "Breathe underwater while heavy",
        "Hides the oxygen gauge while the Magic Armor is heavy, like the Zora Armor.");
    ui::addToggle(pane, s_saverOn, "Rupee saver",
        "Turns off the armor's damage protection and stops all rupee drain. Link takes normal "
        "damage. Overrides \"Keep protection while heavy\".");
    ui::addDropdown(pane, s_saverHotkey, "Rupee saver button",
        "Button combo that toggles the rupee saver in-game. Only works while the Magic Armor is "
        "equipped and Link is not riding. The last button of the combo is not passed to the game.",
        kHotkeyLabels, kHotkeyCount);
}

}  // namespace

const Feature features::magicArmor{"Magic Armor", initialize, update, buildUi};
