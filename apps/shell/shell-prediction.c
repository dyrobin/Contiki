#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include <string.h>
#include <stdio.h>

#define MAX_BUF_SIZE 193

PROCESS(shell_send_udp_process, "send_udp");
SHELL_COMMAND(send_udp_command,
              "send_udp",
              "send_udp: send udp to destination",
              &shell_send_udp_process);

static void 
set_s_addr(uint16_t s, uint16_t opt, uip_ipaddr_t *send_addr)
{
    if(opt == 1) {
        uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, s);
    } else {
        uint16_t hexanum = 0x7400;
        uint16_t var1, var2;
        var1 = hexanum + s;
        var2 = 257*s;
        uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0x0212, var1, s, var2);
    }
}

PROCESS_THREAD(shell_send_udp_process, ev, data)
{
    static uip_ipaddr_t dest_addr;
    const char *nextptr;
    static struct uip_udp_conn *conn;
    char buf[256];
    uint16_t s,i;
    uint16_t opt;

    PROCESS_BEGIN();
    
    s = shell_strtolong(data, &nextptr);
   //opt = shell_strtolong(nextptr, &nextptr);
   //set_s_addr(s, opt, &dest_addr); 
    set_s_addr(s, 1, &dest_addr); 
     

    for(i=0; i<MAX_BUF_SIZE; i++) {
        buf[i] = '0'+(i%8);
    }
    buf[MAX_BUF_SIZE] = 0;
    printf("%d strlen\n", strlen(buf));
    conn = udp_new(&dest_addr, UIP_HTONS(1729), NULL);
    //sprintf(buf, "Hello there node\n");
    uip_udp_packet_send(conn, buf, strlen(buf));

    PROCESS_END();
}

void
shell_prediction_init(void)
{
   shell_register_command(&send_udp_command);
}
