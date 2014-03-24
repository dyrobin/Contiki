/**
 * This shell is used to test PMPD (Path Maximum Transport Protocol Data Unit Discovery)
 * Usage:
 *    send $nodeid $filesize $TPDU
 *        send $filesize bytes to $nodeid using $TPDU
 *        $TPDU will not used if PMPD is on 
 *    check $nodeid
 *        check if $nodeid can be reached or not
 *    showrt
 *        show routing tables
 *
 * Authored by Yang Deng <yang.deng@aalto.fi>
 */
#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h" 
#include <string.h>
#include <stdio.h>
#include "sys/clock.h"

#if PMPD_ENABLED == 1
#include "net/ipv6/pmpd.h"
#endif

#include "net/ip/uip-debug.h"

#define 	MAX_BUF_SIZE 	300
#define 	RETRANS_TIMER	3

PROCESS(shell_send_process, "send");
SHELL_COMMAND(send_command,
              "send",
              "send $nodeid $filesize $TPDU", &shell_send_process);

PROCESS(shell_check_process, "check");
SHELL_COMMAND(check_command,
              "check",
              "check $nodeid", &shell_check_process);

PROCESS(shell_showrt_process, "showrt");
SHELL_COMMAND(showrt_command,
              "showrt",
              "showrt", &shell_showrt_process);

static void
set_ipaddr_by_nodeid(uint16_t nodeid, uip_ipaddr_t * addr)
{
#if DIFF_DOMAIN == 1
    uip_ip6addr(addr, 0xbbbb, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, nodeid);
#else
    uip_ip6addr(addr, 0xaaaa, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, nodeid);
#endif
}

//static clock_time_t
//time_diff(clock_time_t tm, clock_time_t cur)
//{
//    clock_time_t diff, max;
//
//    max = ~0;
//    if(tm < cur) {
//        diff = cur - tm;
//    } else {
//        diff = max + cur - tm;
//    }
//    return diff;
//}

