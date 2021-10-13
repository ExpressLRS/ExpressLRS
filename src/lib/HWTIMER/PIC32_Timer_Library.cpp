/****************************************************************************************/
/*																											*/
/*	SimpleTimers.cpp																				*/
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

#ifndef SIMPLETIMERS_cpp
#define SIMPLETIMERS_cpp

#include "PIC32_Timer_Library.h"


//Global ISR function array
volatile static voidFuncPtr intFunc[5];

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	startTimer()
**
**	Parameters:
**		timerNum:	The timer to initialize <TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45> 
**		microseconds:	The period, in microseconds, to set the timer to.
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Starts the specified timer with a period specified by the user in microseconds.
**
**	Example:
**		startTimer(TIMER23, 10000000);	Starts a 32 bit timer (TIMER2 & TIMER3) with a period of 10 seconds
*/
void picStartTimer(uint8_t timerNum, long microseconds){

	unsigned long cycles = (F_CPU / 1000000) * microseconds;	//Number of cycles = Cycles per second * Seconds
	if (timerNum < 7){
		switch (timerNum){
			case(TIMER1):
				if(cycles < MAX16BIT)              T1CON=(~T_ON & PS_1_1), PR1=cycles-1, T1CONSET=T_ON;  	// No prescale
				else if((cycles >>= 3) < MAX16BIT) T1CON=(~T_ON & PS_1_8), PR1=cycles-1, T1CONSET=T_ON;      	// Prescale by /8 
				else if((cycles >>= 3) < MAX16BIT) T1CON=(~T_ON & PS_1_64), PR1=cycles-1, T1CONSET=T_ON;   	// Prescale by /64
				else if((cycles >>= 2) < MAX16BIT) T1CON=(~T_ON & PS_1_256), PR1=cycles-1, T1CONSET=T_ON; 	// Prescale by /256
				else        					   T1CON=(~T_ON &  PS_1_256), PR1=MAX16BIT-1, T1CONSET=T_ON; 				// Max period
				break;
			case(TIMER2):
				if(cycles < MAX16BIT)              T2CON=(~T_ON & PS_1_1), PR2=cycles-1, T2CONSET=T_ON;   	// No prescale
				else if((cycles >>= 3) < MAX16BIT) T2CON=(~T_ON & PS_1_8), PR2=cycles-1, T2CONSET=T_ON;         	// Prescale by /8 
				else if((cycles >>= 3) < MAX16BIT) T2CON=(~T_ON & PS_1_64), PR2=cycles-1, T2CONSET=T_ON;   		// Prescale by /64
				else if((cycles >>= 2) < MAX16BIT) T2CON=(~T_ON & PS_1_256), PR2=cycles-1, T2CONSET=T_ON;    	// Prescale by /256
				else        					   T2CON=(~T_ON & PS_1_256), PR2=MAX16BIT-1, T2CONSET=T_ON;	// Max period
				break;
			case(TIMER3):
				if(cycles < MAX16BIT)              T3CON=(~T_ON &  PS_1_1), PR3=cycles-1, T3CONSET=T_ON;    // No prescale
				else if((cycles >>= 3) < MAX16BIT) T3CON=(~T_ON &  PS_1_8), PR3=cycles-1, T3CONSET=T_ON;         // Prescale by /8 
				else if((cycles >>= 3) < MAX16BIT) T3CON=(~T_ON &  PS_1_64), PR3=cycles-1, T3CONSET=T_ON;   	 	// Prescale by /64
				else if((cycles >>= 2) < MAX16BIT) T3CON=(~T_ON &  PS_1_256), PR3=cycles-1, T3CONSET=T_ON;     	// Prescale by /256
				else        					   T3CON=(~T_ON &  PS_1_256), PR3=MAX16BIT-1, T3CONSET=T_ON; 		// Max period
				break;
			case(TIMER4):
				if(cycles < MAX16BIT)             T4CON=(~T_ON & PS_1_1), PR4=cycles-1, T4CONSET=T_ON;           // No prescale
				else if((cycles >>= 3) < MAX16BIT)T4CON=(~T_ON & PS_1_8), PR4=cycles-1, T4CONSET=T_ON;       // Prescale by /8 
				else if((cycles >>= 3) < MAX16BIT)T4CON=(~T_ON & PS_1_64), PR4=cycles-1, T4CONSET=T_ON; 	// Prescale by /64
				else if((cycles >>= 2) < MAX16BIT)T4CON=(~T_ON & PS_1_256), PR4=cycles-1, T4CONSET=T_ON;  // Prescale by /256
				else        					  T4CON=(~T_ON & PS_1_256), PR4=MAX16BIT-1, T4CONSET=T_ON; // Max period
				break;
			case(TIMER5):
				if(cycles < MAX16BIT)             T5CON=(~T_ON & PS_1_1), PR5=cycles-1, T5CONSET=T_ON;             // No prescale
				else if((cycles >>= 3) < MAX16BIT)T5CON=(~T_ON & PS_1_8), PR5=cycles-1, T5CONSET=T_ON;        // Prescale by /8
				else if((cycles >>= 3) < MAX16BIT)T5CON=(~T_ON & PS_1_64), PR5=cycles-1, T5CONSET=T_ON;  	// Prescale by /64
				else if((cycles >>= 2) < MAX16BIT)T5CON=(~T_ON & PS_1_256), PR5=cycles-1, T5CONSET=T_ON;   	// Prescale by /256
				else        					  T5CON=(~T_ON & PS_1_256), PR5=MAX16BIT-1, T5CONSET=T_ON; 	// Max period
				break;
			case(TIMER23):
				if(cycles < MAX32BIT)				T2CON=(~T_ON & T_32_BIT_MODE_ON | PS_1_1), PR2=cycles-1, T2CONSET=T_ON;	//No prescale
				else if((cycles >>= 3) < MAX32BIT)	T2CON=(~T_ON & T_32_BIT_MODE_ON | PS_1_8), PR2=cycles-1, T2CONSET=T_ON;  	//Prescale by /8
				else if((cycles >>= 3) < MAX32BIT)	T2CON=(~T_ON & T_32_BIT_MODE_ON | PS_1_64), PR2=cycles-1, T2CONSET=T_ON; 	//Prescale by /64
				else if((cycles >>= 2) < MAX32BIT)	T2CON=(~T_ON & T_32_BIT_MODE_ON | PS_1_256), PR2=cycles-1, T2CONSET=T_ON;	//Prescale by /256
				else								T2CON=(~T_ON & T_32_BIT_MODE_ON | PS_1_256), PR2=MAX16BIT-1, T2CONSET=T_ON;	//Max period possible
				break;
			case(TIMER45):
				if(cycles < MAX32BIT)				T4CON=(~T_ON &  T_32_BIT_MODE_ON | PS_1_1), PR4=cycles-1, T4CONSET=T_ON;	//No prescale
				else if((cycles >>= 3) < MAX32BIT)	T2CON=(~T_ON &  T_32_BIT_MODE_ON | PS_1_8), PR4=cycles-1, T4CONSET=T_ON;  	//Prescale by /8
				else if((cycles >>= 3) < MAX32BIT)	T2CON=(~T_ON &  T_32_BIT_MODE_ON | PS_1_64), PR4=cycles-1, T4CONSET=T_ON; 	//Prescale by /64
				else if((cycles >>= 2) < MAX32BIT)	T2CON=(~T_ON &  T_32_BIT_MODE_ON | PS_1_256), PR4=cycles-1, T4CONSET=T_ON;	//Prescale by /256
				else								T2CON=(~T_ON &  T_32_BIT_MODE_ON | PS_1_256), PR4=MAX16BIT-1, T4CONSET=T_ON;	//Max period possible
				break;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	stopTimer()
**
**	Parameters:
**		timerNum:	The timer to stop <TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45> 
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Stops the specified timer
**
**	Example:
**		stopTimer(TIMER23)	Stops the 32 bit timer (TIMER2 & TIMER3)
*/
void picStopTimer(uint8_t timerNum){
	if (timerNum < 7){
		switch(timerNum){
			case TIMER1:IEC0CLR = _IEC0_T1IE_MASK, T1CON = 0x0;
				break;
			case TIMER2:IEC0CLR = _IEC0_T2IE_MASK, T2CON = 0x0;
				break;
			case TIMER3:IEC0CLR = _IEC0_T3IE_MASK, T3CON = 0x0;
				break;
			case TIMER4:IEC0CLR = _IEC0_T4IE_MASK, T4CON = 0x0;
				break;
			case TIMER5:IEC0CLR = _IEC0_T5IE_MASK, T5CON = 0x0;
				break;
			case TIMER23:IEC0CLR = _IEC0_T3IE_MASK, T2CON = 0x0,T3CON = 0x0;
				break;
			case TIMER45:IEC0CLR = _IEC0_T5IE_MASK, T4CON = 0x0, T5CON = 0x0;
				break;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	setTimerPeriod()
**
**	Parameters:
**		timerNum:	The timer to set the period of <TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45> 
**
**	Return Value:
**		none
**
**	Errors:
**		At the moment, just calls startTimer(), this also enables the timer. This function is
**		mainly for the name.
**
**  Description:
**		Starts the specified timer with the specified period in microseconds
**
**	Example:
**		startTimer(TIMER4, 100000);	Starts a 16 bit timer (TIMER4) with a period of 100 milliseconds.
*/
void picSetTimerPeriod(uint8_t timerNum, long microseconds){
	picStartTimer(timerNum, microseconds);
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	timerReset()
**
**	Parameters:
**		timerNum:	The timer to reset <TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45> 
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Resets the value of the timer counter to 0.
**
**	Example:
**		timerReset(TIMER23);	Resets the 32 bit timer 2-3 count to 0.
*/
void picTimerReset(uint8_t timerNum){
	switch(timerNum){
		case TIMER1: TMR1=0;
			break;
		case TIMER2: TMR2=0;
			break;
		case TIMER3: TMR3=0;
			break;
		case TIMER4: TMR4=0;
			break;
		case TIMER5: TMR5=0;
			break;
		case TIMER23: TMR2=0;TMR3=0;
			break;
		case TIMER45: TMR4=0;TMR5=0;
			break;
	}
}
/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	attachTimerInterrupt()
**
**	Parameters:
**		timerNum:	The timer interrupt to set <TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45> 
**		userfunc:	The interrupt service routine to jump to when the interrupt is triggered
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Specifies a function as an interrupt service routine for a timer.
**
**	Example:
**		attachTimerInterrupt(TIMER23, toggleLED);	Attaches the 32 bit timer 2-3 to the function toggleLED();
*/
void picAttachTimerInterrupt(uint8_t timerNum, void (*userFunc)(void))
{
    if (timerNum < 7)
    {
		if (timerNum==TIMER23)	timerNum=TIMER3;
		else if (timerNum==TIMER45)	timerNum=TIMER5;
		
        intFunc[timerNum]	=	userFunc;

        switch (timerNum)
        {
            case TIMER1:
                IEC0bits.T1IE     =	0;
                IFS0bits.T1IF     =	0;
				setIntVector(_TIMER_1_VECTOR, (isrFunc) Timer1IntHandler);
                IFS0CLR = _IFS0_T1IF_MASK;
				IPC1CLR = _IPC1_T1IP_MASK, IPC1SET = ((_T1_IPL_IPC) << _IPC1_T1IP_POSITION);
				IPC1CLR = _IPC1_T1IS_MASK, IPC1SET = ((_T1_SPL_IPC) << _IPC1_T1IS_POSITION);
				IEC0SET = (1 << _IEC0_T1IE_POSITION);
                break;

            case TIMER2:
				IEC0bits.T2IE     =	0;
                IFS0bits.T2IF     =	0;
                setIntVector(_TIMER_2_VECTOR, (isrFunc) Timer2IntHandler);
                IFS0CLR = _IFS0_T2IF_MASK;
				IPC2CLR = _IPC2_T2IP_MASK, IPC2SET = ((_T2_IPL_IPC) << _IPC2_T2IP_POSITION);
				IPC2CLR = _IPC2_T2IS_MASK, IPC2SET = ((_T2_SPL_IPC) << _IPC2_T2IS_POSITION);
				IEC0SET = (1 << _IEC0_T2IE_POSITION);
                break;

            case TIMER3:
				IEC0bits.T3IE     =	0;
                IFS0bits.T3IF     =	0;
                setIntVector(_TIMER_3_VECTOR, (isrFunc) Timer3IntHandler);
				IFS0CLR = _IFS0_T3IF_MASK;
				IPC3CLR = _IPC3_T3IP_MASK, IPC3SET = ((_T3_IPL_IPC) << _IPC3_T3IP_POSITION);
				IPC3CLR = _IPC3_T3IS_MASK, IPC3SET = ((_T3_SPL_IPC) << _IPC3_T3IS_POSITION);
				IEC0SET = (1 << _IEC0_T3IE_POSITION);
                break;

            case TIMER4:
				IEC0bits.T4IE     =	0;
                IFS0bits.T4IF     =	0;
                setIntVector(_TIMER_4_VECTOR, (isrFunc) Timer4IntHandler);
				IFS0CLR = _IFS0_T4IF_MASK;
				IPC4CLR = _IPC4_T4IP_MASK, IPC4SET = ((_T4_IPL_IPC) << _IPC4_T4IP_POSITION);
				IPC4CLR = _IPC4_T4IS_MASK, IPC4SET = ((_T4_SPL_IPC) << _IPC4_T4IS_POSITION);
				IEC0SET = (1 << _IEC0_T1IE_POSITION);
                break;

            case TIMER5:
			    IEC0bits.T5IE     =	0;
                IFS0bits.T5IF     =	0;
                setIntVector(_TIMER_5_VECTOR, (isrFunc) Timer5IntHandler);
				IFS0CLR = _IFS0_T5IF_MASK;
				#if defined(__PIC32MX__)
					IPC5CLR = _IPC5_T5IP_MASK, IPC5SET = ((_T5_IPL_IPC) << _IPC5_T5IP_POSITION);
					IPC5CLR = _IPC5_T5IS_MASK, IPC5SET = ((_T5_SPL_IPC) << _IPC5_T5IS_POSITION);
				#else if defined(__PIC32MZ__)
					IPC6CLR = _IPC6_T5IP_MASK, IPC5SET = ((_T5_IPL_IPC) << _IPC6_T5IP_POSITION);
					IPC6CLR = _IPC6_T5IS_MASK, IPC5SET = ((_T5_SPL_IPC) << _IPC6_T5IS_POSITION);
				#endif
				IEC0SET = (1 << _IEC0_T1IE_POSITION);
                break;
				
        }
    }
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	detachTimerInterrupt()
**
**	Parameters:
**		timerNum:	The timer interrupt to detach<TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45>
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Detaches the interrupt from the previously specified ISR and disables the timer interrupt
**
**	Example:
**		detachTimerInterrupt(TIMER23);	Detaches timer 2-3 from its ISR, 
*/
void picDetachTimerInterrupt(uint8_t timerNum)
{
    if (timerNum < 7)
    {
		if (timerNum==TIMER23) timerNum=TIMER3;
		else if (timerNum==TIMER45) timerNum=TIMER5;
        switch (timerNum){
            case TIMER1:
                IEC0bits.T1IE     =	0;
                clearIntVector(_TIMER_1_VECTOR);
                break;
            case TIMER2:
                IEC0bits.T2IE     =	0;
                clearIntVector(_TIMER_2_VECTOR);
                break;
            case (TIMER3):
                IEC0bits.T3IE     =	0;
                clearIntVector(_TIMER_3_VECTOR);
                break;
            case TIMER4:
                IEC0bits.T4IE     =	0;
                clearIntVector(_TIMER_4_VECTOR);
                break;
            case (TIMER5):
                IEC0bits.T5IE     =	0;
                clearIntVector(_TIMER_5_VECTOR);
               break;
		}
        intFunc[timerNum]	=	0;
    }
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	disableTimerInterrupt()
**
**	Parameters:
**		timerNum:	The timer interrupt to disable<TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45>
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Turns off the timer interrupt without detaching the ISR
**
**	Example:
**		disableTimerInterrupt(TIMER4);	Disables timer 4 
*/
void picDisableTimerInterrupt(uint8_t timerNum)
{
    if (timerNum < 7)
    {
		if (timerNum==TIMER23) timerNum=TIMER3;	
		else if (timerNum==TIMER45) timerNum=TIMER5;
        switch (timerNum){
            case TIMER1:
                IEC0bits.T1IE     =	0;
                break;
            case TIMER2:
                IEC0bits.T2IE     =	0;
                break;
            case (TIMER3):
                IEC0bits.T3IE     =	0;
                break;
            case TIMER4:
                IEC0bits.T4IE     =	0;
                break;
            case (TIMER5):
                IEC0bits.T5IE     =	0;
               break;
		}
    }
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	enableTimerInterrupt()
**
**	Parameters:
**		timerNum:	The timer interrupt to enable<TIMER1, TIMER2, TIMER3, TIMER4, TIMER5, TIMER23, TIMER45>
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Turns on the timer interrupt
**
**	Example:
**		enableTimerInterrupt(TIMER4);	Enables timer 4 
*/
void picEnableTimerInterrupt(uint8_t timerNum)
{
    if (timerNum < 7)
    {
		if (timerNum==TIMER23) timerNum=TIMER3;	
		else if (timerNum==TIMER45) timerNum=TIMER5;
        switch (timerNum){
            case TIMER1:
                IEC0bits.T1IE     =	1;
                break;
            case TIMER2:
                IEC0bits.T2IE     =	1;
                break;
            case (TIMER3):
                IEC0bits.T3IE     =	1;
                break;
            case TIMER4:
                IEC0bits.T4IE     =	1;
                break;
            case (TIMER5):
                IEC0bits.T5IE     =	1;
               break;
		}
    }
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	startPWM
**
**	Parameters:
**		timerNum:	The timer to use, with the period of the PWM <TIMER2, TIMER3, TIMER23>
**		OCnum:	The output compare module to use <OC1, OC2, OC3, OC4, OC5>
**		dutycycle:	The duty cycle (out of 100%) of the PWM
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Enables one of the output compare modules as a PWM with the specified duty cycle,
**		and the period of the specified timer.
**
**	Example:
**		startPWM(TIMER2, OC1, 80);		Initializes OC1 as a PWM output with 80% duty cycle, and TIMER2's period
*/
void picStartPWM(uint8_t timerNum, uint8_t OCnum, uint8_t dutycycle){
	
	unsigned long outputCompareValue;
	
	if (dutycycle<=100){
		switch(timerNum){
		case TIMER2:
			outputCompareValue = (PR2 * dutycycle) / 100;
			switch(OCnum){
			case OC1:
				OC1RS = (outputCompareValue), OC1R = (outputCompareValue), OC1CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE);
				break;
			case OC2:
				OC2RS = (outputCompareValue), OC2R = (outputCompareValue), OC2CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE);
				break;
			case OC3:
				OC3RS = (outputCompareValue), OC3R = (outputCompareValue), OC3CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE);
				break;
			case OC4:
				OC4RS = (outputCompareValue), OC4R = (outputCompareValue), OC4CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE);
				break;
			case OC5:
				OC5RS = (outputCompareValue), OC5R = (outputCompareValue), OC5CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE);
				break;
			}
			break;
		case TIMER3:
			outputCompareValue = (PR3 * dutycycle) / 100;
			switch(OCnum){
			case OC1:
				OC1RS = (outputCompareValue), OC1R = (outputCompareValue), OC1CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE);
				break;
			case OC2:
				OC2RS = (outputCompareValue), OC2R = (outputCompareValue), OC2CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC3:
				OC3RS = (outputCompareValue), OC3R = (outputCompareValue), OC3CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC4:
				OC4RS = (outputCompareValue), OC4R = (outputCompareValue), OC4CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC5:
				OC5RS = (outputCompareValue), OC5R = (outputCompareValue), OC5CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE16 | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			}
			break;
		case TIMER23:
			outputCompareValue = (PR2 * dutycycle) / 100;
			switch(OCnum){
			case OC1:
				OC1RS = (outputCompareValue), OC1R = (outputCompareValue), OC1CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE32 | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC2:
				OC2RS = (outputCompareValue), OC2R = (outputCompareValue), OC2CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE32 | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC3:
				OC3RS = (outputCompareValue), OC3R = (outputCompareValue), OC3CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE32 | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC4:
				OC4RS = (outputCompareValue), OC4R = (outputCompareValue), OC4CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE32 | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			case OC5:
				OC5RS = (outputCompareValue), OC5R = (outputCompareValue), OC5CON = (OC_ON | OC_IDLE_CON | OC_TIMER_MODE32 | OC_PWM_FAULT_PIN_DISABLE);				
				break;
			}
			break;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	stopPWM()
**
**	Parameters:
**		OCnum:	The output compare module to turn off <OC1, OC2, OC3, OC4, OC5>
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Turns off the specified output compare module.
**
**	Example:
**		stopPWM(OC1);	Turns off the PWM signal being output by OC1
*/
void picStopPWM(uint8_t OCnum){
	switch(OCnum){
		case OC1: IEC0CLR = _IEC0_OC1IE_MASK;
			OC1CONCLR = _OC1CON_ON_MASK;
			break;
		case OC2: IEC0CLR = _IEC0_OC2IE_MASK;
			OC2CONCLR = _OC2CON_ON_MASK;
			break;
		case OC3: IEC0CLR = _IEC0_OC3IE_MASK;
			OC1CONCLR = _OC1CON_ON_MASK;
			break;
		case OC4: IEC0CLR = _IEC0_OC4IE_MASK;
			OC1CONCLR = _OC1CON_ON_MASK;
			break;
		case OC5: IEC0CLR = _IEC0_OC5IE_MASK;
			OC1CONCLR = _OC1CON_ON_MASK;
			break;
	}
}

/* --------------------------------------------------------------------------------------------------------------------------------------- */
/*	setDutyCycle()
**
**	Parameters:
**		OCnum:	The output compare module to turn off <OC1, OC2, OC3, OC4, OC5>
**		dutycycle:	The duty cycle percent (0-100) to set the PWM to
**
**	Return Value:
**		none
**
**	Errors:
**		none
**
**  Description:
**		Sets the duty cycle of the specified output compare module to a certain percent of the current timer's period.
**
**	Example:
**		setDutyCycle(OC3, 90);	Sets the PWM signal on OC3 to 90% duty cycle
*/
void picSetDutyCycle(uint8_t OCnum, float dutycycle){

	unsigned long outputCompareValue;
	bool timerSrc;
	
	if (dutycycle<=100){
		switch(OCnum){
		case OC1:
			timerSrc = (OC1CON>> _OC1CON_OCTSEL_POSITION) & 1;	//Src = Timer2 if 0, Timer3 if 1
			outputCompareValue = (PR2 * !timerSrc + PR3 * timerSrc) * dutycycle / 100;
			OC1RS=outputCompareValue;
			break;
		case OC2:
			timerSrc = (OC2CON>> _OC2CON_OCTSEL_POSITION) & 1;	//Src = Timer2 if 0, Timer3 if 1
			outputCompareValue = (PR2 * !timerSrc + PR3 * timerSrc) * dutycycle / 100;
			OC2RS=outputCompareValue;
			break;
		case OC3:
			timerSrc = (OC3CON>> _OC3CON_OCTSEL_POSITION) & 1;	//Src = Timer2 if 0, Timer3 if 1
			outputCompareValue = (PR2 * !timerSrc + PR3 * timerSrc) * dutycycle / 100;
			OC3RS=outputCompareValue;
			break;
		case OC4:
			timerSrc = (OC4CON>> _OC4CON_OCTSEL_POSITION) & 1;	//Src = Timer2 if 0, Timer3 if 1
			outputCompareValue = (PR2 * !timerSrc + PR3 * timerSrc) * dutycycle / 100;
			OC4RS=outputCompareValue;
			break;
		case OC5:
			timerSrc = (OC5CON>> _OC5CON_OCTSEL_POSITION) & 1;	//Src = Timer2 if 0, Timer3 if 1
			outputCompareValue = (PR2 * !timerSrc + PR3 * timerSrc) * dutycycle / 100;
			OC5RS=outputCompareValue;
			break;
		}
	}
}


//Interrupt Service Routines
//************************************************************************
// Timer1 ISR
void __attribute__((interrupt(),nomips16)) Timer1IntHandler(void)
{

	if (intFunc[TIMER1] != 0)
	{
		(*intFunc[TIMER1])();
	}
	IFS0bits.T1IF	=	0;
}

//************************************************************************
// Timer2 ISR
void __attribute__((interrupt(),nomips16)) Timer2IntHandler(void)
{

	if (intFunc[TIMER2] != 0)
	{
		(*intFunc[TIMER2])();
	}
	IFS0bits.T2IF	=	0;
}

//************************************************************************
// Timer3 ISR
void __attribute__((interrupt(),nomips16)) Timer3IntHandler(void)
{

	if (intFunc[TIMER3] != 0)
	{
		(*intFunc[TIMER3])();
	}
	IFS0bits.T3IF	=	0;
}

//************************************************************************
// Timer4 ISR
void __attribute__((interrupt(),nomips16))Timer4IntHandler(void)
{

	if (intFunc[TIMER4] != 0)
	{
		(*intFunc[TIMER4])();
	}
	IFS0bits.T4IF	=	0;
}

//************************************************************************
// Timer5 ISR
void __attribute__((interrupt(),nomips16)) Timer5IntHandler(void)
{

	if (intFunc[TIMER5] != 0)
	{
		(*intFunc[TIMER5])();
	}
	IFS0bits.T5IF	=	0;
}

//************************************************************************


#endif

