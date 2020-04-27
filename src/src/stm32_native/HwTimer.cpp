#include "HwTimer.h"
#include "internal.h"
#include "irq.h"

#include <Arduino.h>

HwTimer TxTimer;

/****************************************************************
 * Low level timer code
 ****************************************************************/

#ifdef TIM2
#define TIMx TIM2
#define TIMx_IRQn TIM2_IRQn
#define HAVE_TIMER_32BIT 1
#define TIMx_IRQx_FUNC TIM2_IRQHandler
#else
#define TIMx TIM3
#define TIMx_IRQn TIM3_IRQn
#define HAVE_TIMER_32BIT 0
#define TIMx_IRQx_FUNC TIM3_IRQHandler
#endif
// originally TIM1;

static inline uint32_t timer_counter_get(void)
{
    return TIMx->CNT;
}

static inline void timer_counter_set(uint32_t cnt)
{
    TIMx->CNT = cnt;
}

static inline void timer_set(uint32_t next)
{
    TIMx->CCR1 = next;
    TIMx->SR = 0;
}

/****************************************************************
 * Setup and irqs
 ****************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif
    // Hardware timer IRQ handler - dispatch software timers
    void TIMx_IRQx_FUNC(void)
    {
        irqstatus_t flag = irq_save();
        TxTimer.callback();
        irq_restore(flag);
    }
#ifdef __cplusplus
}
#endif

void timer_enable(void)
{
    TIMx->CR1 = TIM_CR1_CEN;
}
void timer_disable(void)
{
    TIMx->CR1 = 0;
}

static void timer_init(void)
{
    irqstatus_t flag = irq_save();
    enable_pclock((uint32_t)TIMx);
    TIMx->CNT = 0;
    TIMx->DIER = TIM_DIER_CC1IE;
    TIMx->CCER = TIM_CCER_CC1E;
    TIMx->PSC = get_pclock_frequency((uint32_t)TIMx) / 1000000; // Prescaler to us
    NVIC_SetPriority(TIMx_IRQn, 2);
    NVIC_EnableIRQ(TIMx_IRQn);
    //TIMx->CR1 = TIM_CR1_CEN;
    irq_restore(flag);
}

/****************************************************************
 * Public
 ****************************************************************/
void HwTimer::init()
{
    timer_disable();
    timer_init();
    //timer_enable();
}

void HwTimer::start()
{
    timer_set(HWtimerInterval);
    timer_enable();
}

void HwTimer::stop()
{
    timer_disable();
}

void HwTimer::pause()
{
    timer_disable();
}

void HwTimer::reset(int32_t offset)
{
    if (running)
    {
        timer_counter_set(0);
        timer_set(HWtimerInterval - offset);
    }
}

void HwTimer::setTime(uint32_t time)
{
    if (!time)
        time = HWtimerInterval;
    timer_set(time);
}
