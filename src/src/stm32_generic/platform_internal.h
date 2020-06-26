#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "irq.h"

#define ICACHE_RAM_ATTR
#define DRAM_ATTR
#define DMA_ATTR WORD_ALIGNED_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(32)))

#define _DISABLE_IRQ() irq_disable()
#define _ENABLE_IRQ() irq_enable()
#define _SAVE_IRQ() irq_save()
#define _RESTORE_IRQ(_x) irq_restore(_x)

#endif /* PLATFORM_H_ */
