#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/uip-ds6.h"
#include <string.h>
#include <stdio.h>
#include "sys/clock.h"

#define MAX_BUF_SIZE 500

PROCESS(shell_send_udp_process, "send_udp");
SHELL_COMMAND(send_udp_command,
              "send_udp",
              "send_udp: send udp to destination",
              &shell_send_udp_process);

PROCESS(shell_check_path_process, "check_path");
SHELL_COMMAND(check_path_command,
              "check_path",
              "check_path: check path to destination",
              &shell_check_path_process);

PROCESS(shell_show_routes_process, "show_routes");
SHELL_COMMAND(show_routes_command,
              "show_routes",
              "show_routes: show routing table",
              &shell_show_routes_process);

static void 
set_s_addr(uint16_t s, uint16_t opt, uip_ipaddr_t *send_addr)
{
    if(opt == 1) {
        uip_ip6addr(send_addr, 0xbbbb, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, s);
    } else {
        uint16_t hexanum = 0x7400;
        uint16_t var1, var2;
        var1 = hexanum + s;
        var2 = 257*s;
        uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0x0212, var1, s, var2);
    }
}

static clock_time_t
time_diff(clock_time_t tm, clock_time_t cur)
{
    clock_time_t diff, max;

    max = ~0;
    if(tm < cur) {
        diff = cur - tm;
    } else {
        diff = max + cur - tm;
    }
    return diff;
}
        
PROCESS_THREAD(shell_send_udp_process, ev, data)
{
    static uip_ipaddr_t dest_addr;
    static struct etimer et;
    static clock_time_t t, cur;
    const char *nextptr;
    static struct uip_udp_conn *conn;
    static struct uip_udp_conn *server_conn;
    uint16_t s, opt;
    static uint16_t size,sec,sent,data_size;
    static uint16_t j,seq,len;
    char *appdata;

    PROCESS_BEGIN();
    
    s = shell_strtolong(data, &nextptr);
   // opt = shell_strtolong(nextptr, &nextptr);
    size = shell_strtolong(nextptr, &nextptr);
    data_size = shell_strtolong(nextptr, &nextptr);

    //set_s_addr(s, opt, &dest_addr); 
    set_s_addr(s, 1, &dest_addr); 
    
    seq = 1;
    sec = 5; 
    server_conn = udp_new(NULL, 0, NULL);
    udp_bind(server_conn, UIP_HTONS(1729));
    conn = udp_new(&dest_addr, UIP_HTONS(1729), NULL);
    while(sent < data_size) {
        char buf[MAX_BUF_SIZE], tmp[6];
        for(j=0; j<size; j++) {
            buf[j] = '0'+(j%8);
        }
        buf[size] = 0;
        sprintf(buf,"%3d", seq);
        buf[3] = '0';
        printf("APP: sending %d packet strlen buf %d\n", seq, strlen(buf));
        t = clock_time();
        if((data_size - sent) < size) {
            len = data_size - sent;
        } else {
            len = strlen(buf);
        }
        uip_udp_packet_send(conn, buf, len);
        etimer_set(&et, CLOCK_SECOND*sec);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || (ev == tcpip_event));
        if(ev == tcpip_event) {
            appdata = (char *)uip_appdata;
            sprintf(buf, "%c%c%c%c%c", appdata[0], appdata[1], appdata[2],
                appdata[3], appdata[4], appdata[5]);
            buf[6] = 0;
            sprintf(tmp, "ACK%3d", seq); 
            printf("APP: buf = %s, tmp = %s\n", buf, tmp);
            if(!strcmp(buf, tmp)) {
                seq++;
                sent += size;
                cur = clock_time();
                printf("APP: Latency = %u msec\n", time_diff(t, cur)*1000/CLOCK_SECOND);
            }
        } else {
            printf("APP: timer expired\n");
        }
    }
    
    PROCESS_END();
}

PROCESS_THREAD(shell_check_path_process, ev, data)
{
    static uip_ipaddr_t dest_addr;
    const char *nextptr;
    static struct uip_udp_conn *conn;
    char buf[64];
    uint16_t s,i;
    uint16_t opt;

    PROCESS_BEGIN();
    
    s = shell_strtolong(data, &nextptr);
    //opt = shell_strtolong(nextptr, &nextptr);
    //set_s_addr(s, opt, &dest_addr); 
    set_s_addr(s, 1, &dest_addr); 
     
    conn = udp_new(&dest_addr, UIP_HTONS(1729), NULL);
    sprintf(buf, "hello there node\n");
    //printf("%d strlen\n", strlen(buf));
    uip_udp_packet_send(conn, buf, strlen(buf));

    PROCESS_END();
}

PROCESS_THREAD(shell_show_routes_process, ev, data)
{
    static uip_ds6_route_t *r;

    PROCESS_BEGIN();
    //printf("Routes\n");
    for(r = uip_ds6_route_list_head(); r != NULL; r = list_item_next(r)) {
        uip_debug_ipaddr_print(&r->ipaddr);
        printf(" via ");
        uip_debug_ipaddr_print(&r->nexthop);
        printf("\n");
    }
    PROCESS_END();
}

void
shell_prediction_init(void)
{
   shell_register_command(&send_udp_command);
   shell_register_command(&check_path_command);
   shell_register_command(&show_routes_command);
}
