/**
******************************************************************************
* @file     zc_hf_adpter.c
* @authors  cxy
* @version  V1.0.0
* @date     10-Sep-2014
* @brief    Event
******************************************************************************
*/
#include <zc_protocol_controller.h>
#include <zc_timer.h>
#include <zc_module_interface.h>
#include <hsf.h>
#include <zc_hf_adpter.h>
#include <stdlib.h>
#include <stdio.h> 
#include <stdarg.h>
#include <ac_cfg.h>
#include <ac_api.h>
#include <ac_hal.h>

PROCESS(HF_Cloudfunc, "HF_Cloudfunc");
//PROCESS(HF_CloudRecvfunc, "HF_CloudRecvfunc");
struct udp_socket u_socket;
struct tcp_socket t_client_socket;
struct tcp_socket t_server_socket;
	
extern PTC_ProtocolCon  g_struProtocolController;
PTC_ModuleAdapter g_struHfAdapter;

MSG_Buffer g_struRecvBuffer;
MSG_Buffer g_struRetxBuffer;
MSG_Buffer g_struClientBuffer;


MSG_Queue  g_struRecvQueue;
MSG_Buffer g_struSendBuffer[MSG_BUFFER_SEND_MAX_NUM];
MSG_Queue  g_struSendQueue;

u8 g_u8MsgBuildBuffer[MSG_BULID_BUFFER_MAXLEN];
u8 g_u8ClientSendLen = 0;

