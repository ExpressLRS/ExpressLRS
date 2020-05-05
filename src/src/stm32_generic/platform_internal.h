#ifndef PLATFORM_H_
#define PLATFORM_H_

#define ICACHE_RAM_ATTR
#define DRAM_ATTR
#define DMA_ATTR WORD_ALIGNED_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(32)))

#endif /* PLATFORM_H_ */
