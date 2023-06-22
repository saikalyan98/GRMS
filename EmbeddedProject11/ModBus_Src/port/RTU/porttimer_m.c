/*
 * FreeModbus Libary: LPC1769 Port
 * Copyright (C) 2013 Armink <armink.ztl@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: porttimer_m.c,v 1.60 2013/08/13 15:07:05 Armink add Master Functions$
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"
#include "mbconfig.h"
#include "chip.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"

#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0
/* ----------------------- Variables ----------------------------------------*/
static USHORT usT35TimeOut50us;

/* ----------------------- static functions ---------------------------------*/
static void prvvTIMERExpiredISR(void);

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBMasterPortTimersInit(USHORT usTimeOut50us)
{
    /* backup T35 ticks */
    usT35TimeOut50us = usTimeOut50us;

    Chip_TIMER_Init(LPC_TIMER0);                                          //De-Initialize the Clock for Timer0

	Chip_TIMER_Disable(LPC_TIMER0);											// Enable TC Counter for Timer0
    Chip_Clock_SetPCLKDiv(SYSCTL_PCLK_TIMER0, SYSCTL_CLKDIV_1);             // Set Peripheral Clock to 1
    Chip_TIMER_MatchEnableInt(LPC_TIMER0, 0);                               // Enable match Interrupt
    Chip_TIMER_ResetOnMatchEnable(LPC_TIMER0, 0);                           // Reset TC on Match
    Chip_TIMER_SetMatch(LPC_TIMER0, 0, (usT35TimeOut50us * (SystemCoreClock/20000)));          // Set match to 4800 (For 50uS)
    NVIC_EnableIRQ(TIMER0_IRQn);                                            // Enable Interrupt for Timer0

    Chip_TIMER_Enable(LPC_TIMER0);                                          // Enable TC Counter for Timer0

    return TRUE;
}

void vMBMasterPortTimersT35Enable()
{
	xMBMasterPortTimersInit(usT35TimeOut50us);
	NVIC_DisableIRQ(TIMER0_IRQn);											//Disable NVIC Timer Interrupt
    Chip_TIMER_Disable(LPC_TIMER0);                                         // Disable TC Counter for Timer0
	
	Chip_TIMER_Reset(LPC_TIMER0);
	Chip_TIMER_SetMatch(LPC_TIMER0, 0, (usT35TimeOut50us * (SystemCoreClock / 20000)));          // Set match to usT35TimeOut50us (For (T3.5) * (50uS))
	/* Set current timer mode, don't change it.*/
    vMBMasterSetCurTimerMode(MB_TMODE_T35);
	
	Chip_TIMER_Enable(LPC_TIMER0); // Enable TC Counter for Timer0
	NVIC_EnableIRQ(TIMER0_IRQn);   // Enable Interrupt for Timer0
}

void vMBMasterPortTimersConvertDelayEnable()
{
	xMBMasterPortTimersInit(usT35TimeOut50us);
	NVIC_DisableIRQ(TIMER0_IRQn); //Disable NVIC Timer Interrupt
	Chip_TIMER_Disable(LPC_TIMER0); // Disable TC Counter for Timer0
	
	Chip_TIMER_Reset(LPC_TIMER0);
	Chip_TIMER_SetMatch(LPC_TIMER0, 0, (MB_MASTER_DELAY_MS_CONVERT * (SystemCoreClock / 1000))); // Set match to usT35TimeOut50us (For (T3.5) * (50uS))
	/* Set current timer mode, don't change it.*/
	vMBMasterSetCurTimerMode(MB_TMODE_CONVERT_DELAY);
	
	Chip_TIMER_Enable(LPC_TIMER0); // Enable TC Counter for Timer0
	NVIC_EnableIRQ(TIMER0_IRQn);											// Enable Interrupt for Timer0
}

void vMBMasterPortTimersRespondTimeoutEnable()
{
	xMBMasterPortTimersInit(usT35TimeOut50us);
	NVIC_DisableIRQ(TIMER0_IRQn); //Disable NVIC Timer Interrupt
	Chip_TIMER_Disable(LPC_TIMER0); // Disable TC Counter for Timer0
	
	Chip_TIMER_Reset(LPC_TIMER0);
	Chip_TIMER_SetMatch(LPC_TIMER0, 0, (MB_MASTER_TIMEOUT_MS_RESPOND * (SystemCoreClock / 1000)));          // Set match to usT35TimeOut50us (For (T3.5) * (50uS))
    /* Set current timer mode, don't change it.*/
	vMBMasterSetCurTimerMode(MB_TMODE_RESPOND_TIMEOUT);
	
	Chip_TIMER_Enable(LPC_TIMER0); // Enable TC Counter for Timer0
	NVIC_EnableIRQ(TIMER0_IRQn); // Enable Interrupt for Timer0
}

void vMBMasterPortTimersDisable()
{
	xMBMasterPortTimersInit(usT35TimeOut50us);
	NVIC_DisableIRQ(TIMER0_IRQn); //Disable NVIC Timer Interrupt
	Chip_TIMER_Disable(LPC_TIMER0); // Disable TC Counter for Timer0
	Chip_TIMER_Reset(LPC_TIMER0);
}

void prvvTIMERExpiredISR(void)
{
    (void) pxMBMasterPortCBTimerExpired();
}

void TIMER0_IRQHandler()
{
	/* Clear Timer Interrupt */
	LPC_TIMER0->IR |= (1 << 0);
	
    prvvTIMERExpiredISR();
}

#endif