u8 g_u8recvbuffer[HF_MAX_SOCKET_LEN];
ZC_UartBuffer g_struUartBuffer;
HF_TimerInfo g_struHfTimer[ZC_TIMER_MAX_NUM];
//hfthread_mutex_t g_struTimermutex; not support mutex
u8  g_u8BcSendBuffer[100];
u32 g_u32BcSleepCount;//about 500*10 = 5000ms
struct sockaddr_in struRemoteAddr;
static unsigned char connect_func_state=0;
static unsigned char connect_func_time=0;
uip_ip4addr_t	connect_remote_ip_addr;
static process_event_t event_send_ok;
static process_event_t event_connect_ok;
u8 g_u8sendflag = 0;
/*************************************************
* Function: HF_ReadDataFormFlash
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_ReadDataFromFlash(u8 *pu8Data, u16 u16Len) 
{
#ifdef __LPT200__
    hffile_userbin_read(0, (char *)(pu8Data), u16Len);
#else
    hfuflash_read(0, (pu8Data), u16Len);
#endif 
}

/*************************************************
* Function: HF_WriteDataToFlash
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_WriteDataToFlash(u8 *pu8Data, u16 u16Len)
{
#ifdef __LPT200__
    hffile_userbin_write(0, (char*)pu8Data, u16Len);
#else
    hfuflash_erase_page(0,1); 
    hfuflash_write(0, pu8Data, u16Len);
#endif 
}

/*************************************************
* Function: HF_timer_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void USER_FUNC HF_timer_callback(hftimer_handle_t htimer) 
{
    u8 u8TimeId;
    //hfthread_mutext_lock(g_struTimermutex);
    u8TimeId = hftimer_get_timer_id(htimer);
#if 0    
    if (1 == g_struHfTimer[u8TimeId].u32FirstFlag)
    {
        g_struHfTimer[u8TimeId].u32FirstFlag = 0;
        hftimer_start(htimer);
    }
    else
#endif
    {
        TIMER_TimeoutAction(u8TimeId);
        TIMER_StopTimer(u8TimeId);
    }
    
    //hfthread_mutext_unlock(g_struTimermutex);
}
/*************************************************
* Function: HF_StopTimer
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_StopTimer(u8 u8TimerIndex)
{
    hftimer_stop(g_struHfTimer[u8TimerIndex].struHandle);
    hftimer_delete(g_struHfTimer[u8TimerIndex].struHandle);
}

/*************************************************
* Function: HF_SetTimer
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_SetTimer(u8 u8Type, u32 u32Interval, u8 *pu8TimeIndex)
{
    u8 u8TimerIndex;
    u32 u32Retval;

    u32Retval = TIMER_FindIdleTimer(&u8TimerIndex);
    if (ZC_RET_OK == u32Retval)
    {
        TIMER_AllocateTimer(u8Type, u8TimerIndex, (u8*)&g_struHfTimer[u8TimerIndex]);
        g_struHfTimer[u8TimerIndex].struHandle = hftimer_create(NULL,u32Interval,false,u8TimerIndex,
             HF_timer_callback,0);
        hftimer_start(g_struHfTimer[u8TimerIndex].struHandle);  
        
        *pu8TimeIndex = u8TimerIndex;
    }
    
    return u32Retval;
}

/*************************************************
* Function: HF_FirmwareUpdateFinish
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_FirmwareUpdateFinish(u32 u32TotalLen)
{
    int retval;
    retval = hfupdate_complete(HFUPDATE_SW, u32TotalLen);
    if (HF_SUCCESS == retval)
    {
        return ZC_RET_OK;
    }
    else
    {
        return ZC_RET_ERROR;    
    }
}

/*************************************************
* Function: HF_FirmwareUpdate
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_FirmwareUpdate(u8 *pu8FileData, u32 u32Offset, u32 u32DataLen)
{
    int retval;
    if (0 == u32Offset)
    {
        hfupdate_start(HFUPDATE_SW);
    }
    
    retval = hfupdate_write_file(HFUPDATE_SW, u32Offset, (char *)pu8FileData, u32DataLen); 
    if (retval < 0)
    {
        return ZC_RET_ERROR;
    }
    
    return ZC_RET_OK;
}
/*************************************************
* Function: HF_SendDataToMoudle
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_SendDataToMoudle(u8 *pu8Data, u16 u16DataLen)
{
#ifdef ZC_MODULE_DEV 
    AC_RecvMessage((ZC_MessageHead *)pu8Data);
#else
    u8 u8MagicFlag[4] = {0x02,0x03,0x04,0x05};
    if(hfsys_get_run_mode() != HFSYS_STATE_RUN_THROUGH)
	return 0;
	
    hfuart_send(HFUART0,(char*)u8MagicFlag,4,1000); 
    hfuart_send(HFUART0,(char*)pu8Data,u16DataLen,1000); 
#endif
    return ZC_RET_OK;
}

/*************************************************
* Function: HF_Rest
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Rest(void)
{   
    //msleep(500);
    hfsmtlk_start();
}
/*************************************************
* Function: HF_SendTcpData
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_SendTcpData(u32 u32Fd, u8 *pu8Data, u16 u16DataLen, ZC_SendParam *pstruParam)
{
	hfnet_tcp_send(u32Fd, pu8Data, u16DataLen);
    g_u8sendflag = 1;
}
/*************************************************
* Function: HF_SendUdpData
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_SendUdpData(u32 u32Fd, u8 *pu8Data, u16 u16DataLen, ZC_SendParam *pstruParam)
{
   	struct sockaddr_in *addr;
   	addr = (struct sockaddr_in *)pstruParam->pu8AddrPara;
  	hfnet_udp_sendto(u32Fd, pu8Data, u16DataLen, &addr->addr, addr->port);
}

/*************************************************
* Function: HF_GetMac
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_GetMac(u8 *pu8Mac)
{
    char rsp[64]={0};
    char *mac[3]={0};
    memset(rsp, 0, sizeof(rsp));
    hfat_send_cmd("AT+WSMAC\r\n", sizeof("AT+WSMC\r\n"), rsp, 64);
    //ZC_Printf("AT+WSMAC's response:%s\n",rsp);
    hfat_get_words(rsp, mac, 3);
    memcpy(pu8Mac,mac[1],ZC_SERVER_MAC_LEN);
}

/*************************************************
* Function: HF_Reboot
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Reboot(void)
{
    hfsys_reset();
}

/*************************************************
* Function: app_tcp_client_recv_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
static int is_connected_cloud=0;
tcp_socket_recv_callback_t app_tcp_client_recv_callback(NETSOCKET socket, unsigned char *data, unsigned short len)
{
	if(len > HF_MAX_SOCKET_LEN)
		len = HF_MAX_SOCKET_LEN;
	memcpy(g_u8recvbuffer, data, len);
	ZC_Printf("recv data len = %d\n", (u32)len);
	MSG_RecvDataFromCloud(g_u8recvbuffer, (u32)len);

	return 0;
}
/*************************************************
* Function: app_tcp_client_connect_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
tcp_socket_connect_callback_t app_tcp_client_connect_callback(NETSOCKET socket)
{
#ifdef DEBUG_IKAIR
	HF_Debug(DEBUG_LEVEL_USER, "TCP client connect.\r\n");
#endif
	/*change state to wait access*/

    	PTC_ProtocolCon *pstruContoller = &g_struProtocolController;
    if(PCT_STATE_ACCESS_NET==pstruContoller->u8MainState)
    {
        connect_func_state = 0;
	    connect_func_time = 0;

	    is_connected_cloud = 1;
	
    	g_struProtocolController.struCloudConnection.u32ConnectionTimes = 0;

    	ZC_Printf("connect ok!\n");
    	g_struProtocolController.struCloudConnection.u32Socket = (U32)socket;
	    ZC_Rand(g_struProtocolController.RandMsg);
	    pstruContoller->u8MainState = PCT_STATE_WAIT_ACCESS;
    	pstruContoller->u8keyRecv = PCT_KEY_UNRECVED;
        process_post(&HF_Cloudfunc, event_connect_ok, NULL);
    }
}

