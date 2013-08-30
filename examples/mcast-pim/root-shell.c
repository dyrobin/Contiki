#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/multicast6/uip-mcast6.h"

#include "shell.h"
#include "serial-shell.h"
#include "net/uip.h"
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

//static struct uip_udp_conn *server_conn;

PROCESS(root_shell_process, "Basic Shell For Multicast Testing for Dodag Root");
AUTOSTART_PROCESSES(&root_shell_process);

/*static void
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
}*/

static uip_ipaddr_t *
set_global_address(void)
{
    static uip_ipaddr_t ipaddr;
    int i;
    uint8_t state;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

    PRINTF("IPV6 addresses:");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
            (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
            printf("\n");
        }
    }

  return &ipaddr;
}

static void
create_rpl_dag(uip_ipaddr_t *ipaddr)
{
    struct uip_ds6_addr *root_if;

    root_if = uip_ds6_addr_lookup(ipaddr);
    if(root_if != NULL) {
        rpl_dag_t *dag;
        uip_ipaddr_t prefix;
    
        rpl_set_root(RPL_DEFAULT_INSTANCE, ipaddr);
        dag = rpl_get_any_dag();
        uip_ip6addr(&prefix, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &prefix, 64);
        PRINTF("created a new RPL dag\n");
    } else {
        PRINTF("failed to create a new RPL DAG\n");
    }
}

PROCESS_THREAD(root_shell_process, ev, data)
{
    uip_ipaddr_t *ipaddr;
   
    PROCESS_BEGIN();
 
   
#if CONTIKI_TARGET_Z1
    uart0_set_input(serial_line_input_byte);
#else
    uart1_set_input(serial_line_input_byte);
#endif
    serial_line_init();
    serial_shell_init();

    shell_mcast_init();

    ipaddr = set_global_address();

    create_rpl_dag(ipaddr);

/*    server_conn = udp_new(NULL, 0, NULL);
    udp_bind(server_conn, UIP_HTONS(1729));

    while(1) {
        PROCESS_YIELD();
        if(ev == tcpip_event) {
            tcpip_handler();
        }
    }*/
    
    PROCESS_END();
}
