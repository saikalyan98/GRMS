/*
 * FreeModbus Libary: RT-Thread Port
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
 * File: $Id: portserial_m.c,v 1.60 2013/08/13 15:07:05 Armink add Master Functions $
 */

#include "port.h"
#include "mbconfig.h"
#include "chip.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0
/* ----------------------- Static variables ---------------------------------*/


/* ----------------------- Defines ------------------------------------------*/
/* serial transmit event */
#define EVENT_SERIAL_TRANS_START    (1<<0)

/* RS-485 Pin Configurations */
#define RS_485_REBar_GPIO_PORT_NUM              1
#define RS_485_REBar_GPIO_BIT_NUM				21
#define RS_485_DE_GPIO_PORT_NUM					1
#define RS_485_DE_GPIO_BIT_NUM					20

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBMasterPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    /* Setup UART for 115.2K8N1 */
    /* Enable Clock for UART3*/
    Chip_UART_Init(LPC_UART3);
	/* Set Buadrate to 115200 */
	Chip_UART_SetBaudFDR(LPC_UART3, ulBaudRate);
	/* Config data as 8 Data bits and 1 Stop bit */
	Chip_UART_ConfigData(LPC_UART3, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT ));
    /* Enable FIFO and Set Level to 0 */
	Chip_UART_SetupFIFOS(LPC_UART3, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV0));
    /* Enable Transmission */
	Chip_UART_TXEnable(LPC_UART3);

    /* Reset and enable FIFOs, FIFO trigger level 3 (14 chars) */
	Chip_UART_SetupFIFOS(LPC_UART3, (UART_FCR_FIFO_EN | UART_FCR_RX_RS | UART_FCR_TX_RS | UART_FCR_TRG_LEV0));

    /* Enable receive data and line status interrupt */
	Chip_UART_IntEnable(LPC_UART3, (UART_IER_RBRINT | UART_IER_THREINT));

    /* preemption = 1, sub-priority = 1 */
    NVIC_SetPriority(UART3_IRQn, 1);
    NVIC_EnableIRQ(UART3_IRQn);

    return TRUE;
}

void vMBMasterPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    uint32_t recved_event;
    if (xRxEnable)
    {
        /* enable RX interrupt */
        Chip_UART_IntEnable(LPC_UART3, UART_IER_RBRINT);
        /* switch 485 to receive mode */
//		LPC_GPIO[1].PIN &= ~(0X300000);
		
		Chip_GPIO_WritePortBit(LPC_GPIO, RS_485_REBar_GPIO_PORT_NUM, RS_485_REBar_GPIO_BIT_NUM, false);     /* Enable Receiving from RS-485 */
		Chip_GPIO_WritePortBit(LPC_GPIO, RS_485_DE_GPIO_PORT_NUM, RS_485_DE_GPIO_BIT_NUM, false);		    /* Disable Transmit from RS-485 */
    }
    else
    {
        /* disable RX interrupt */
        Chip_UART_IntDisable(LPC_UART3, UART_IER_RBRINT);
		/* switch 485 to transmit mode */
		//LPC_GPIO[1].PIN |= (0X300000);
		Chip_GPIO_WritePortBit(LPC_GPIO1, RS_485_REBar_GPIO_PORT_NUM, RS_485_REBar_GPIO_BIT_NUM, true);     /* Disable Receiving from RS-485 */
		Chip_GPIO_WritePortBit(LPC_GPIO1, RS_485_DE_GPIO_PORT_NUM, RS_485_DE_GPIO_BIT_NUM, true);		    /* Enable Transmit from RS-485 */
    }
    if (xTxEnable)
    {
        /* Start serial transmit */
        Chip_UART_IntEnable(LPC_UART3, UART_IER_THREINT);
		NVIC_SetPendingIRQ(UART3_IRQn);

		/* switch 485 to Transmit mode */
//		LPC_GPIO[1].PIN |= (0X300000);
        Chip_GPIO_WritePortBit(LPC_GPIO, RS_485_REBar_GPIO_PORT_NUM, RS_485_REBar_GPIO_BIT_NUM, true);      /* Disable Receiving from RS-485 */
		Chip_GPIO_WritePortBit(LPC_GPIO, RS_485_DE_GPIO_PORT_NUM, RS_485_DE_GPIO_BIT_NUM, true);		    /* Enable Transmit from RS-485 */
    }
    else
    {
        /* Disable serial transmit */
        Chip_UART_IntDisable(LPC_UART3, UART_IER_THREINT);

		/* switch 485 to receive mode */
//		LPC_GPIO[1].PIN &= ~((0X300000));
        Chip_GPIO_WritePortBit(LPC_GPIO, RS_485_REBar_GPIO_PORT_NUM, RS_485_REBar_GPIO_BIT_NUM, false);     /* Enable Receiving from RS-485 */
		Chip_GPIO_WritePortBit(LPC_GPIO, RS_485_DE_GPIO_PORT_NUM, RS_485_DE_GPIO_BIT_NUM, false);		    /* Disable Transmit from RS-485 */
    }
}

void vMBMasterPortClose(void)
{
    ;
}

BOOL xMBMasterPortSerialPutByte(CHAR ucByte)
{
	Chip_UART_SendBlocking(LPC_UART3, &ucByte, 1);
	return TRUE;
}

BOOL xMBMasterPortSerialGetByte(CHAR * pucByte)
{
    *pucByte = Chip_UART_ReadByte(LPC_UART3);
    return TRUE;
}

/*
 * Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call
 * xMBPortSerialPutByte( ) to send the character.
 */
void prvvUARTTxReadyISR(void)
{
    pxMBMasterFrameCBTransmitterEmpty();
}

/*
 * Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
void prvvUARTRxISR(void)
{
    pxMBMasterFrameCBByteReceived();
}

/**
 * This function is UART Interrupt Handler called when any interrupt occur
 */
void UART3_IRQHandler() 
{
	int x = LPC_UART3->IIR;

	if ((LPC_UART3->IIR & (7 << 1)) == 0x02)
	{
		prvvUARTTxReadyISR();
	}
	else if ((LPC_UART3->LSR & 1<<5) == (1<<5))
	{
		prvvUARTTxReadyISR();		
	}
	
	
	if ((LPC_UART3->IIR & (7 << 1)) == 0x04)
        prvvUARTRxISR();
}

#endif
