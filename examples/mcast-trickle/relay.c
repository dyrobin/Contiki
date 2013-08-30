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
 *         This node is part of the MPL multicast example. It is an relay
 *         node and forwards multicast messages.
 *
 * \author Zhen-Huan Hwang <praise.digital@gmail.com>
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#define DEBUG  DEBUG_NONE
#include "net/uip-debug.h"

#include "net/multicast6/uip-mcast6.h"


#if !UIP_CONF_IPV6
#error I need UIP_CONF_IPV6
#endif
#if !UIP_CONF_ROUTER
#error I need UIP_CONF_ROUTER
#endif


PROCESS(mcast_relay_process, "Relay");
AUTOSTART_PROCESSES(&mcast_relay_process);

static void prepare_mcast(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr,0xff1e,0,0,0,0,0,0x89,0xABCD);
  subscribe_domain(&ipaddr);
}

static void tail(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr,0xff1e,0,0,0,0,0,0x89,0xabcd);
  unsubscribe_domain(&ipaddr);
  uip_ip6addr(&ipaddr,0xff03,0,0,0,0,0,0,0xfc);
  unsubscribe_domain(&ipaddr);
}

PROCESS_THREAD(mcast_relay_process, ev, data)
{
  PROCESS_BEGIN();

  prepare_mcast();
  while (1) {
    PROCESS_YIELD();
  }
  tail();
  PROCESS_END();
}

