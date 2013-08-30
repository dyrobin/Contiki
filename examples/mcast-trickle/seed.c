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
 *         This node is part of the MPL multicast example. It is a seed
 *         node and sends multicast data message periodically.
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

#define START_DELAY     (20*CLOCK_SECOND)
#define SEND_INTERVAL1  (CLOCK_SECOND)
#define SEND_INTERVAL2  SEND_INTERVAL1
#define SEND_TURNPOINT  ITERATIONS /* disabled */
#define MCAST_SINK_PORT 3001

static uint8_t sequence = 1;

static struct uip_udp_conn * mcast_conn;
static uint8_t buf[MAX_PAYLOAD];

PROCESS(mcast_seed_process, "Seed");
AUTOSTART_PROCESSES(&mcast_seed_process);

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

  uip_ip6addr(&ipaddr,0xff1e,0,0,0,0,0,0x89,0xabcd);
  mcast_conn = udp_new(&ipaddr, UIP_HTONS(MCAST_SINK_PORT), NULL);
  subscribe_domain(&ipaddr);
#if UIP_MCAST6_CONF_ENGINE == UIP_MCAST6_ENGINE_MPL
  roll_mpl_set_last_seq(0);
  roll_mpl_set_seed_id((union seed_id_u *)&ipaddr);
#endif
}

static void send_mcast(void)
{
  leds_on(LEDS_RED);
  printf("SEED %d\n", sequence);
  memset(buf, 0xff, MAX_PAYLOAD);
  set_payload(buf, sequence);
  uip_udp_packet_send(mcast_conn, buf, SEND_PAYLOAD);
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

static void show_energy(void)
{
#if ENERGEST_CONF_ON
  printf("ENE CPU %ld LPM %ld TRA %ld LIS %ld\n",
    energest_total_time[ENERGEST_TYPE_CPU].current,
    energest_total_time[ENERGEST_TYPE_LPM].current,
    energest_total_time[ENERGEST_TYPE_TRANSMIT].current,
    energest_total_time[ENERGEST_TYPE_LISTEN].current);
#endif
}

PROCESS_THREAD(mcast_seed_process, ev, data)
{
  static struct etimer et;
  static int count = 0;

  PROCESS_BEGIN();

  printf("Multicast engine: " UIP_MCAST6_ENGINE_STR "\n");
//  printf("MAX_PAYLOAD: %u\n", MAX_PAYLOAD);
//  printf("RTIMER_SECOND: %u\n", RTIMER_SECOND);
  init_net();
  prepare_mcast();
  leds_off(LEDS_ALL);
  etimer_set(&et, START_DELAY);
  //show_energy();
  while (1) {
    PROCESS_YIELD();
    leds_on(LEDS_GREEN);
    if (etimer_expired(&et)) {
      if (count < ITERATIONS) {
        send_mcast();
        sequence ++;
        count ++;
      } else {
        break;
      }
      leds_off(LEDS_GREEN);
      if (count >= SEND_TURNPOINT) {
        etimer_set(&et, SEND_INTERVAL2);
      } else {
        etimer_set(&et, SEND_INTERVAL1);
      }
    }
  }
  leds_off(LEDS_GREEN);
  etimer_set(&et, AFTERMATH);
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