/*************************************************
* Function: app_tcp_client_close_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/

tcp_socket_close_callback_t app_tcp_client_close_callback(NETSOCKET socket)
{
	HF_Debug(DEBUG_LEVEL_USER, "TCP client close <%d>.\r\n", socket);
	connect_func_state = 0;
	connect_func_time = 0;

	if(is_connected_cloud == 1)
	{
		 ZC_Printf("recv error, len = %d\n",0);
		PCT_DisConnectCloud(&g_struProtocolController);
	                    
		g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
		g_struUartBuffer.u32RecvLen = 0;

		if(g_struProtocolController.struCloudConnection.u32ConnectionTimes++>20)
		{
			g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig = 0;
		}
        is_connected_cloud =0;
	}
}

/*************************************************
* Function: app_tcp_client_send_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/

tcp_socket_send_callback_t app_tcp_client_send_callback(NETSOCKET socket)
{
	 ZC_Printf("Send ok\n");
     process_post(&HF_Cloudfunc, event_send_ok, NULL);
}


/*************************************************
* Function: HF_ConnectToCloud
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_ConnectToCloud(PTC_Connection *pstruConnection)
{ 
	if(connect_func_state == 0)
	{
		t_client_socket.recv_callback = app_tcp_client_recv_callback;
		t_client_socket.connect_callback = app_tcp_client_connect_callback;
		t_client_socket.close_callback = app_tcp_client_close_callback;
		t_client_socket.send_callback = app_tcp_client_send_callback;
		t_client_socket.recv_data_maxlen = 1024;

		if (1 == g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig)
		{
			t_client_socket.r_port = g_struZcConfigDb.struSwitchInfo.u16ServerPort;
			t_client_socket.r_ip.u8[0] = ((g_struZcConfigDb.struSwitchInfo.u32ServerIp>>(8*3))&0xFF);
			t_client_socket.r_ip.u8[1] = ((g_struZcConfigDb.struSwitchInfo.u32ServerIp>>(8*2))&0xFF);
			t_client_socket.r_ip.u8[2] = ((g_struZcConfigDb.struSwitchInfo.u32ServerIp>>(8*1))&0xFF);
			t_client_socket.r_ip.u8[3] = ((g_struZcConfigDb.struSwitchInfo.u32ServerIp>>(8*0))&0xFF);
			
			ZC_Printf("connect ip = %d.%d.%d.%d  port = %d!\n", t_client_socket.r_ip.u8[0],  t_client_socket.r_ip.u8[1],  t_client_socket.r_ip.u8[2],  t_client_socket.r_ip.u8[3], t_client_socket.r_port);
			hfnet_tcp_connect(&t_client_socket);

			connect_func_state = 3;
			connect_func_time = 1;
		}
		else
		{
			t_client_socket.r_port = ZC_CLOUD_PORT;
			ZC_Printf("Connect to [%s]\r\n", g_struZcConfigDb.struCloudInfo.u8CloudAddr);
			resolv_query(g_struZcConfigDb.struCloudInfo.u8CloudAddr);
			connect_func_state = 1;
			connect_func_time = 1;
			
			return ZC_RET_ERROR;
		}
		//t_socket.l_port = t_socket.r_port -100;
	}
	else if(connect_func_state == 2)
	{
		t_client_socket.r_ip.u8[0] = connect_remote_ip_addr.u8[0];
		t_client_socket.r_ip.u8[1] = connect_remote_ip_addr.u8[1];
		t_client_socket.r_ip.u8[2] = connect_remote_ip_addr.u8[2];
		t_client_socket.r_ip.u8[3] = connect_remote_ip_addr.u8[3];
		ZC_Printf("connect ip = %d.%d.%d.%d  port = %d!\n", t_client_socket.r_ip.u8[0],  t_client_socket.r_ip.u8[1],  t_client_socket.r_ip.u8[2],  t_client_socket.r_ip.u8[3], t_client_socket.r_port);
		hfnet_tcp_connect(&t_client_socket);

		connect_func_state = 3;
		connect_func_time = 1;
	}
	if(g_struProtocolController.struCloudConnection.u32ConnectionTimes++>20)
	{
		g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig = 0;
	}
	return ZC_RET_ERROR;
}

/*************************************************
* Function: app_tcp_server_recv_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/

static tcp_socket_recv_callback_t app_tcp_server_recv_callback(NETSOCKET socket, unsigned char *data, unsigned short len)
{
#ifdef DEBUG_IKAIR
	data[len]='\0';
	HF_Debug(DEBUG_LEVEL_USER, "TCP server recv <%d>[%s]:\r\n", len, data);
#endif
	if(len > HF_MAX_SOCKET_LEN)
		len = HF_MAX_SOCKET_LEN;
	memcpy(g_u8recvbuffer, data, len);
	ZC_RecvDataFromClient(socket, g_u8recvbuffer, len);

	return 0;
}

/*************************************************
* Function: app_tcp_server_close_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/

static tcp_socket_close_callback_t app_tcp_server_close_callback(NETSOCKET socket)
{
	  ZC_ClientDisconnect(socket);
       //close(socket); no need close
       hfnet_tcp_close(socket);
}

/*************************************************
* Function: app_tcp_server_accept_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/

static tcp_socket_accept_callback_t app_tcp_server_accept_callback(NETSOCKET socket)
{
	printf("socket %d accept\n", socket);
	if (ZC_RET_ERROR == ZC_ClientConnect((u32)socket))
	{
		hfnet_tcp_close(socket);
	}
	else
	{
		ZC_Printf("accept client = %d\n", socket);
	}
}

/*************************************************
* Function: app_tcp_server_send_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/

static tcp_socket_send_callback_t app_tcp_server_send_callback(NETSOCKET socket)
{
          //HF_Debug(DEBUG_LEVEL_USER, "TCP server send ok \r\n");
}

/*************************************************
* Function: HF_ListenClient
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_ListenClient(PTC_Connection *pstruConnection)
{
	int fd;
	t_server_socket.listen_port = pstruConnection->u16Port;
	t_server_socket.recv_callback = app_tcp_server_recv_callback;
	t_server_socket.close_callback = app_tcp_server_close_callback;
	t_server_socket.accept_callback = app_tcp_server_accept_callback;
	t_server_socket.send_callback = app_tcp_server_send_callback;
	t_server_socket.recv_data_maxlen = 1024;
	fd = hfnet_tcp_listen(&t_server_socket);

	ZC_Printf("Tcp Listen Port = %d,socket = %d\r\n", pstruConnection->u16Port,fd);
	g_struProtocolController.struClientConnection.u32Socket = (U32)fd;

	return ZC_RET_OK;
}

/*************************************************
* Function: HF_Printf
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Printf(const char *pu8format, ...)
{
    char buffer[100 + 1]={0};
    va_list arg;
    va_start (arg, pu8format);
    vsnprintf(buffer, 100, pu8format, arg);
    va_end (arg);
    HF_Debug(DEBUG_LEVEL_USER, "%s", buffer);
}

/*************************************************
* Function: app_udp_recv_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
static udp_socket_recv_callback_t app_udp_recv_callback(NETSOCKET socket, 
	unsigned char *data, 
	unsigned short len,
	uip_ipaddr_t *peeraddr, 
	unsigned short peerport)
{
	if(len > 100)
		len=100;
	memcpy(g_u8BcSendBuffer, data, len);
	if(len > 0) 
	{
		ZC_SendClientQueryReq(g_u8BcSendBuffer, (u16)len);
	} 
}
/*************************************************
* Function: HF_BcInit
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_BcInit()
{
	u_socket.l_port = ZC_MOUDLE_PORT;
	u_socket.recv_callback = app_udp_recv_callback;
	u_socket.recv_data_maxlen = 1024;
	g_Bcfd = hfnet_udp_create(&u_socket);
	if(g_Bcfd<0)
	{
		HF_Debug(DEBUG_WARN,"create udp failed\n");
	}

	g_struProtocolController.u16SendBcNum = 0;

	struRemoteAddr.addr.u8[0]= 0xFF;
	struRemoteAddr.addr.u8[1]= 0xFF;
	struRemoteAddr.addr.u8[2]= 0xFF;
	struRemoteAddr.addr.u8[3]= 0xFF;//"255.255.255.255"
	struRemoteAddr.port = ZC_MOUDLE_BROADCAST_PORT;
	
	g_pu8RemoteAddr = (u8*)&struRemoteAddr;
	g_u32BcSleepCount = 0;//about 500*10 = 5000ms
	return;
}

/*************************************************
* Function: HF_Cloudfunc
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
PROCESS_THREAD(HF_Cloudfunc, ev, data)
{
	PROCESS_BEGIN();
	HF_Debug(DEBUG_WARN, "Process HF_Cloudfunc start:\r\n");
	static struct etimer HF_Cloudfunc_timer; 
	//int fd;
	u32 u32Timer = 0;

	HF_BcInit();

	etimer_set(&HF_Cloudfunc_timer, 1 * CLOCK_SECOND);
	while(1) 
	{

		etimer_set(&HF_Cloudfunc_timer, 5* CLOCK_MINI_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(ev == resolv_event_found || ev ==PROCESS_EVENT_TIMER);

		ZC_StartClientListen();
		
		if(ev == resolv_event_found )
		{
			char *pHostName = (char*)data;
			uip_ipaddr_t addr;
			uip_ipaddr_t *addrptr = &addr;
			resolv_lookup(pHostName, &addrptr);
			uip_ipaddr_copy(&connect_remote_ip_addr, addrptr);
			HF_Debug(10, "Resolv IP = %d.%d.%d.%d\n", connect_remote_ip_addr.u8[0], connect_remote_ip_addr.u8[1], connect_remote_ip_addr.u8[2], connect_remote_ip_addr.u8[3] );

			connect_func_state = 2;
			connect_func_time = 1;
		}
		//test
		/*if(connect_func_state == 1)
		{
			connect_remote_ip_addr.u8[0] = 123;
			connect_remote_ip_addr.u8[1] = 57;
			connect_remote_ip_addr.u8[2] = 206;
			connect_remote_ip_addr.u8[3] = 235;
			HF_Debug(10, "Resolv IP = %d.%d.%d.%d\n", connect_remote_ip_addr.u8[0], connect_remote_ip_addr.u8[1], connect_remote_ip_addr.u8[2], connect_remote_ip_addr.u8[3] );

			connect_func_state = 2;
			connect_func_time = 0;
		}*///test
		
		
		if((connect_func_state == 0)||(connect_func_state == 2))
		{
			//fd = g_struProtocolController.struCloudConnection.u32Socket;
			PCT_Run();
            if(3 == connect_func_state)
            {
               etimer_set(&HF_Cloudfunc_timer,CLOCK_SECOND);
	       PROCESS_WAIT_EVENT_UNTIL(ev == event_connect_ok || ev ==PROCESS_EVENT_TIMER);
            }
		}
		
		if(connect_func_state != 0)
		{
			connect_func_time++;
			if(connect_func_time > 30)
			{
				if(connect_func_state == 3)
				{
                                    if (PCT_INVAILD_SOCKET != g_struProtocolController.struCloudConnection.u32Socket)
                                    {
	                                hfnet_tcp_disconnect((NETSOCKET)g_struProtocolController.struCloudConnection.u32Socket);
	                                g_struProtocolController.struCloudConnection.u32Socket = PCT_INVAILD_SOCKET;
	                            }
					//app_tcp_client_close_callback(g_struProtocolController.struCloudConnection.u32Socket);
				}
					
				connect_func_state = 0;
				connect_func_time = 0;
			}
			continue;
		}
		
		if (PCT_STATE_DISCONNECT_CLOUD == g_struProtocolController.u8MainState)
		{
                        if (PCT_INVAILD_SOCKET != g_struProtocolController.struCloudConnection.u32Socket)
                        {
	                       hfnet_tcp_disconnect((NETSOCKET)g_struProtocolController.struCloudConnection.u32Socket);
	                      g_struProtocolController.struCloudConnection.u32Socket = PCT_INVAILD_SOCKET;
	                }
			if (0 == g_struProtocolController.struCloudConnection.u32ConnectionTimes)
			{
				u32Timer = 1000;
			}
			else
			{
				u32Timer = rand();
				u32Timer = (PCT_TIMER_INTERVAL_RECONNECT) * (u32Timer % 10 + 1);
			}
			PCT_ReconnectCloud(&g_struProtocolController, u32Timer);
			g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
			g_struUartBuffer.u32RecvLen = 0;
		}
		else
		{
			MSG_SendDataToCloud((u8*)&g_struProtocolController.struCloudConnection);
                        if(1 == g_u8sendflag)
                       {
                         etimer_set(&HF_Cloudfunc_timer,CLOCK_SECOND);
	                 PROCESS_WAIT_EVENT_UNTIL(ev == event_send_ok || ev ==PROCESS_EVENT_TIMER);
                         g_u8sendflag = 0;
                       }
		}

		ZC_SendBc();
	} 
	PROCESS_END();
}
/*************************************************
* Function: HF_Init
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Init()
{
    //\CD\F8\C2\E7闁瓡D0濞屻倖绉綷DA
    g_struHfAdapter.pfunConnectToCloud = HF_ConnectToCloud;
    g_struHfAdapter.pfunListenClient = HF_ListenClient;
    g_struHfAdapter.pfunSendTcpData = HF_SendTcpData; 
    g_struHfAdapter.pfunSendUdpData = HF_SendUdpData;     
    g_struHfAdapter.pfunUpdate = HF_FirmwareUpdate;  
    //\C9閻犵偟鐝旵4鐠囨彫BF闁瓡D0濞屻倖绉綷DA
    g_struHfAdapter.pfunSendToMoudle = HF_SendDataToMoudle; 
    //\B6\A8閺冪ΡC6\F7\C0\E0\BD濞戝DA
    g_struHfAdapter.pfunSetTimer = HF_SetTimer;   
    g_struHfAdapter.pfunStopTimer = HF_StopTimer;        
    //\B4婵炲樊鏅淐0\E0\BD濞戝DA
    g_struHfAdapter.pfunUpdateFinish = HF_FirmwareUpdateFinish;
    g_struHfAdapter.pfunWriteFlash = HF_WriteDataToFlash;
    g_struHfAdapter.pfunReadFlash = HF_ReadDataFromFlash;
    //缁崵绮篭C0\E0\BD濞戝DA    
    g_struHfAdapter.pfunRest = HF_Rest;    
    g_struHfAdapter.pfunGetMac = HF_GetMac;
    g_struHfAdapter.pfunReboot = HF_Reboot;
    g_struHfAdapter.pfunMalloc = hfmem_malloc;
    g_struHfAdapter.pfunFree = hfmem_free;
    g_struHfAdapter.pfunPrintf = HF_Printf;
    //g_struHfAdapter.pfunPrintf = printf;
    PCT_Init(&g_struHfAdapter);
    
    ZC_Printf("MT Init\n");
    
    g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
    g_struUartBuffer.u32RecvLen = 0;

//#ifdef ZC_EASY_UART 
    AC_Init();
//#endif	
  
    process_start(&HF_Cloudfunc, NULL);	
    event_send_ok = process_alloc_event();
    event_connect_ok = process_alloc_event();
    //process_start(&HF_CloudRecvfunc, NULL);
    //hfthread_mutext_new(&g_struTimermutex);
}

/*************************************************
* Function: HF_WakeUp
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_WakeUp()
{
	PCT_WakeUp();
}

/*************************************************
* Function: HF_Sleep
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Sleep()
{
	u32 u32Index;

	hfnet_udp_close((NETSOCKET)g_Bcfd);

	if (PCT_INVAILD_SOCKET != g_struProtocolController.struClientConnection.u32Socket)
	{
		hfnet_tcp_unlisten((NETSOCKET)g_struProtocolController.struClientConnection.u32Socket);
		g_struProtocolController.struClientConnection.u32Socket = PCT_INVAILD_SOCKET;
	}

	if (PCT_INVAILD_SOCKET != g_struProtocolController.struCloudConnection.u32Socket)
	{
		hfnet_tcp_disconnect((NETSOCKET)g_struProtocolController.struCloudConnection.u32Socket);
		g_struProtocolController.struCloudConnection.u32Socket = PCT_INVAILD_SOCKET;
	}
    
	for (u32Index = 0; u32Index < ZC_MAX_CLIENT_NUM; u32Index++)
	{
		if (0 == g_struClientInfo.u32ClientVaildFlag[u32Index])
		{
			hfnet_tcp_close(g_struClientInfo.u32ClientFd[u32Index]);
			g_struClientInfo.u32ClientFd[u32Index] = PCT_INVAILD_SOCKET;
		}
	}

	PCT_Sleep();

	g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
	g_struUartBuffer.u32RecvLen = 0;
}

/*************************************************
* Function: AC_UartSend
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void AC_UartSend(u8* inBuf, u32 datalen)
{
     if(hfsys_get_run_mode() != HFSYS_STATE_RUN_THROUGH)
	return;
	
     hfuart_send(HFUART0,(char*)inBuf,datalen,1000); 
}
/******************************* FILE END ***********/

