#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-debug.h"
#include "net/rpl/rpl.h"
#include "dev/serial-line.h"
#if CONTIKI_TARGET_Z1
#include "dev/uart0.h"
#else
#include "dev/uart1.h"
#endif

#include <stdio.h>
#include <string.h>

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct uip_udp_conn *server_conn;

PROCESS(child_shell_process, "Basic Shell For prediction testing- dodag child");
AUTOSTART_PROCESSES(&child_shell_process);

static void
tcpip_handler(void)
{
    char *appdata;

    if(uip_newdata()) {
        appdata = (char *)uip_appdata;
        appdata[uip_datalen()] = 0;
        printf("data recieved %s from ", appdata);
        uip_debug_ipaddr_print(&UIP_IP_BUF->srcipaddr);
        printf("\n");
    }
}

static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}

PROCESS_THREAD(child_shell_process, ev, data)
{
    PROCESS_BEGIN();
    
#if CONTIKI_TARGET_Z1
    uart0_set_input(serial_line_input_byte);
#else
    uart1_set_input(serial_line_input_byte);
#endif
    serial_line_init();
    serial_shell_init();

    shell_prediction_init();
    
    set_global_address();
    
    server_conn = udp_new(NULL, 0, NULL);
    udp_bind(server_conn, UIP_HTONS(1729));

    while(1) {
        PROCESS_YIELD();
        if(ev == tcpip_event) {
            tcpip_handler();
        }
    }

    PROCESS_END();
}
