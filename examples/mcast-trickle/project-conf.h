/*
 * Copyright (c) 2013, Aalto University
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
 *         Project specific configuration defines for the MPL
 *         multicast examples.
 *
 * \author Zhen-Huan Hwang <praise.digital@gmail.com>
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#include "net/multicast6/uip-mcast6-engines.h"

/* Change this to switch engines. Engine codes in uip-mcast6-engines.h */
#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_TRICKLE

/* How many test messages should be generated. */
#define ITERATIONS    100
/* Waiting time after last iteration or last reception before
 * test applications exit. This should be long enough to allow
 * the last packet to reach every node. */
#define AFTERMATH     (40*CLOCK_SECOND)
/* The amount of application payload in test messages. */
#define SEND_PAYLOAD  32 /* max 56 for MPL */

/* cheat : trace a Data Message using its payload.
 * Note: mind the payload/packet size. see also roll-mpl/trickle.h
 * Note: this disables UDP checksuming and modifies payload en route. */
//#define MCAST_PAYLOAD_TRACING

/* Duty-cycling algorithm selection. nullrdc_ or contikimac_
 * The default is contikimac_ for z1 */
#undef  NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC nullrdc_driver

/* default is 0 for z1 */
#undef  NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK 1

#undef  ENERGEST_CONF_ON
#define ENERGEST_CONF_ON 1

/* change CSMA backoff */
//#define CSMA_CONF_BACKOFF_HACK 1
//#define CSMA_CONF_PRINT_DEFAULT_TIMEBASE 1
/* print a message when CSMA sets retry timer */
//#define CSMA_CONF_SHOW_WAIT 1

/* statistics */
#define UIP_MCAST6_CONF_STATS  1

/* -04 parameters - see roll-mpl.h for defaults */
//#define MPL_CONF_PROACTIVE_FORWARDING   x
//#define MPL_CONF_OUR_SEED_ID_TYPE       x
//#define MPL_CONF_DEFAULT_IMIN           x
//#define MPL_CONF_DATA_MESSAGE_IMIN      x
//#define MPL_CONF_DATA_MESSAGE_IMAX      x
//#define MPL_CONF_DATA_MESSAGE_K         x
//#define MPL_CONF_DATA_MESSAGE_TIMER_EXPIRATIONS  x
//#define MPL_CONF_CONTROL_MESSAGE_IMIN   x
//#define MPL_CONF_CONTROL_MESSAGE_IMAX   x
//#define MPL_CONF_CONTROL_MESSAGE_K      x
//#define MPL_CONF_CONTROL_MESSAGE_TIMER_EXPIRATIONS  x

/* -00 parameters - see roll-trickle.h for defaults */
//#define ROLL_TRICKLE_CONF_SET_M_BIT   1
/* aggressive */
//#define ROLL_TRICKLE_CONF_IMIN_0      32
//#define ROLL_TRICKLE_CONF_IMAX_0      1
//#define ROLL_TRICKLE_CONF_K_0         255
//#define ROLL_TRICKLE_CONF_T_ACTIVE_0  3
//#define ROLL_TRICKLE_CONF_T_DWELL_0   11
/* conservative */
/* For Imin: Use 16 over NullRDC, 64 over Contiki MAC */
#define ROLL_TRICKLE_CONF_IMIN_1      16
#define ROLL_TRICKLE_CONF_IMAX_1      14
//#define ROLL_TRICKLE_CONF_K_1         2
//#define ROLL_TRICKLE_CONF_T_ACTIVE_1  3
//#define ROLL_TRICKLE_CONF_T_DWELL_1   12



#if UIP_MCAST6_CONF_ENGINE == UIP_MCAST6_ENGINE_MPL
#define MAX_PAYLOAD    (UIP_BUFSIZE-UIP_LLH_LEN-UIP_IPUDPH_LEN-HBHO_TOTAL_LEN)
#define PAYLOAD          struct roll_mpl_payload
#define show_stat(x)     roll_mpl_show_stat(x)
#define show_payload(x)  roll_mpl_show_payload(x)
#define set_payload(x,y) roll_mpl_set_payload(x,y)
#elif UIP_MCAST6_CONF_ENGINE == UIP_MCAST6_ENGINE_TRICKLE
#define MAX_PAYLOAD      120
#define PAYLOAD          struct roll_trickle_payload
#define show_stat(x)     roll_trickle_show_stat(x)
#define show_payload(x)  roll_trickle_show_payload(x)
#define set_payload(x,y) roll_trickle_set_payload(x,y)
#define subscribe_domain(x)
#define unsubscribe_domain(x)
#endif

#ifdef MCAST_PAYLOAD_TRACING
#undef  UIP_CONF_UDP_CHECKSUMS
#define UIP_CONF_UDP_CHECKSUMS 0
#define MPL_PAYLOAD_TRACING
#define ROLL_TRICKLE_PAYLOAD_TRACING
#endif

#undef UIP_CONF_IPV6
#undef UIP_CONF_IPV6_RPL
#undef UIP_CONF_ND6_SEND_RA
#undef UIP_CONF_ROUTER
#undef UIP_CONF_DS6_MCAST_ROUTES
#define UIP_CONF_IPV6             1
#define UIP_CONF_IPV6_RPL         0
#define UIP_CONF_ND6_SEND_RA      0
#define UIP_CONF_ROUTER           1
#define UIP_CONF_DS6_MCAST_ROUTES 0

/* Code/RAM footprint savings so that things will fit on our device */
#undef UIP_CONF_DS6_NBR_NBU
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_NBR_NBU     10
#define UIP_CONF_DS6_ROUTE_NBU   10

#undef UIP_CONF_TCP
#define UIP_CONF_TCP              0

#endif /* PROJECT_CONF_H_ */
