// Definitions for irq enable/disable on ARM Cortex-M processors
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
// https://github.com/KevinOConnor/klipper
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __GENERIC_IRQ_H
#define __GENERIC_IRQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef unsigned long irqstatus_t;

    void irq_disable(void);
    void irq_enable(void);
    irqstatus_t irq_save(void);
    void irq_restore(irqstatus_t flag);
    void irq_wait(void);
    void irq_poll(void);
#ifdef __cplusplus
}
#endif

#endif // irq.h
