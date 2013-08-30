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
 *         Header file for the IPv6 multicast protocol described
 *         in the "Multicast Protocol for Low power and Lossy Networks (MPL)"
 *         internet draft.
 *
 *         The current version of the draft can always be found in
 *         http://tools.ietf.org/html/draft-ietf-roll-trickle-mcast
 *
 *         This implementation is based on the draft version -04
 *
 * \author Zhen-Huan Hwang <praise.digital@gmail.com>
 */

#ifndef __ROLL_MPL_H__
#define __ROLL_MPL_H__

#if 0
/* defined in net/uip-icmp6.h */
#define ICMP6_MPL  200 /* same as ICMP6_TRICKLE_MCAST */
#endif

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/uip-ds6.h"
#include "lib/trickle-timer.h"
#include "sys/node-id.h"

#define HBHO_MPL_OPT_TYPE     0x6D  /* 01 1 01101 */

// 5.1
extern const uip_ipaddr_t  all_mpl_forwarders;
#define ALL_MPL_FORWARDERS  (&all_mpl_forwarders)

// 5.4
#ifndef MPL_CONF_PROACTIVE_FORWARDING
#define PROACTIVE_FORWARDING  1  /* default TRUE */
#else
#define PROACTIVE_FROWARDING MPL_CONF_PROACTIVE_FORWARDING
#endif

#ifndef MPL_CONF_SEED_SET_ENTRY_LIFETIME
/* default: 30 minutes */
#define SEED_SET_ENTRY_LIFETIME           (1800*CLOCK_SECOND)
#else
#define SEED_SET_ENTRY_LIFETIME MPL_CONF_SEED_SET_ENTRY_LIFE_TIME
#endif

// 5.5 MPL Trickle Parameters 
/* default: "10 times the worst-case link-layer latency"
 * assume 3 ms latency, the following is around 31.25ms on Z1 */
#ifndef MPL_CONF_DEFAULT_IMIN
#define DEFAULT_IMIN                      (CLOCK_SECOND/30)
#else
#define DEFAULT_IMIN                      MPL_CONF_DEFAULT_IMIN
#endif

#ifndef MPL_CONF_DATA_MESSAGE_IMIN
#define DATA_MESSAGE_IMIN                 DEFAULT_IMIN
#else
#define DATA_MESSAGE_IMIN                 MPL_CONF_DATA_MESSAGE_IMIN
#endif

/* default |IMAX| = IMIN, but Trikle library does not allow IMAX = 0,
 * so we set default IMAX = 1 (e.g. |IMAX| = 2*IMIN)  */
#ifndef MPL_CONF_DATA_MESSAGE_IMAX
#define DATA_MESSAGE_IMAX                 1
#else
#define DATA_MESSAGE_IMAX                 MPL_CONF_DATA_MESSAGE_IMAX
#endif

#ifndef MPL_CONF_DATA_MESSAGE_K
#define DATA_MESSAGE_K                    5 /* default: 5 */
#else
#define DATA_MESSAGE_K                    MPL_CONF_DATA_MESSAGE_K
#endif

#ifndef MPL_CONF_DATA_MESSAGE_TIMER_EXPIRATIONS
#define DATA_MESSAGE_TIMER_EXPIRATIONS    3 /* default: 3 */
#else
#define DATA_MESSAGE_TIMER_EXPIRATIONS    MPL_CONF_DATA_MESSAGE_TIMER_EXPIRATIONS
#endif

#ifndef MPL_CONF_CONTROL_MESSAGE_IMIN
#define CONTROL_MESSAGE_IMIN              DEFAULT_IMIN
#else
#define CONTROL_MESSAGE_IMIN              MPL_CONF_CONTROL_MESSAGE_IMIN
#endif

/* default 5 min (300 s). Must calculate by ourself.
 * IMAX = log2(300s/IMIN) */
#ifndef MPL_CONF_CONTROL_MESSAGE_IMAX
#if defined(MPL_CONF_CONTROL_MESSAGE_IMIN) || defined(MPL_CONF_DEFAULT_IMIN)
#error You have a custom Control or default IMIN, then you must \
       calculate Control IMAX = log2(300s/IMIN) or specify your own IMAX.
#endif
#define CONTROL_MESSAGE_IMAX              13 /* 300/0.03125=9600 ~ 8192 */
#else
#define CONTROL_MESSAGE_IMAX              MPL_CONF_CONTROL_MESSAGE_IMAX
#endif

#ifndef MPL_CONF_CONTROL_MESSAGE_K
#define CONTROL_MESSAGE_K                 1 /* default: 1 */
#else
#define CONTROL_MESSAGE_K                 MPL_CONF_CONTROL_MESSAGE_K
#endif

