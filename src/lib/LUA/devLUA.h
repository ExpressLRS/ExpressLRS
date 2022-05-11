#pragma once

#include "device.h"

extern device_t LUA_device;

#if defined(TARGET_TX)
void luadevUpdateFolderNames();
#endif