PROCESS_THREAD(shell_send_process, ev, data)
{
  static struct uip_udp_conn *conn;
  static struct etimer et;

  static char buf[MAX_BUF_SIZE];
  static int len;
  
  static uint16_t szTPDU;
  static uint16_t nSent, szFile;
  static uint16_t nSeq;

#if PMPD_ENABLED == 1
  static uint16_t maxTPDU;
#endif


  PROCESS_BEGIN();
  /* parse arguments */
  const char *nextptr;
  uint16_t nodeid = shell_strtolong(data, &nextptr);
  szFile = shell_strtolong(nextptr, &nextptr);
#if PMPD_ENABLED == 0
  szTPDU = shell_strtolong(nextptr, &nextptr);
#endif

#if PMPD_ENABLED == 1
  /* Attach PMPD */
  if(pmpd_attach_process(process_current)) {
    printf("APP: pmpd OK\n");
  } else {
    printf("APP: pmpd Failed\n");
    PROCESS_EXIT();
  }
#endif

  /* generate IP destaddr by nodeid then establish connection */
  uip_ipaddr_t dest_addr;
  set_ipaddr_by_nodeid(nodeid, &dest_addr);  

  conn = udp_new(&dest_addr, UIP_HTONS(3000), NULL);
  if(conn) {
    /* prepare transmitting */
    udp_bind(conn, UIP_HTONS(3001));
    nSent = 0;
    nSeq = 1; // starting from 1 not 0
    etimer_set(&et, CLOCK_SECOND * RETRANS_TIMER);

    /* start transmitting start */
    printf("Timing: start %lu\n", clock_time());
    while(nSent < szFile) {
#if PMPD_ENABLED == 1
      if(maxTPDU == 0) {
        szTPDU = pmpd_get_max_payload(&dest_addr);
      } else {
        szTPDU = maxTPDU;
      }
#endif
      if (szTPDU > MAX_BUF_SIZE) {
        printf("APP: OOps. Buffer is not big enough.\n");
        break;
      }
      /* generate fake file data */
      uint16_t i;
      for(i = 0; i < szTPDU; i++) {
        buf[i] = '0' + (i % 8);
      }

      /* fill seq */
      sprintf(buf, "%03u", nSeq);
      buf[3] = '0';

      /* set length of buffer
       * the minimum size is 3 due to seq
       */
      if((szFile - nSent) <= 3) {
        len = 3;
      } else if((szFile - nSent) < szTPDU) {
        len = szFile - nSent;
      } else {
        len = szTPDU;
      }

      printf("APP: sending %u packet strlen buf %u\n", nSeq, len);
      printf("Timing: sending %lu\n", clock_time());

      uip_udp_packet_send(conn, buf, len);

      /* start retrans timer */
      etimer_restart(&et);

#if PMPD_ENABLED == 1
      // check pmpd and resend immediately if changed
      if(szTPDU != pmpd_get_max_payload(&dest_addr)) {
        printf("APP: resend immediately as max_payload changed!\n");
        continue;
      }

      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || (ev == tcpip_event)
                               || (ev == pmpd_event));
#else
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || (ev == tcpip_event));
#endif

      if(ev == tcpip_event) {
        char *appdata = (char *)uip_appdata;
        char tmp[7];

        sprintf(buf, "%c%c%c%c%c%c", appdata[0], appdata[1], appdata[2],
                appdata[3], appdata[4], appdata[5]);
        buf[6] = 0;
        sprintf(tmp, "ACK%03u", nSeq);
        printf("APP: seq = %s, ack = %s\n", &tmp[3], &buf[3]);
        if(strcmp(buf, tmp) == 0) {
#if PMPD_ENABLED == 1
          // receive first ack then save max_payload
          if(nSeq == 1) {
            maxTPDU = szTPDU;
          }
#endif
          nSeq++;
          nSent += len;
          printf("Timing: ACKed %lu\n", clock_time());
        }
#if PMPD_ENABLED == 1
      } else if(ev == pmpd_event) {
        printf("APP: pmpd update!\n");
#endif
      } else {
        printf("APP: timer expired\n");
      }
    }

    /* finish transmitting */
    etimer_stop(&et);
#if PMPD_ENABLED == 1
    maxTPDU = 0;
#endif
    printf("Timing: end %lu\n", clock_time());
    
    /* remove connection */
    conn->lport = 0;
    printf("APP: connection removed\n");
  } else {
    printf("APP: cannot establish connection\n");
  }

#if PMPD_ENABLED == 1
  /* detach pmpd */
  if(pmpd_detach_process(process_current)) {
    printf("APP: detached from pmpd\n");
  }
#endif

  PROCESS_END();
}

PROCESS_THREAD(shell_check_process, ev, data)
{
  PROCESS_BEGIN();
  
  /* parse arguments */
  const char *nextptr;
  uint16_t nodeid = shell_strtolong(data, &nextptr);
  uip_ipaddr_t dest_addr;
  set_ipaddr_by_nodeid(nodeid, &dest_addr);

  /* say "hello" */
  struct uip_udp_conn *conn;
  conn = udp_new(&dest_addr, UIP_HTONS(3000), NULL);
  udp_bind(conn, UIP_HTONS(3001));

  if(conn) {
    char buf[16];
    sprintf(buf, "hello there");
    uip_udp_packet_send(conn, buf, strlen(buf));
    conn->lport = 0;
  }

  PROCESS_END();
}

PROCESS_THREAD(shell_showrt_process, ev, data)
{
  PROCESS_BEGIN();

  uip_ds6_route_t *r = uip_ds6_route_head();
  while (uip_ds6_route_next(r)) {
    uip_debug_ipaddr_print(&r->ipaddr);
    printf(" via ");
    uip_debug_ipaddr_print(uip_ds6_route_nexthop(r));
    printf("\n");
  }

  PROCESS_END();
}

void
shell_pmpd_init(void)
{
  shell_register_command(&send_command);
  shell_register_command(&check_command);
  shell_register_command(&showrt_command);
}
