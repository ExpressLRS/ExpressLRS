#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <c_types.h>

#define DRAM_ATTR
#define DMA_ATTR WORD_ALIGNED_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))

#define _DISABLE_IRQ()
#define _ENABLE_IRQ()

#endif /* PLATFORM_H_ */
