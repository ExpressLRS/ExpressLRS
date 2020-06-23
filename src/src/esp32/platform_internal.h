#ifndef __PLATFORM_H_
#define __PLATFORM_H_

#include <esp_attr.h>

#if !defined(ICACHE_RAM_ATTR)
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif // !defined(ICACHE_RAM_ATTR)

#define _DISABLE_IRQ()
#define _ENABLE_IRQ()

#endif /* __PLATFORM_H_ */
