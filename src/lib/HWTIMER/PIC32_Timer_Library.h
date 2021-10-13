/****************************************************************************************/
/*																											*/
/*	SimpleTimers.h																					*/
/*                                                                                                     		*/
/*	This library provides a simple interface to control the timers on				*/
/*		the PIC32, as well as timer interrupts and PWM								*/
/*																											*/
/***************************************************************************************/
/*	Authors: 	Thomas Kappenman 															*/
/*	Copyright 2014, Digilent Inc.																*/
/***************************************************************************************/
/*  Module Description: 																			*/
/*																											*/
/*	This library provides functions that set up the timers on the PIC32	    	*/
/*	microcontroller. It also provides functions to link timer interrupts to 		*/
/* other functions, and provides an easy way to set up a basic PWM	    	*/
/* signal using output compare modules.													*/
/*																											*/
/**************************************************************************************/
/*  Revision History:																				*/
/*																											*/
/*		8/7/2014(TommyK): Created															*/
/*																											*/
/***************************************************************************************/
/*																											*/
/*		This module was adapted from the PIC32 core library WInterrupts.c	*/
/*		by Mark Sproul and Gene Apperson.												*/
/*																											*/
/***************************************************************************************/

/*
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SIMPLETIMERS_h
#define SIMPLETIMERS_h

#include <p32xxxx.h>
#include "pins_arduino.h"
#include "wiring_private.h"


//#include "WConstants.h"
//#include <sys/attribs.h>
//#include "p32_defs.h"

//Symbols for referring to Timer interrupts
#define TIMER1 0
#define TIMER2	1
#define TIMER3	2
#define TIMER4	3
#define TIMER5	4
#define TIMER23	5
#define TIMER45	6

//Symbols for output compare modules
#define OC1	1
#define OC2	2
#define OC3	3
#define OC4	4
#define OC5	5

//Max timer counter values
#define MAX16BIT 65536
#define MAX32BIT 4294967296

#define OC_ON                      (1 << _OC1CON_ON_POSITION)
#define OC_OFF                     (0)

// Stop-in-idle control - values are mutually exclusive
#define OC_IDLE_STOP               (1 << _OC1CON_OCSIDL_POSITION)    /* stop in idle mode */
#define OC_IDLE_CON                (0)          /* continue operation in idle mode */

// 16/32 bit mode - values are mutually exclusive
#define OC_TIMER_MODE32             (1 << _OC1CON_OC32_POSITION)    /* use 32 bit mode */
#define OC_TIMER_MODE16             (0)         /* use 16 bit mode */

// Timer select - values are mutually exclusive
#define OC_TIMER3_SRC               (1 << _OC1CON_OCTSEL_POSITION)   /* Timer3 is the clock source */
#define OC_TIMER2_SRC               (0)         /* Timer2 is the clock source */

// Operation mode select - values are mutually exclusive
#define OC_PWM_FAULT_PIN_ENABLE  	(7 << _OC1CON_OCM_POSITION)		/* PWM Mode on OCx, fault pin enabled */
#define OC_PWM_FAULT_PIN_DISABLE    (6 << _OC1CON_OCM_POSITION)     /* PWM Mode on OCx, fault pin disabled */
#define OC_CONTINUE_PULSE           (5 << _OC1CON_OCM_POSITION)     /* Generates Continuous Output pulse on OCx Pin */
#define OC_SINGLE_PULSE             (4 << _OC1CON_OCM_POSITION)     /* Generates Single Output pulse on OCx Pin */
#define OC_TOGGLE_PULSE             (3 << _OC1CON_OCM_POSITION)     /* Compare1 toggels  OCx pin*/
#define OC_HIGH_LOW                 (2 << _OC1CON_OCM_POSITION)     /* Compare1 forces OCx pin Low*/
#define OC_LOW_HIGH                 (1 << _OC1CON_OCM_POSITION)     /* Compare1 forces OCx pin High*/
#define OC_MODE_OFF                 (0 << _OC1CON_OCM_POSITION)     /* OutputCompare x Off*/

#define PS_1_256 0x7 << _T1CON_TCKPS_POSITION
#define PS_1_64	 0x6 << _T1CON_TCKPS_POSITION
#define PS_1_32	 0x5 << _T1CON_TCKPS_POSITION
#define PS_1_16	 0x4 << _T1CON_TCKPS_POSITION
#define PS_1_8	 0x3 << _T1CON_TCKPS_POSITION
#define PS_1_4	 0x2 << _T1CON_TCKPS_POSITION
#define PS_1_2	 0x1 << _T1CON_TCKPS_POSITION
#define PS_1_1	 0x0 << _T1CON_TCKPS_POSITION

#define T_32_BIT_MODE_ON	0x1 << _T2CON_T32_POSITION

#define T_ON	1<< _T1CON_ON_POSITION


// forward references to the ISRs
void __attribute__((interrupt(),nomips16)) Timer1IntHandler(void);
void __attribute__((interrupt(),nomips16)) Timer2IntHandler(void);
void __attribute__((interrupt(),nomips16)) Timer3IntHandler(void);
void __attribute__((interrupt(),nomips16)) Timer4IntHandler(void);
void __attribute__((interrupt(),nomips16)) Timer5IntHandler(void);

//Forward references to library functions
void picStartTimer(uint8_t timerNum, long microseconds);
void picStopTimer(uint8_t timerNum);
void picSetTimerPeriod(uint8_t timerNum, long microseconds);
void picTimerReset(uint8_t timerNum);
void picAttachTimerInterrupt(uint8_t timerNum, void (*userFunc)(void));
void picDetachTimerInterrupt(uint8_t timerNum);
void picDisableTimerInterrupt(uint8_t timerNum);
void picEnableTimerInterrupt(uint8_t timerNum);

void picStartPWM(uint8_t timerNum, uint8_t OCnum, uint8_t dutycycle);
void picStopPWM(uint8_t OCnum);
void picSetDutyCycle(uint8_t OCnum, float dutycycle);




#endif
