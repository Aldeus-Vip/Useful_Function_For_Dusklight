#pragma once

#include "config.hpp"

#include "mods/svc/ui.h"

namespace ui {

void addToggle(UiElementHandle pane, const cfg::BoolVar& var, const char* label, const char* help);

// Percentage stepper bound to an int var (e.g. 200 = x2.0).
void addPercent(UiElementHandle pane, const cfg::IntVar& var, const char* label, const char* help,
    int64_t min, int64_t max, int64_t step);

void addDropdown(UiElementHandle pane, const cfg::IntVar& var, const char* label, const char* help,
    const char* const* options, size_t optionCount);

void toast(const char* title, const char* body);

}  // namespace ui
