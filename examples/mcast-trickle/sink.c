/*
 * Copyright (c) 2013, Aalto University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *         This node is part of the MPL multicast example. It is a sink
 *         node and receive multicast data messages.
 *
 * \author Zhen-Huan Hwang <praise.digital@gmail.com>
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#define DEBUG  DEBUG_NONE
#include "net/uip-debug.h"

#include "dev/leds.h"
#include "net/multicast6/uip-mcast6.h"

#if !UIP_CONF_IPV6
#error I need UIP_CONF_IPV6
#endif
#if !UIP_CONF_ROUTER
#error I need UIP_CONF_ROUTER
#endif

#define MCAST_SINK_PORT 3001

#define UIP_IP_BUF    ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct uip_udp_conn * mcast_conn;
static unsigned int count = 0;
static unsigned int low = 1;
static unsigned long received[16];

#define R_GOT(s)  (received[s/32]|=(0x1l<<(s%32)))
#define R_HAS(s)  ((received[s/32]&(0x1l<<(s%32)))!=0)
#define R_CLR(s)  (memset(received, 0, 64))

PROCESS(mcast_sink_process, "Sink");
AUTOSTART_PROCESSES(&mcast_sink_process);

static void init_net(void)
{
  uip_ipaddr_t ipaddr;
  uint8_t i;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  PRINTF("Our IPv6 addresses:\n");
  for (i = 0;i < UIP_DS6_ADDR_NB;i ++) {
    if (uip_ds6_if.addr_list[i].isused &&
        (uip_ds6_if.addr_list[i].state == ADDR_TENTATIVE ||
         uip_ds6_if.addr_list[i].state == ADDR_PREFERRED)) {
      PRINTF("[%d] ", i);
      PRINT6ADDR(&(uip_ds6_if.addr_list[i].ipaddr));
      PRINTF("\n");
      if (uip_ds6_if.addr_list[i].state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}

static void prepare_mcast(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr,0xff1e,0,0,0,0,0,0x89,0xABCD);
  subscribe_domain(&ipaddr);
  mcast_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  udp_bind(mcast_conn, UIP_HTONS(MCAST_SINK_PORT));
}

static void recv_mcast(void)
{
  PAYLOAD *p;
  uint8_t seq;

  leds_on(LEDS_RED);
  if(uip_newdata()) {
    count ++;
    p = (PAYLOAD *)uip_appdata;
    seq = p->sequence;
    R_GOT(seq);
    while (R_HAS(low)) {
      low ++;
    }
    PRINTF("RECV: seq %u low %u TTL %u total %u\n",
      seq, low, UIP_IP_BUF->ttl, count);
    show_payload(p);
  }
  leds_off(LEDS_RED);
}

static void tail(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr,0xff1e,0,0,0,0,0,0x89,0xabcd);
  unsubscribe_domain(&ipaddr);
  uip_ip6addr(&ipaddr,0xff03,0,0,0,0,0,0,0xfc);
  unsubscribe_domain(&ipaddr);
}

static void show_energy()
{
#if ENERGEST_CONF_ON
  printf("ENE CPU %ld LPM %ld TRA %ld LIS %ld\n",
    energest_total_time[ENERGEST_TYPE_CPU].current,
    energest_total_time[ENERGEST_TYPE_LPM].current,
    energest_total_time[ENERGEST_TYPE_TRANSMIT].current,
    energest_total_time[ENERGEST_TYPE_LISTEN].current);
#endif
}

PROCESS_THREAD(mcast_sink_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  init_net();
  prepare_mcast();
  R_CLR();
  leds_off(LEDS_ALL);
  etimer_set(&et, AFTERMATH);
  //show_energy();
  while (1) {
    PROCESS_YIELD();
    leds_on(LEDS_GREEN);
    if (ev == tcpip_event) {
      recv_mcast();
      etimer_restart(&et);
    } else if (etimer_expired(&et) && count > 0) {
      leds_off(LEDS_GREEN);
      break;
    }
    leds_off(LEDS_GREEN);
  }
  etimer_restart(&et);
  leds_on(LEDS_YELLOW);
  while (1) {
    PROCESS_YIELD();
    if (etimer_expired(&et)) {
      break;
    }
  }
  leds_off(LEDS_YELLOW);
  tail();
  show_energy();
  show_stat();
  PROCESS_END();
}

