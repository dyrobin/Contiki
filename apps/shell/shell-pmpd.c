/**
 * This shell is used to test PMPD (Path Maximum Transport Protocol Data Unit Discovery)
 * Usage:
 *    send <nodeid> <filesize> <TPDU>
 *        send <filesize> bytes to <nodeid> using <TPDU>
 *        <TPDU> will not used if PMPD is on 
 *    ping <nodeid>
 *        check if <nodeid> can be reached or not
 *    show <type>
 *        show routing tables (type 1) or collected data (type 2)
 *    bgtfc <on/off> <interval>
 *        activate/deactivate background traffic with random % <interval>
 *
 * Authored by Yang Deng <yang.deng@aalto.fi>
 */
#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/ipv6/sicslowpan.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h" 
#include <string.h>
#include <stdio.h>
#include "sys/clock.h"
#include "lib/random.h"


#if PMPD_ENABLED == 1
#include "net/ipv6/pmpd.h"
#endif

#include "net/ip/uip-debug.h"

#define 	MAX_BUF_SIZE 	512
#define 	RETRANS_TIMER	3
#define   MAX_RETX_NUM  5

PROCESS(shell_send_process, "send");
SHELL_COMMAND(send_command,
              "send",
              "send <nodeid> <datasize> <TPDU>: send data", 
              &shell_send_process);

PROCESS(shell_ping_process, "ping");
SHELL_COMMAND(ping_command,
              "ping",
              "ping <nodeid>: check reachability", 
              &shell_ping_process);

PROCESS(shell_show_process, "show");
SHELL_COMMAND(show_command,
              "show",
              "show <type>: type 1 is routing table; type 2 is collected data", 
              &shell_show_process);

PROCESS(shell_bgtfc_process, "bgtfc");
SHELL_COMMAND(bgtfc_command,
              "bgtfc",
              "bgtfc <on/off> <interval>: [de]activate background traffic",
              &shell_bgtfc_process);


PROCESS(bgtfcgen, "background traffic generator");


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
  static uip_ipaddr_t dest_addr;
  static struct etimer et;
  static clock_time_t tStart;
  static clock_time_t tEnd;

  static char buf[MAX_BUF_SIZE];
  static int len;
  
  static uint16_t szTPDU;
  static uint16_t nSent, szFile;
  static uint16_t nSeq;
  static uint8_t nTx;
  static uint8_t nRetx;

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
  if (szTPDU == 0) {
    printf("shell: <TPDU> is missing.\n");
    PROCESS_EXIT();
  }
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
  set_ipaddr_by_nodeid(nodeid, &dest_addr);  

  conn = udp_new(&dest_addr, UIP_HTONS(61616), NULL);
  if(conn) {
    /* prepare transmitting */
    udp_bind(conn, UIP_HTONS(61617));
    nSent = 0;
    nSeq = 1; // starting from 1 not 0
    nTx = 0;
    nRetx = 0;
    etimer_set(&et, CLOCK_SECOND * RETRANS_TIMER);

    /* start transmitting start */
    printf("APP: start sending %u %u using %u\n", nodeid, szFile, szTPDU);
    tStart = clock_time();
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

//      printf("APP: sending packet %u(%u)\n", nSeq, len);
//      printf("Timing: sending %lu\n", clock_time());

      uip_udp_packet_send(conn, buf, len);
      nTx++;

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
//        printf("APP: seq = %s, ack = %s\n", &tmp[3], &buf[3]);
        if(strcmp(buf, tmp) == 0) {
#if PMPD_ENABLED == 1
          // receive first ack then save max_payload
          if(nSeq == 1) {
            maxTPDU = szTPDU;
          }
#endif
          nSeq++;
          nSent += len;
          nTx = 0;
//          printf("Timing: ACKed %lu\n", clock_time());
        }
#if PMPD_ENABLED == 1
      } else if(ev == pmpd_event) {
        printf("APP: pmpd update!\n");
#endif
      } else {
        printf("APP: timer expired\n");
        nRetx ++;
        if (nTx >= MAX_RETX_NUM) {
          break;
        }
      }
    }

    /* finish transmitting */
    tEnd = clock_time();

    etimer_stop(&et);
#if PMPD_ENABLED == 1
    maxTPDU = 0;
