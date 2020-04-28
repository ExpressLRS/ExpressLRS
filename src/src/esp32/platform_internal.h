#ifndef __PLATFORM_H_
#define __PLATFORM_H_

#include <esp_attr.h>

#if !defined(ICACHE_RAM_ATTR)
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif // !defined(ICACHE_RAM_ATTR)

#endif /* __PLATFORM_H_ */
