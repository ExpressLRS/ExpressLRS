#pragma once

#include "CRSFParameters.h"
#include "common.h"

// Common functions
void luadevGeneratePowerOpts(selectionParameter *luaPower);

// Common Lua storage (mutable)
extern char strPowerLevels[];

// Common Lua storate (constant)
constexpr char STR_EMPTYSPACE[1] = {};
extern const char STR_LUA_PACKETRATES[];
