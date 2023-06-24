/*
 * @brief LWIP no-RTOS TCP Echo example
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

/* ------------------------ System includes ------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ------------------------ Defines --------------------------------------- */
#define mainCOM_TEST_BAUD_RATE ((unsigned portLONG)38400)

#define mainMB_TASK_PRIORITY (tskIDLE_PRIORITY + 3)
#define PROG "FreeModbus"
#define REG_INPUT_START 1000
#define REG_INPUT_NREGS 4
#define REG_HOLDING_START 2000
#define REG_HOLDING_NREGS 130
#define REG_COIL_START 3000
#define REG_COIL_NREGS 500


//xComPortHandle xSTDComPort = NULL;
/*User defined Includes*/
#include "User_Config.h"

#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/memp.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/timers.h"
#include "netif/etharp.h"

#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif

#include "arch\lpc17xx_40xx_emac.h"
#include "arch\lpc_arch.h"
#include "board.h"
#include "lpc_phy.h"

/* ------------------------ FreeModbus includes --------------------------- */
#include "mb.h"
#include "mbconfig.h"
#include "user_mb_app.h"

/* ----------------------- Static variables ---------------------------------*/
/*
static USHORT usRegInputStart = REG_INPUT_START;
static USHORT usRegInputBuf[REG_INPUT_NREGS];
static USHORT usRegHoldingStart = REG_HOLDING_START;
static USHORT usRegHoldingBuf[REG_HOLDING_NREGS];
static USHORT usRegCoilStart = REG_HOLDING_START;
static USHORT usRegCoilBuf[REG_HOLDING_NREGS];
*/

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* NETIF data */
static struct netif lpc_netif;
uint16_t LED1_Count = 0;
uint16_t OneSec_Count = 0;
uint8_t u8_Poll_State = 1;
uint8_t change = 0;

uint8_t u8ChangeValue[50] = {0}, u8ChangeRegister = 0, u8ChangeHappened = 0, u8WriteChange = 0, u8ChangeUpdated = 1;
;

USHORT Touch_Panel_LED_Config[8] = {1, 1, 1, 1, 1, 1, 1, 1};
/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
extern USHORT usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];
extern USHORT usSRegHoldBuf[S_REG_HOLDING_NREGS];

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Poll for any Received TCP Packets in Grand Loop as we are not using Interrupts */
uint32_t u32_TCPPoll(uint32_t physts);

/* Toggle Led for every 1 Second */
void vToggleLed(void);

/* Toggle Led for every ModBus Command */
void vMBToggleLeds(void);

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* LED0 is used for the link status, on = PHY cable detected */
	/* Initial LED state is off to show an unconnected cable state */
	Board_LED_Set(0, false);

	/* Setup a 1mS sysTick for the primary time base */
	SysTick_Enable(1);
	
	/*Board Pins Initialisations*/
	Board_SystemInit();

	LPC_SYSCTL->PCONP |= 1 << 25;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	main routine for example_lwip_tcpecho_sa_17xx40xx
 * @return	Function should not exit.
 */
int main(void)
{
#if PORT_MODBUS
	eMBErrorCode xStatus;
#endif
	bool led_toggle_flag = 0;
	uint32_t physts;
	ip_addr_t ipaddr, netmask, gw;
	static int prt_ip = 0;

	prvSetupHardware();

	/* Initialize LWIP */
	lwip_init();

	LWIP_DEBUGF(LWIP_DBG_ON, ("Starting LWIP TCP echo server...\n"));

	/* Static IP assignment */
#if LWIP_DHCP
	IP4_ADDR(&gw, 192, 168, 10, 1);
	IP4_ADDR(&ipaddr, 0, 0, 0, 0);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
#else
	IP4_ADDR(&gw, 192, 168, 10, 1);
	IP4_ADDR(&ipaddr, 192, 168, 10, 56);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
//	APP_PRINT_IP(&ipaddr);
#endif

	/* Add netif interface for lpc17xx_8x */
	netif_add(&lpc_netif, &ipaddr, &netmask, &gw, NULL, lpc_enetif_init,
			  ethernet_input);
	netif_set_default(&lpc_netif);
	netif_set_up(&lpc_netif);

#if LWIP_DHCP
	dhcp_start(&lpc_netif);
#endif

#if PORT_MODBUS==1
	
#else
	/* Initialize and start application */
	echo_init();
#endif

	eMBMasterInit(MB_RTU, 2, 19200, MB_PAR_NONE);
	eMBMasterEnable();
	eMBMasterReqWriteHoldingRegister(1, 10, 0, 100);
	while (OneSec_Count != 300)
		;
	eMBMasterReqWriteHoldingRegister(1, 20, 0, 100);
	while (OneSec_Count != 600)
		;
	eMBMasterReqReadHoldingRegister(1, u8ChangeRegister, 50, 100);
	while (OneSec_Count != 1000)
		;
	OneSec_Count = 0;

	while (true)
	{
		if (eMBTCPInit(MB_TCP_PORT_USE_DEFAULT) != MB_ENOERR)
		{
			//Debug Statement
		}
		else if (eMBEnable() != MB_ENOERR)
		{
			//Debug Statement
		}
		else
		{
			/*Grand Loop Start*/
			do
			{
				/* TCP Polling function */
				/* This could be done in the sysTick ISR, but may stay in IRQ context
				too long, so do this stuff with a background loop. */
				physts = u32_TCPPoll(physts);
				
				/* ModBus TCP Slave Polling function */
				xStatus = eMBPoll();

				/* ModBus RTU Master Polling function */
				eMBMasterPoll();
				
				/* Toggle Led for every one second */
				vToggleLed();

				/* From TCP to Slave */
				if (u8ChangeValue[20] != usSRegHoldBuf[20])
				{
					u8ChangeUpdated = 0;
					u8ChangeHappened = 1;
					u8ChangeRegister = 20;
					u8WriteChange = u8ChangeValue[20] = usSRegHoldBuf[20];
					u8_Poll_State = 0;
					eMBMasterReqWriteHoldingRegister(1, u8ChangeRegister, u8WriteChange, 100);
				}
				if (u8ChangeValue[10] != usSRegHoldBuf[10])
				{
					u8ChangeUpdated = 0;
					u8ChangeHappened = 1;
					u8ChangeRegister = 10;
					u8WriteChange = u8ChangeValue[10] = usSRegHoldBuf[10];
					u8_Poll_State = 0;
					eMBMasterReqWriteHoldingRegister(1, u8ChangeRegister, u8WriteChange, 100);
				}

				if (u8ChangeValue[0] != usSRegHoldBuf[0])
				{
					u8ChangeUpdated = 0;
					u8ChangeHappened = 1;
					u8ChangeRegister = 0;
					u8WriteChange = u8ChangeValue[0] = usSRegHoldBuf[0];
					u8_Poll_State = 0;
					eMBMasterReqWriteHoldingRegister(1, u8ChangeRegister, u8WriteChange, 100);
				}
				
				/* Read Touch Panel for every 1 Second */
				if (OneSec_Count == 1000)
				{
					eMBMasterReqReadHoldingRegister(1, u8ChangeRegister, 50, 100);
				}
				else if (OneSec_Count == 2000)
				{
					OneSec_Count = 0;
					usSRegHoldBuf[0] = usMRegHoldBuf[1][0];
					usSRegHoldBuf[10] = usMRegHoldBuf[1][10];
					usSRegHoldBuf[20] = usMRegHoldBuf[1][20];
				}

			} while (xStatus == MB_ENOERR);
		}
		/* An error occured. Maybe we can restart. */
		(void)eMBDisable();
		(void)eMBClose();
	}

	/* Never returns, for warning only */
	return 0;
}

