#pragma once

#include "mods/svc/config.h"

#include <cstdint>

// Thin wrappers over ConfigService. Values are persisted in the user's config.json under
// mod.<escaped mod id>.<name>.
namespace cfg {

struct BoolVar {
    const char* name;
    bool defaultValue;
    ConfigVarHandle handle = 0;
    bool value = false;
};

struct IntVar {
    const char* name;
    int64_t defaultValue;
    ConfigVarHandle handle = 0;
    int64_t value = 0;
};

ModResult registerVar(BoolVar& var);
ModResult registerVar(IntVar& var);

// Re-reads every registered var's cached value. Called once per frame from mod_update so that
// hooks can read `value` without calling into the service.
void refreshAll();

void set(BoolVar& var, bool value);

}  // namespace cfg