#endif
    if (nSent >= szFile) {
      printf("APP: Done. Time: %lu Packets: %u Retrans: %u\n", tEnd - tStart, nSeq - 1, nRetx);
    } else {
      printf("APP: Failed.");
    }
    /* remove connection */
    uip_udp_remove(conn);
//    printf("APP: connection removed\n");
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

PROCESS_THREAD(shell_ping_process, ev, data)
{
  PROCESS_BEGIN();
 
  static struct etimer et;
  static struct uip_udp_conn *conn;

  /* parse arguments */
  uint16_t nodeid = shell_strtolong(data, NULL);
  uip_ipaddr_t dest_addr;
  set_ipaddr_by_nodeid(nodeid, &dest_addr);

  /* say "hello" */
  conn = udp_new(&dest_addr, UIP_HTONS(61616), NULL);
  udp_bind(conn, UIP_HTONS(61617));

  if(conn) {
    char buf[16];
    sprintf(buf, "hello there");
    uip_udp_packet_send(conn, buf, strlen(buf));
    etimer_set(&et, CLOCK_SECOND * RETRANS_TIMER); 

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || (ev == tcpip_event));

    if (ev == tcpip_event) {
        char *appdata = (char *)uip_appdata;
        if (appdata[0] == 'h') {
          printf("OK.\n");
        } else {
          printf("Failed.\n");
        }
    }else {
        printf("Failed.\n");
    }

    etimer_stop(&et);
    uip_udp_remove(conn);
  }

  PROCESS_END();
}

PROCESS_THREAD(shell_show_process, ev, data)
{
  PROCESS_BEGIN();
  
//  printf("dco: %x %x %x %x \n", DCOCTL, BCSCTL1, BCSCTL2, BCSCTL3);

  /* parse arguments */
  uint16_t type = shell_strtolong(data, NULL);
  
  if (type == 1) {
    printf("Routing Tables\n");
    uip_debug_ipaddr_print(uip_ds6_defrt_choose());
    printf(" [DFT]\n");

    uip_ds6_route_t *r = uip_ds6_route_head();
    while (r != NULL) {
      uip_debug_ipaddr_print(&r->ipaddr);
      printf(" via ");
      uip_debug_ipaddr_print(uip_ds6_route_nexthop(r));
      printf("\n");
      r = uip_ds6_route_next(r);
    }
  } else if (type == 2) {
    flow_filter_print();
  } else {
    printf("Unknown type.\n");
  }

  PROCESS_END();
}

PROCESS_THREAD(shell_bgtfc_process, ev, data)
{
  static uint16_t intvl;

  PROCESS_BEGIN();

  /* parse arguments */
  const char *nextptr;
  uint16_t on_off = shell_strtolong(data, &nextptr);
  intvl = shell_strtolong(nextptr, &nextptr);

  if (on_off) {
    process_start(&bgtfcgen, (char *)&intvl);
  } else {
    process_exit(&bgtfcgen);
  }

  PROCESS_END();
}

PROCESS_THREAD(bgtfcgen, ev, data)
{
  static struct uip_udp_conn *conn;
  static struct etimer et;
  static uint16_t intvl;

  PROCESS_BEGIN();

  /* send dummy message to port 0, where message will be dropped. */
  uip_ipaddr_t dest_addr;
  set_ipaddr_by_nodeid(1, &dest_addr);
  conn = udp_new(&dest_addr, UIP_HTONS(0), NULL);
  if (data) intvl = *(uint16_t *)data;
  printf("bgtfc activated with interval %d\n", intvl);

  while (1) {
    uint16_t delay = random_rand() % intvl + 1;
//    printf("delay: %d intvl: %d\n", delay, intvl);
    etimer_set(&et, CLOCK_SECOND * delay);
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER || 
                             ev == PROCESS_EVENT_EXIT);
    if (ev == PROCESS_EVENT_EXIT) {
      etimer_stop(&et);
      uip_udp_remove(conn);
      break;
    } else {
      uip_udp_packet_send(conn, "dummy", 5);
    }
  }

  PROCESS_END();
}

void
shell_pmpd_init(void)
{
  shell_register_command(&send_command);
  shell_register_command(&ping_command);
  shell_register_command(&show_command);
  shell_register_command(&bgtfc_command);
}
