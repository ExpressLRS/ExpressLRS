#include "LR2021.h"
#include "LR2021PRAM.h"
#include "LR2021_hal.h"
#include "LR2021_Regs.h"
#include "SPIEx.h"

extern LR2021Hal hal;

#define LR20XX_PRAM_BASE_ADDRESS ( 0x801000 )

uint16_t LR2021Driver::WriteRegMem32(const uint32_t address, const uint32_t* buffer, const uint8_t length, const SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[5 + 32 * sizeof(uint32_t)] {
        (uint8_t)(LR2021_REGMEM_WRITE_REGMEM32_OC >> 8),
        (uint8_t)(LR2021_REGMEM_WRITE_REGMEM32_OC),
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)(address),
    };

    for(uint16_t index = 0; index < length; index++)
    {
        uint8_t* cdata_local = &OutBuffer[5 + index * sizeof( uint32_t )];

        cdata_local[0] = ( uint8_t ) ( buffer[index] >> 24 );
        cdata_local[1] = ( uint8_t ) ( buffer[index] >> 16 );
        cdata_local[2] = ( uint8_t ) ( buffer[index] >> 8 );
        cdata_local[3] = ( uint8_t ) ( buffer[index] >> 0 );
    }

    const uint32_t size = length * sizeof(uint32_t) + 5;

    hal.WaitOnBusy(radioNumber);
    SPIEx.read(radioNumber, OutBuffer, size);
    while (!hal.WaitOnBusy(radioNumber))
    {
        delay(1);
    }
    return OutBuffer[0] << 8 | OutBuffer[1];
}

uint16_t LR2021Driver::LoadPatchRAM(const SX12XX_Radio_Number_t radioNumber)
{
    // MAX SPI buffer size is 64 bytes, so we do 12 x 32-bit words + command overhard
    constexpr uint8_t PRAM_CHUNK_SIZE = 12;

    // The PRAM is written in n chunks of the size above
    constexpr uint32_t n_chunks = sizeof(pram_lr2021) / (PRAM_CHUNK_SIZE * sizeof(uint32_t));

    // Write all blocks of chunk words
    for( uint32_t index_32_word_block = 0; index_32_word_block < n_chunks; index_32_word_block++ )
    {
        const uint32_t *local_buffer  = pram_lr2021 + ( index_32_word_block * PRAM_CHUNK_SIZE );
        const uint32_t local_address = LR20XX_PRAM_BASE_ADDRESS + ( index_32_word_block * PRAM_CHUNK_SIZE * sizeof(uint32_t) );
        const uint16_t ret = WriteRegMem32(local_address, local_buffer, PRAM_CHUNK_SIZE, radioNumber);
        if ((ret & 0x400) != 0x400) return ret;
    }

    // Check if there are remaining words to write and if so, write it in a single call
    constexpr uint8_t n_remaining_words = (sizeof(pram_lr2021) / sizeof(uint32_t)) - (n_chunks * PRAM_CHUNK_SIZE);
    if( n_remaining_words > 0 )
    {
        const uint32_t *local_buffer  = pram_lr2021 + ( n_chunks * PRAM_CHUNK_SIZE );
        constexpr uint32_t local_address = LR20XX_PRAM_BASE_ADDRESS + ( n_chunks * PRAM_CHUNK_SIZE * sizeof(uint32_t) );
        const uint16_t ret = WriteRegMem32(local_address, local_buffer, n_remaining_words, radioNumber);
        if ((ret & 0x400) != 0x400) return ret;
    }

    constexpr uint8_t enable = 0;
    return hal.WriteCommand(LR2021_PATCH_ENABLE_PRAM_OC, &enable, 1, radioNumber, 10000);
}
