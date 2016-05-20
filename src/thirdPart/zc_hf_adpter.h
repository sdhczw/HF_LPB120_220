/**
******************************************************************************
* @file     zc_hf_adpter.h
* @authors  cxy
* @version  V1.0.0
* @date     10-Sep-2014
* @brief    HANDSHAKE
******************************************************************************
*/

#ifndef  __ZC_HF_ADPTER_H__ 
#define  __ZC_HF_ADPTER_H__

#include <zc_common.h>
#include <zc_protocol_controller.h>
#include <zc_module_interface.h>

//#define DEBUG_IKAIR

typedef struct 
{
    hftimer_handle_t struHandle;
}HF_TimerInfo;

struct sockaddr_in
{
	uip_ipaddr_t addr;
	unsigned short port;
};

#define HF_MAX_SOCKET_LEN    (1000)



#ifdef __cplusplus
extern "C" {
#endif
void HF_Init(void);
void HF_BcInit(void);
void HF_Sleep(void);
void HF_WakeUp(void);
void HF_ReadDataFromFlash(u8 *pu8Data, u16 u16Len);
void HF_WriteDataToFlash(u8 *pu8Data, u16 u16Len);
#ifdef __cplusplus
}
#endif
#endif
/******************************* FILE END ***********************************/

