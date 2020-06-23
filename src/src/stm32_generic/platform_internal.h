#ifndef PLATFORM_H_
#define PLATFORM_H_

#define ICACHE_RAM_ATTR
#define DRAM_ATTR
#define DMA_ATTR WORD_ALIGNED_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(32)))

#define _DISABLE_IRQ() __disable_irq()
#define _ENABLE_IRQ() __enable_irq()

#endif /* PLATFORM_H_ */