#ifndef MPL_CONF_CONTROL_MESSAGE_TIMER_EXPIRATIONS
#define CONTROL_MESSAGE_TIMER_EXPIRATIONS 10 /* default: 10 */
#else
#define CONTROL_MESSAGE_TIMER_EXPIRATIONS MPL_CONF_CONTROL_MESSAGE_TIMER_EXPIRATIONS
#endif

#define MPL_MAX_PACKET_SIZE     (UIP_BUFSIZE - UIP_LLH_LEN)
#define MPL_MAX_DOMAINS         2
#define MPL_MAX_SEEDS           2
/* Mind the size of `buffered' in trickle_cb_data_out()
 * and roll_mpl_icmp_input()  */
#define MPL_MAX_MESSAGE_BUFFER  6
#define INDEX_NOT_FOUND         255

#ifndef MPL_CONF_OUR_SEED_ID_TYPE
/* Default to use IPv6 address as seed id of this node */
#define MPL_OUR_SEED_ID_TYPE       0
#else
#define MPL_OUT_SEED_ID_TYPE MPL_CONF_OUR_SEED_ID_TYPE
#endif

#if MPL_OUR_SEED_ID_TYPE == 0
#define HBHO_TOTAL_LEN  8
#define HBHO_MPL_LEN    4
#define HBHO_S_FLAG     0x0
#elif MPL_OUR_SEED_ID_TYPE == 1
#define HBHO_TOTAL_LEN  8
#define HBHO_MPL_LEN    6
#define HBHO_S_FLAG     0x1
#elif MPL_OUR_SEED_ID_TYPE == 2
#define HBHO_TOTAL_LEN  16
#define HBHO_MPL_LEN    12
#define HBHO_S_FLAG     0x2
#elif MPL_OUR_SEED_ID_TYPE == 3
#define HBHO_TOTAL_LEN  24
#define HBHO_MPL_LEN    20
#define HBHO_S_FLAG     0x3
#endif

/* Domain Set */
struct roll_mpl_domain_set {
  uint8_t inuse;
  uip_ipaddr_t domain_address;
  struct trickle_timer control_timer;
  uint8_t timer_expires;
};

/* Seed ID */
union seed_id_u {
  uip_ipaddr_t ip6;
  uint8_t u128[16];
  uint8_t u64[8];
  uint8_t u16[2];
};

/* Seed Set */
struct roll_mpl_seed_set {
  uint8_t inuse;
  uint8_t domain_set_index;
  uint8_t s;
  union seed_id_u seed_id;
  uint8_t min_sequence;
  struct timer lifetime;
  uint8_t count;
};

struct roll_mpl_message_buffer {
  uint8_t  inuse;
  uint8_t  sequence;
  uint8_t  seed_index;
  struct trickle_timer t;
  uint8_t  timer_expires;
  uint16_t len;
  uint8_t  data[MPL_MAX_PACKET_SIZE];
};

struct roll_mpl_hbho_hdr {
  uint8_t type;
  uint8_t len;
  uint8_t flags;
  uint8_t sequence;
  union seed_id_u seed_id;
};


/* API hooks */
void    roll_mpl_init(); /* invoked by uip6.c:uip_init() */
uint8_t roll_mpl_in(); /* invoked by uip6.c:uip_process() */
void    roll_mpl_out(); /* invoked by uip-udp-packet.c:uip_udp_packet_send() */
void    roll_mpl_icmp_input(); /* invoked by tcpip.h:tcpip_icmp6_call() */

/* for uip_process() */
uint8_t roll_mpl_is_subscribed(const uip_ipaddr_t const *a);

/* our API */
void  subscribe_domain(const uip_ipaddr_t const *da);
void  unsubscribe_domain(const uip_ipaddr_t const *da);
void  roll_mpl_set_last_seq(uint8_t seq);
void  roll_mpl_set_seed_id(const union seed_id_u const *id);

/* assuming a 32 bytes payload */
#define MPL_MAX_TRACE (MAX_PAYLOAD-3)

struct roll_mpl_payload {
  uint8_t sequence;
  uint8_t hops;
  uint8_t path[MPL_MAX_TRACE + 1];
};
void  roll_mpl_set_payload(void *p, uint8_t sequence);
void  roll_mpl_add_trace(void);
void  roll_mpl_show_payload(void *p);


struct roll_mpl_stats {
  uint16_t icmp_sent;
  uint16_t icmp_in;
  uint16_t icmp_drop;
  uint16_t data_sent;
  uint16_t data_in;
  uint16_t data_drop;
};

#if UIP_MCAST6_STATS
extern struct roll_mpl_stats roll_mpl_stat;
#endif
void  roll_mpl_show_stat(void);

#endif /* __ROLL_MPL_H__ */

