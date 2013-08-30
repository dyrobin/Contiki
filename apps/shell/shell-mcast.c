#include "contiki.h"
#include "net/multicast6/pim.h"
#include "net/multicast6/pim-control.h"

#include "shell.h"
#include "net/uip.h"
#include "net/uip-udp-packet.h"
#include <string.h>
#include <stdio.h>

#define MAX_BUF_SIZE 64
#define MIN_BUF_TEST 10
//---------------------------------------------------
/*PROCESS(shell_help_process, "help");
SHELL_COMMAND(help_command,
            "help",
            "help: prints help",
            &shell_help_process);
*/
PROCESS(shell_send_join_process, "send_join");
SHELL_COMMAND(send_join_command,
            "send_join",
            "send_join: sends a join request",
            &shell_send_join_process);

PROCESS(shell_send_prune_process, "send_prune");
SHELL_COMMAND(send_prune_command,
            "send_prune",
            "send_prune: sends a prune request",
            &shell_send_prune_process);

PROCESS(shell_send_data_process, "send_data");
SHELL_COMMAND(send_data_command,
            "send_data",
            "send_data: sends data by multicast",
            &shell_send_data_process);

PROCESS(shell_print_table_process, "print_table");
SHELL_COMMAND(print_table_command,
              "print_table",
              "print_table: print multicast table",
              &shell_print_table_process);

/*
PROCESS(shell_check_path_process, "check_path");
SHELL_COMMAND(check_path_command,
              "check_path",
              "check_path: check path to destination",
              &shell_check_path_process);
*/

//---------------------------------------------------
static void 
set_s_addr(uint16_t s, uip_ipaddr_t *send_addr)
{
#if CONTIKI_TARGET_Z1
    uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, s);
#endif

#if CONTIKI_TARGET_SKY
    uint16_t hexanum = 0x7400;
    uint16_t var1, var2;
    var1 = hexanum + s;
    var2 = 257*s;
    uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0x0212, var1, s, var2);
#endif
}

static void
set_g_addr(uint16_t g, uip_ipaddr_t *grp_addr) 
{
    uip_ip6addr(grp_addr, 0xff31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, g);
}

/*PROCESS_THREAD(shell_help_process, ev, data)
{
    PROCESS_BEGIN();
    shell_output_str(&help_command, "you can run the following commands:", "");
    shell_output_str(&help_command, "help, send_join, send_prune, print_table", "");
    PROCESS_END();
}*/

PROCESS_THREAD(shell_send_join_process, ev, data)
{
    static uip_ipaddr_t grp_addr;
    static uip_ipaddr_t send_addr;
    const char *nextptr;
    uint16_t s,g;
    
    PROCESS_BEGIN();
    shell_output_str(&send_join_command, "Sending join request", "");
    
    s = shell_strtolong(data, &nextptr);
    g = shell_strtolong(nextptr, &nextptr);
    set_g_addr(g, &grp_addr);
    set_s_addr(s, &send_addr);
    mcast6_join_send(&send_addr, &grp_addr, 1);
     
    PROCESS_END();
}

PROCESS_THREAD(shell_send_prune_process, ev, data)
{
    static uip_ipaddr_t grp_addr;
    static uip_ipaddr_t send_addr;
    const char *nextptr;
    uint16_t s,g;
    
    PROCESS_BEGIN();
    shell_output_str(&send_prune_command, "Sending prune request", "");
    
    s = shell_strtolong(data, &nextptr);
    g = shell_strtolong(nextptr, &nextptr);
    set_g_addr(g, &grp_addr);
    set_s_addr(s, &send_addr);
    mcast6_prune_send(&send_addr, &grp_addr);
     
    PROCESS_END();
}

PROCESS_THREAD(shell_send_data_process, ev, data)
{
    static uip_ipaddr_t grp_addr;
    static struct uip_udp_conn *conn;
    const char *nextptr;
    char buf[MAX_BUF_SIZE];
    uint16_t g;
    int len;
    
    PROCESS_BEGIN();
    //shell_output_str(&send_data_command, "Sending data to ", "");    
    
    g = shell_strtolong(data, &nextptr);
    set_g_addr(g, &grp_addr);    
    len = (int)strlen(nextptr);
    if(len < MAX_BUF_SIZE) {
        strncpy(buf, nextptr, len);
    } else {
        strncpy(buf, nextptr, MIN_BUF_TEST);
        len = MIN_BUF_TEST;
    }
    printf("len = %d, message = %s \n", len, buf);
    conn = udp_new(&grp_addr, UIP_HTONS(2345), NULL);
    uip_udp_packet_send(conn, buf, len);

    PROCESS_END();
}

PROCESS_THREAD(shell_print_table_process, ev, data)
{
    PROCESS_BEGIN();
    print_mcast_table();
    PROCESS_END();
}

/*PROCESS_THREAD(shell_check_path_process, ev, data)
{
    static uip_ipaddr_t dest_addr;
    const char *nextptr;
    static struct uip_udp_conn *conn;
    char buf[64];
    uint16_t s;

    PROCESS_BEGIN();
    //shell_output_str(&check_path_command, "Checking path...", "");
    
    s = shell_strtolong(data, &nextptr);
    set_s_addr(s, &dest_addr); 

    conn = udp_new(&dest_addr, UIP_HTONS(1729), NULL);
    sprintf(buf, "Hello there \n");
    uip_udp_packet_send(conn, buf, strlen(buf));

    PROCESS_END();
}*/

//----------------------------------------------------
void
shell_mcast_init(void)
{
    shell_register_command(&send_join_command);
    shell_register_command(&send_prune_command);
    shell_register_command(&send_data_command);
    shell_register_command(&print_table_command);
   // shell_register_command(&check_path_command);
}
