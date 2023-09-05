#include "targets.h"
#include <SPI.h>

/**
 * @brief An extension to the platform SPI class that provides some performance enhancements.
 *
 * 1. The provided data buffer is expected to be word-aligned and word-padded, this allow the
 *    data to be copied to the SPI fifo in word-sized chunks.
 * 2. The wait for non-busy is removed from the end of the write call so the processor can
 *    continue to do work while the SPI module is pumping the data from the FIFO to the external
 *    device.
 */
class SPIExClass : public SPIClass
{
public:
#if defined(PLATFORM_ESP32)
    explicit SPIExClass(uint8_t spi_bus=HSPI) : SPIClass(spi_bus) {}
#else
    explicit SPIExClass() : SPIClass() {}
#endif

    /**
     * @brief Perform an SPI read operation on the SPI bus.
     *
     * If the SPI bus is busy the processor waits for the current operation to complete before starting
     * this operation.
     * Once the SPI bus is free, the data is copied to the SPI fifo and the processor waits for the operation
     * to complete then copies the incoming data back into the provided data buffer.
     *
     * @param cs_mask mask of CS pins to enable for this operation
     * @param data word-aligned and padded data buffer
     * @param size the number of bytes to be read into in the data buffer
     */
    void inline ICACHE_RAM_ATTR read(uint8_t cs_mask, uint8_t *data, uint32_t size) { _transfer(cs_mask, data, size, true); }

    /**
     * @brief Perform an SPI write operation on the SPI bus.
     *
     * If the SPI bus is busy the processor waits for the current operation to complete before starting
     * this operation.
     * One the SPI bus is not busy, it copies the data to the SPI fifo then returns without waiting for
     * the operation to complete allowing the processor to do other work.
     *
     * @param cs_mask mask of CS pins to enable for this operation
     * @param data word-aligned and padded data buffer
     * @param size the number of bytes to be written to the SPI device
     */
    void inline ICACHE_RAM_ATTR write(uint8_t cs_mask, uint8_t * data, uint32_t size) { _transfer(cs_mask, data, size, false); }

private:
    void _transfer(uint8_t cs_mask, uint8_t *data, uint32_t size, bool reading);
};

extern SPIExClass SPIEx;
