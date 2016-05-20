
#include "hsf.h"

PROCESS(connect_test_process, "connect_test_process");

static int g_wifi_connect_flag =0;
static NETSOCKET app_tcp_socket=-1;
static struct tcp_socket t_socket;
static int g_tcp_connect_flag = 0;

static tcp_socket_recv_callback_t app_tcp_recv_callback(NETSOCKET socket, unsigned char *data, unsigned short len)
{
	data[len] = '0';
	u_printf("socket %d receive data with length %d, [%s]\r\n\n", socket, len, data);
}

static tcp_socket_connect_callback_t app_tcp_connect_callback(NETSOCKET socket)
{
	u_printf("socket %d connected\n", socket);

	g_tcp_connect_flag = 3;
	hfnet_tcp_send(socket, "GET / HTTP/1.1\r\n\r\n", strlen("GET / HTTP/1.1\r\n\r\n"));
}

static tcp_socket_close_callback_t app_tcp_close_callback(NETSOCKET socket)
{
	g_tcp_connect_flag = 0;
	u_printf("socket %d closed\n", socket);
}

static tcp_socket_send_callback_t app_tcp_send_callback(NETSOCKET socket)
{
	u_printf("TCP data send ack\n");
}

PROCESS_THREAD(connect_test_process, ev, data)
{
	PROCESS_BEGIN();
	static struct etimer timer_sleep;
	static int connect_state = 0;
	
	etimer_set(&timer_sleep, 3 * CLOCK_SECOND);
	while(1) 
	{
		
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER || ev == PROCESS_EVENT_CONTINUE || ev == resolv_event_found );
		if(g_wifi_connect_flag == 0)
		{
			u_printf("wait wifi connect ...\r\n");
			etimer_set(&timer_sleep, 10 * CLOCK_SECOND);
			g_wifi_connect_flag = 1;
			continue;
		}

		if(ev == resolv_event_found )
		{
			char *pHostName = (char*)data;
			if(strcmp(pHostName, "www.baidu.com"))
				continue;

			uip_ipaddr_t addr;
			uip_ipaddr_t *addrptr = &addr;
			resolv_lookup(pHostName, &addrptr);
			u_printf("Resolv IP = %d.%d.%d.%d\n", addrptr->u8[0], addrptr->u8[1], addrptr->u8[2], addrptr->u8[3] );

			uip_ipaddr_copy(&t_socket.r_ip, addrptr);
			t_socket.r_port = 80;
			t_socket.recv_callback = app_tcp_recv_callback;
			t_socket.connect_callback = app_tcp_connect_callback;
			t_socket.close_callback = app_tcp_close_callback;
			t_socket.send_callback = app_tcp_send_callback;
			t_socket.recv_data_maxlen = 1024;

			app_tcp_socket = hfnet_tcp_connect(&t_socket);
			
			g_tcp_connect_flag = 1;
			etimer_set(&timer_sleep, 15 * CLOCK_SECOND);
			continue;
		}
		
		if(g_tcp_connect_flag == 0)
		{
			resolv_query("www.baidu.com");
			
			etimer_set(&timer_sleep, 15 * CLOCK_SECOND);
			g_tcp_connect_flag = 0;
			continue;
		}
		else if(g_tcp_connect_flag == 1)
		{
			app_tcp_socket = hfnet_tcp_connect(&t_socket);
			
			etimer_set(&timer_sleep, 15 * CLOCK_SECOND);
			g_tcp_connect_flag = 2;
			continue;
		}
		else if(g_tcp_connect_flag == 2)
		{
			etimer_set(&timer_sleep, 5 * CLOCK_SECOND);
			
			g_tcp_connect_flag = 0;
			continue;
		}
		else
		{
			etimer_set(&timer_sleep, 5 * CLOCK_SECOND);
		}
	}
	
	PROCESS_END();
}

void tcpclient_test_start(void)
{
	process_start(&connect_test_process, NULL);
}