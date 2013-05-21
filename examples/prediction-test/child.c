#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-debug.h"
#include "net/uip-udp-packet.h"
#include "net/rpl/rpl.h"
#include "dev/serial-line.h"
#include "dev/leds.h"
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
    static struct uip_udp_conn *conn;
    char buf[16];
    char *appdata;

    if(uip_newdata()) {
        if(uip_datalen() > 4) {        
            appdata = (char *)uip_appdata;
            printf("data no: %c%c%c%c recieved of size %d from ", appdata[0], appdata[1], appdata[2], appdata[3], uip_datalen());
            uip_debug_ipaddr_print(&UIP_IP_BUF->srcipaddr);
            printf("\n");
            leds_toggle(LEDS_GREEN);
//            conn = udp_new(&UIP_IP_BUF->srcipaddr, UIP_HTONS(1729), NULL);
//            sprintf(buf, "OK");
//            uip_udp_packet_send(conn, buf, strlen(buf));
        } else if(uip_datalen() == 2) {
            printf("OK received\n");
        } else {
            printf("Weird data\n");
        }
    }
}

static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xbbbb, 0, 0, 0, 0, 0, 0, 0);
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
/*#ifdef CONTIKI_TARGET_SKY
static void
create_route(void)
{
    uip_ipaddr_t dest;
    uip_ipaddr_t next;
    uip_ds6_route_t *r;

    uip_ip6addr(&dest, 0xaaaa, 0, 0, 0, 0xc30c, 0, 0, 0x0003);
    uip_ip6addr(&next, 0xfe80, 0, 0, 0, 0xc30c, 0, 0, 0x0003);

//    printf("adding route\n");
    uip_ds6_route_add(&dest, 128, &next, 0);
    
    for(r = uip_ds6_route_list_head(); r != NULL; r = list_item_next(r)) {
        uip_debug_ipaddr_print(&r->ipaddr);
        printf(" via ");
        uip_debug_ipaddr_print(&r->nexthop);
        printf("\n");
    }
}
#endif*/

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
//#ifdef CONTIKI_TARGET_SKY
//    create_route();
//#endif
    
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
