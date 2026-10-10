#include "ui_helpers.hpp"

namespace ui {

void addToggle(UiElementHandle pane, const cfg::BoolVar& var, const char* label, const char* help) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE;
    desc.label = label;
    desc.help_rml = help;
    desc.binding = UI_BINDING_CONFIG_VAR;
    desc.config_var = var.handle;
    svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr);
}

void addPercent(UiElementHandle pane, const cfg::IntVar& var, const char* label, const char* help,
    int64_t min, int64_t max, int64_t step) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_NUMBER;
    desc.label = label;
    desc.help_rml = help;
    desc.binding = UI_BINDING_CONFIG_VAR;
    desc.config_var = var.handle;
    desc.min = min;
    desc.max = max;
    desc.step = step;
    desc.suffix = "%";
    svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr);
}

void addDropdown(UiElementHandle pane, const cfg::IntVar& var, const char* label, const char* help,
    const char* const* options, size_t optionCount) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_DROPDOWN;
    desc.label = label;
    desc.help_rml = help;
    desc.binding = UI_BINDING_CONFIG_VAR;
    desc.config_var = var.handle;
    desc.options = options;
    desc.option_count = optionCount;
    svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr);
}

void toast(const char* title, const char* body) {
    UiToastDesc desc = UI_TOAST_DESC_INIT;
    desc.title_rml = title;
    desc.body_rml = body;
    desc.duration_ms = 2000;
    svc_ui->push_toast(mod_ctx, &desc);
}

}  // namespace ui
