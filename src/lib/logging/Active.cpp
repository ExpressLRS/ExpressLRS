/*******************************************************************************
 * Copyright 2023 ModalAI Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * 4. The Software is used solely in conjunction with hardware devices provided by
 *    ModalAI® Inc. Reverse engineering or copying this code for use on hardware 
 *    not manufactured by ModalAI is not permitted.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
#ifndef TARGET_NATIVE

#include <Arduino.h>
#include "logging.h"



#ifdef DEBUG_ACTIVE
#define ACTIVE_DEBUG_ON
#endif // Active Debug

#ifdef ACTIVE_DEBUG_ON

void ACTIVEInit( void )
{
}

void SendACTIVEPacket( unsigned char *data, int length )   // This routine is used to send a packet to the ACTIVE Debug Interface
{
    USART2->CR1 |= USART_CR1_TE;
    while(length--)
    {
        uint8_t value = *data++;
        while(!(USART2->SR & USART_SR_TXE)) {}; // Wait for room in register 
        USART2->DR = value;
    }
    while(!(USART2->SR & USART_SR_TC)) {}; // Wait for tx to finish
    USART2->CR1 &= ~USART_CR1_TE;
} 

#define MAX_ACTIVE_LENGTH 255 
unsigned char ACTIVETxBuffer[MAX_ACTIVE_LENGTH];

void ACTIVEValue( int channel, int value )
{
    int length = 0;
    char done = 0;
    char positive = (value >= 0);

    ACTIVETxBuffer[length++] = 0x7F;
    ACTIVETxBuffer[length++] = channel & 0x3F;

    while(!done)
    {
        if ((positive && (value >= 32)) || (!positive && (value < -32)))
        {
            ACTIVETxBuffer[length++] = value & 0x3F;
            value = value >> 6;
        }
        else
        {
            ACTIVETxBuffer[length++] =  (value & 0x3F ) | 0x40;
            done = 1;
        }
    }

    SendACTIVEPacket(ACTIVETxBuffer, length);
}

void ACTIVEText( int channel, char *string )
{
    int length = 0;

    ACTIVETxBuffer[length++] = 0x7F;
    ACTIVETxBuffer[length++] = 0x40 | (channel & 0x3F);
    
    while(*string)
    {
        if (length >= MAX_ACTIVE_LENGTH-1)
            break;
        ACTIVETxBuffer[length++] = *string & 0x7F;     // Send the ascii characters
        string++;
        
    }
    ACTIVETxBuffer[length++] = 0;     // Send the string termination
        
    SendACTIVEPacket(ACTIVETxBuffer, length);

}

#include "stdio.h"
#include <stdarg.h>   // va_list, va_start, and va_end
char ACTIVEstr[MAX_ACTIVE_LENGTH];
void ACTIVEprintf( int channel, char *format, ... )
{
    va_list arglist;
    va_start( arglist, format );
    vsprintf( ACTIVEstr, format, arglist );
    va_end( arglist );
    ACTIVEText( channel, ACTIVEstr );
}

#else
void ACTIVEInit( void ) {};
void ACTIVEText( int channel, char *string ) {};
void ACTIVEValue( int channel, int value ) {};
void ACTIVEprintf( int channel, char *format, ... ) {};
#endif
#endif