uint32_t u32_TCPPoll(uint32_t physts)
{
	/* Handle packets as part of this loop, not in the IRQ handler */
	lpc_enetif_input(&lpc_netif);

	/* lpc_rx_queue will re-qeueu receive buffers. This normally occurs
		   automatically, but in systems were memory is constrained, pbufs
		   may not always be able to get allocated, so this function can be
		   optionally enabled to re-queue receive buffers. */
#if 1
	while (lpc_rx_queue(&lpc_netif))
	{
	}
#endif

	/* Free TX buffers that are done sending */
	lpc_tx_reclaim(&lpc_netif);

	/* LWIP timers - ARP, DHCP, TCP, etc. */
	sys_check_timeouts();

	/* Call the PHY status update state machine once in a while
		   to keep the link status up-to-date */
	physts = lpcPHYStsPoll();

	/* Only check for connection state when the PHY status has changed */
	if (physts & PHY_LINK_CHANGED)
	{
		if (physts & PHY_LINK_CONNECTED)
		{
			Board_LED_Set(0, true);
#if PORT_MODBUS
#else
			prt_ip = 0;
#endif

			/* Set interface speed and duplex */
			if (physts & PHY_LINK_SPEED100)
			{
				Chip_ENET_Set100Mbps(LPC_ETHERNET);
				NETIF_INIT_SNMP(&lpc_netif, snmp_ifType_ethernet_csmacd, 100000000);
			}
			else
			{
				Chip_ENET_Set10Mbps(LPC_ETHERNET);
				NETIF_INIT_SNMP(&lpc_netif, snmp_ifType_ethernet_csmacd, 10000000);
			}
			if (physts & PHY_LINK_FULLDUPLX)
			{
				Chip_ENET_SetFullDuplex(LPC_ETHERNET);
			}
			else
			{
				Chip_ENET_SetHalfDuplex(LPC_ETHERNET);
			}

			netif_set_link_up(&lpc_netif);
		}
		else
		{
			Board_LED_Set(0, false);
			netif_set_link_down(&lpc_netif);
		}

		DEBUGOUT("Link connect status: %d\r\n", ((physts & PHY_LINK_CONNECTED) != 0));
	}

#if PORT_MODBUS
#else
	/* Print IP address info */
	if (!prt_ip)
	{
		if (lpc_netif.ip_addr.addr)
		{
			static char tmp_buff[16];
			DEBUGOUT("IP_ADDR    : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *)&lpc_netif.ip_addr, tmp_buff, 16));
			DEBUGOUT("NET_MASK   : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *)&lpc_netif.netmask, tmp_buff, 16));
			DEBUGOUT("GATEWAY_IP : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *)&lpc_netif.gw, tmp_buff, 16));
			prt_ip = 1;
		}
	}
#endif
	return physts;
}

void vToggleLed(void)
{
	if (LED1_Count == 1000)
	{
		Board_LED_Set(1, false);
	}
	else if (LED1_Count == 2000)
	{
		Board_LED_Set(1, true);
		LED1_Count = 0;
	}
}

/**
 * @}
 */
