#pragma once

#include "common.h"
#include "device.h"
#include "lua.h"
#include "config.h"
#include "POWERMGNT.h"
#include "LBT.h"

// Common functions
void luadevGeneratePowerOpts(luaItem_selection *luaPower);

// Common Lua storage (mutable)
extern char strPowerLevels[];

// Common Lua storate (constant)
extern const char STR_EMPTYSPACE[];
extern const char STR_LUA_PACKETRATES[];
