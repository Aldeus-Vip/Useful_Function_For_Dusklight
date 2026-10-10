#include "config.hpp"

#include <vector>

namespace cfg {

namespace {
std::vector<BoolVar*> s_boolVars;
std::vector<IntVar*> s_intVars;

void refresh(BoolVar& var) {
    bool value = var.defaultValue;
    if (svc_config->get_bool(mod_ctx, var.handle, &value) == MOD_OK) {
        var.value = value;
    }
}

void refresh(IntVar& var) {
    int64_t value = var.defaultValue;
    if (svc_config->get_int(mod_ctx, var.handle, &value) == MOD_OK) {
        var.value = value;
    }
}
}  // namespace

ModResult registerVar(BoolVar& var) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = var.name;
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = var.defaultValue;
    ModResult result = svc_config->register_var(mod_ctx, &desc, &var.handle);
    if (result == MOD_OK) {
        refresh(var);
        s_boolVars.push_back(&var);
    }
    return result;
}

ModResult registerVar(IntVar& var) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = var.name;
    desc.type = CONFIG_VAR_INT;
    desc.default_int = var.defaultValue;
    ModResult result = svc_config->register_var(mod_ctx, &desc, &var.handle);
    if (result == MOD_OK) {
        refresh(var);
        s_intVars.push_back(&var);
    }
    return result;
}

void refreshAll() {
    for (BoolVar* var : s_boolVars) {
        refresh(*var);
    }
    for (IntVar* var : s_intVars) {
        refresh(*var);
    }
}

void set(BoolVar& var, bool value) {
    if (svc_config->set_bool(mod_ctx, var.handle, value) == MOD_OK) {
        var.value = value;
    }
}

}  // namespace cfg
