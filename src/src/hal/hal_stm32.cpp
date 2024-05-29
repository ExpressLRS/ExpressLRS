
#if defined(PLATFORM_STM32) && !defined(TARGET_TX_FLYSKY_IRM301)

#include <stm32_def.h>
#include <string.h>
#include <stdint.h>


void __attribute__((used)) copy_functions_to_ram(void)
{
    /* Copy RAM_CODE section into RAM.
     * NOTE: vectors are the first and then other code.
     *       Check variants/ldscript_gen.ld file.
     */
    extern uint8_t ram_code_start;
    extern uint8_t ram_code_end;
    extern uint8_t ram_code;
    memcpy(&ram_code_start, &ram_code, (size_t) (&ram_code_end - &ram_code_start));
}

void __attribute__((constructor(102))) __attribute__((used)) init_vectors(void)
{
    copy_functions_to_ram();

    /** Reset vector location
     *
     * g_pfnVectors variable comes from the system startup files and
     * must always point to beginning of the vectors table where
     * stack pointer is the first word and second is the reset vector.
     * Vector table is automatically aligned by the linker command file.
     */
    extern uint32_t g_pfnVectors;
    SCB->VTOR = (__IO uint32_t) &g_pfnVectors;
}

extern "C"
void __attribute__((used)) initVariant(void)
{
    /* This can be used to run some init before \ref setup() */
}

#endif // PLATFORM_STM32
