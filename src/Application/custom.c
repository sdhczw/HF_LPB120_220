/****************************************************************************************
* File Name: custom.c
*
* Description:
*  Common user application file.
*
*****************************************************************************************/

#include <hsf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "zc_hf_adpter.h"
#include "ac_hal.h"

static int hfsys_event_callback( uint32_t event_id,void * param) 
{ 
	switch(event_id) 
	{ 
		case HFE_WIFI_STA_CONNECTED: 
			HF_Debug(DEBUG_WARN, "wifi sta connected!!\n"); 
			break; 
		case HFE_WIFI_STA_DISCONNECTED: 
			HF_Debug(DEBUG_WARN, "wifi sta disconnected!!\n"); 
			HF_Sleep();
			HF_BcInit();
			break; 
		case HFE_DHCP_OK: 
		{ 
			g_u32GloablIp = *((uint32_t*)param); 
			g_u32GloablIp = ZC_HTONL(g_u32GloablIp);
			HF_Debug(DEBUG_WARN, "dhcp ok %x!\r\n", *((uint32_t*)param));
			HF_WakeUp();
            resolv_query(g_struZcConfigDb.struCloudInfo.u8CloudAddr);
			break; 
		} 
		case HFE_SMTLK_OK: 
			HF_Debug(DEBUG_WARN, "smtlk ok!\n"); 
			break; 
		case HFE_CONFIG_RELOAD: 
			HF_Debug(DEBUG_WARN, "reload!\n"); 
			break; 
		default: 
			break; 
	} 
	return 0; 
} 

static int uart_recv_callback(uint32_t event,char *data,uint32_t len,uint32_t buf_len)
{
	HF_Debug(DEBUG_WARN,"sys run mode=[%d] and recv len=[%d]",hfsys_get_run_mode(),len);
	if(hfsys_get_run_mode() != HFSYS_STATE_RUN_THROUGH)
		return len;
	
#ifdef ZC_EASY_UART 
        AC_UartRecv((u8 *)data, len);  
#else
        ZC_Moudlefunc((u8 *)data, len);
#endif


	return len;
}

static void uart_config(int baudrate)
{
	char buffer[64];
	char cmd[40];
	int ret, resetflag = 0;

	ret = hfat_send_cmd("AT+UART\r\n", sizeof("AT+URAT\r\n"), buffer, 64);
	sprintf(cmd, "%d,8,1,None,NFC", baudrate);
	if(memcmp(buffer + 4, cmd, strlen(cmd)) != 0)
	{
		sprintf(cmd, "AT+UART=%d,8,1,NONE,NFC\r\n", baudrate);
		ret = hfat_send_cmd(cmd, strlen(cmd), buffer, 64);
		if(ret != HF_SUCCESS)
		{
			HF_Debug(DEBUG_WARN, "Failed to set UART0");
		}
		else
			resetflag = 1;
	}

	if(resetflag == 1)
	{
		hfsys_reset();
	}
}
 
static void show_reset_reason(void)
{
	uint32_t reset_reason=0;
	reset_reason = hfsys_get_reset_reason();
	
	if(reset_reason&HFSYS_RESET_REASON_SMARTLINK_OK)
	{
		u_printf("RESET FOR SMARTLINK OK\n");
        HF_ReadDataFromFlash((u8 *)&g_struZcConfigDb,sizeof(g_struZcConfigDb));
        g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig = 0;
        g_struZcConfigDb.struDeviceInfo.u32UnBcFlag = 0xFFFFFFFF;
        g_struZcConfigDb.u32Crc = crc16_ccitt(((u8 *)&g_struZcConfigDb) + 4, sizeof(g_struZcConfigDb) - 4);        
        HF_WriteDataToFlash((u8 *)&g_struZcConfigDb, sizeof(ZC_ConfigDB));
	}

	return;
}

/*
*name:	void start_custom_service(void)
*Para:	none
*return:	none
*/
void start_custom_service(void)
{
	show_reset_reason();
	
	if(hfsys_register_system_event((hfsys_event_callback_t)hfsys_event_callback)!=HF_SUCCESS)
	{
		HF_Debug(DEBUG_WARN, "register system event fail\n");
	}

	if(hfnet_start_assis(ASSIS_PORT) != HF_SUCCESS)
	{
		HF_Debug(DEBUG_WARN,"start httpd fail\n");
	}
	 
	if(hfnet_start_uart(0, (hfnet_callback_t)uart_recv_callback)!=HF_SUCCESS)
	{
		HF_Debug(DEBUG_WARN,"start uart fail!\n");
	}

#if 0 
	//test API Functions
	HF_Debug(DEBUG_WARN,"Uart test (UART0 should recv \'UART0\; UART1 should recv \'UART1\'):\r\n");
	hfuart_send(HFUART0, "\r\nUART0\r\n", 9, 0);
	hfuart_send(HFUART1, "\r\nUART1\r\n", 9, 0);

	HF_Debug(DEBUG_WARN,"Flash test:\r\n");
	uint8_t data_read[16]={0};
	uint8_t data_write[16]={0};
	memset(data_write, "0123456789ABCDEF", 16);
	hfuflash_erase_page(0,1); 
	hfuflash_write(0, data_write, 16);
	hfuflash_read(0, data_read, 16);
	if(memcmp(data_write, data_read, 16) == 0)
		HF_Debug(DEBUG_WARN,"Flash sucess.\r\n");
	else
		HF_Debug(DEBUG_WARN,"Flash fail.\r\n");

	HF_Debug(DEBUG_WARN,"Get MAC test:\r\n");
	uint8_t pu8Mac[32];
	extern void HF_GetMac(u8 *pu8Mac);
	HF_GetMac(pu8Mac);
	HF_Debug(DEBUG_WARN,"MAC:[%s]\r\n", pu8Mac);

	//hfsmtlk_start();
#endif

	/*Hardware Init*/
	uart_config(115200);
	HF_Init();
}


