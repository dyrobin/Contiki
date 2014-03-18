#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/ipv6/uip-ds6.h"
#include <string.h>
#include <stdio.h>
#include "sys/clock.h"

#if PMPD_ENABLED == 1
#include "net/pmpd.h"
#endif

#define 	MAX_BUF_SIZE 	464
#define 	RETRANS_TIMER	5

PROCESS(shell_send_udp_process, "send_udp");
SHELL_COMMAND(send_udp_command,
              "send_udp",
              "send_udp: send udp to destination", &shell_send_udp_process);

PROCESS(shell_check_path_process, "check_path");
SHELL_COMMAND(check_path_command,
              "check_path",
              "check_path: check path to destination",
              &shell_check_path_process);

PROCESS(shell_show_routes_process, "show_routes");
SHELL_COMMAND(show_routes_command,
              "show_routes",
              "show_routes: show routing table", &shell_show_routes_process);

static void
set_s_addr(uint16_t s, uint16_t opt, uip_ipaddr_t * send_addr)
{
  if(opt == 1) {
#if DIFF_DOMAIN == 1
    uip_ip6addr(send_addr, 0xbbbb, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, s);
#else
    uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, s);
#endif
  } else {
    uint16_t hexanum = 0x7400;
    uint16_t var1, var2;

    var1 = hexanum + s;
    var2 = 257 * s;
    uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0x0212, var1, s, var2);
  }
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

PROCESS_THREAD(shell_send_udp_process, ev, data)
{
  const char *nextptr;

  static struct uip_udp_conn *conn;
  static uip_ipaddr_t dest_addr;
  static struct etimer et;

//    static clock_time_t start, end;

  static uint16_t max_payload;
  static uint16_t sent, data_size;
  static uint16_t seq, len;

  PROCESS_BEGIN();

#if PMPD_ENABLED == 1
  static uint16_t session_payload = 0;

  if(pmpd_attach_process(process_current)) {
    printf("APP: attached to pmpd \n");
  } else {
    printf("APP: pmpd proc list full \n");
    PROCESS_EXIT();
  }
#endif

  uint16_t s = shell_strtolong(data, &nextptr);

#if PMPD_ENABLED == 0
  max_payload = shell_strtolong(nextptr, &nextptr);
#endif
  data_size = shell_strtolong(nextptr, &nextptr);

  // prepare for sending data
  set_s_addr(s, 1, &dest_addr);

  seq = 1;                      // starting from 1 not 0

  conn = udp_new(&dest_addr, UIP_HTONS(3000), NULL);
  if(conn) {
    udp_bind(conn, UIP_HTONS(3001));

    sent = 0;
    etimer_set(&et, CLOCK_SECOND * RETRANS_TIMER);
    energest_type_set(ENERGEST_TYPE_TRANSMIT, 0);

    printf("Timing: start %lu\n", clock_time());
    while(sent < data_size) {
      char buf[MAX_BUF_SIZE];

#if PMPD_ENABLED == 1
      if(session_payload == 0) {
        max_payload = pmpd_get_max_payload(&dest_addr);
      } else {
        max_payload = session_payload;
      }
#endif
      uint16_t i;

      for(i = 0; i < max_payload; i++) {
        buf[i] = '0' + (i % 8);
      }
      buf[max_payload] = 0;

      // fill in seq
      sprintf(buf, "%03u", seq);
      buf[3] = '0';

      // compute length
      if((data_size - sent) <= 3) {
        len = 3;
      } else if((data_size - sent) < max_payload) {
        len = data_size - sent;
      } else {
        len = max_payload;
      }

      printf("APP: sending %u packet strlen buf %u\n", seq, len);

      etimer_restart(&et);
      printf("Timing: sending %lu\n", clock_time());

      uip_udp_packet_send(conn, buf, len);

#if PMPD_ENABLED == 1
      // check pmpd and resend immediately if changed
      if(session_payload == 0
         && max_payload != pmpd_get_max_payload(&dest_addr)) {
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
        sprintf(tmp, "ACK%03u", seq);
        printf("APP: seq = %s, ack = %s\n", &tmp[3], &buf[3]);
        if(!strcmp(buf, tmp)) {
#if PMPD_ENABLED == 1
          // receive first ack then save max_payload
          if(seq == 1) {
            session_payload = max_payload;
          }
#endif
          seq++;
          sent += len;
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
    etimer_stop(&et);
#if PMPD_ENABLED == 1
    session_payload = 0;
#endif
    printf("Timing: end %lu\n", clock_time());

  } else {
    printf("APP: cannot establish connection\n");
  }

  // remove connection
  if(conn) {
    printf("APP: connection removed\n");
    conn->lport = 0;
  }
#if PMPD_ENABLED == 1
  // detach pmpd
  if(pmpd_detach_process(process_current)) {
    printf("APP: detached from pmpd\n");
  }
#endif

  PROCESS_END();
}

PROCESS_THREAD(shell_check_path_process, ev, data)
{
  PROCESS_BEGIN();

  const char *nextptr;
  uip_ipaddr_t dest_addr;
  uint16_t s;

  s = shell_strtolong(data, &nextptr);
  //opt = shell_strtolong(nextptr, &nextptr);
  set_s_addr(s, 1, &dest_addr);

  struct uip_udp_conn *conn;

  conn = udp_new(&dest_addr, UIP_HTONS(3000), NULL);
  udp_bind(conn, UIP_HTONS(3001));

  if(conn) {
    char buf[64];

    sprintf(buf, "hello there");
    uip_udp_packet_send(conn, buf, strlen(buf));
    conn->lport = 0;
  }

  PROCESS_END();
}

PROCESS_THREAD(shell_show_routes_process, ev, data)
{
  static uip_ds6_route_t *r;

  PROCESS_BEGIN();
  //printf("Routes\n");

  r = uip_ds6_route_head();
  while (uip_ds6_route_next(r)) {
    uip_debug_ipaddr_print(&r->ipaddr);
    printf(" via ");
    uip_debug_ipaddr_print(uip_ds6_route_nexthop(r));
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
