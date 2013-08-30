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
 *         Implements the IPv6 multicast forwarding protocol described
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

/* TODO :
 *  1. Subscribe to Solicited-Node address (RFC 3513, 2.7)
 *  2. IPv6-in-IPv6 support (draft-04, 10.1)
 *  3. "Accept the message and update the corresponding MinSequence in the
 *      MPL Domain's Seed Set to 1 greater than the message's sequence
 *      number." (draft-04, 10.3) -- How to do this was unclear.
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/multicast6/uip-mcast6.h"

#ifdef __ROLL_MPL_H__
/* Build MPL code only if MPL engine is selected.
   This macro is defined in roll-mpl.h,
   if it gets included by uip-mcas6.h */

#include <string.h>
#include "lib/trickle-timer.h"

#define DEBUG DEBUG_NONE
#include "net/uip-debug.h"

#if UIP_MCAST6_STATS
struct roll_mpl_stats roll_mpl_stat;
#define STATS_ADD(x) roll_mpl_stat.x++
#else
#define STATS_ADD(x)
#endif

static struct roll_mpl_domain_set mpl_domain_set[MPL_MAX_DOMAINS];
static struct roll_mpl_seed_set mpl_seed_set[MPL_MAX_SEEDS];
static struct roll_mpl_message_buffer mpl_message_buffer[MPL_MAX_MESSAGE_BUFFER];

const uip_ipaddr_t  all_mpl_forwarders =
  { .u8 = {0xff,0x03,0,0,0,0,0,0,0,0,0,0,0,0,0,0xfc} };

/* Local functions */
static uint8_t lookup_domain_set(const uip_ipaddr_t const *a);
static void  unsubscribe_domain_index(uint8_t domain_index);
static uint8_t seed_id_is_equal(uint8_t s, union seed_id_u *a, union seed_id_u *b);
#define seed_id_copy(to,from) memcpy(to, from, sizeof(union seed_id_u))
#define seed_id_size(s)       ((s==3||s==0)?16:(s==2?8:(s==1?2:0)))
static void seed_id_from_buf(void *buf, uint8_t s, union seed_id_u *id);
static void seed_id_to_buf(void *buf, uint8_t s, union seed_id_u *id);
static uint8_t is_valid_mpl_hdr(const void const *pkt);
static uint8_t extract_seed_id(const void const *pkt, union seed_id_u *id);
static uint8_t lookup_seed_set(uint8_t s, union seed_id_u *id);
static uint8_t alloc_seed_set(void);
static uint8_t is_data_in_window(uint8_t sequence, uint8_t seed_index);
static uint8_t max_sequence_in_window(uint8_t seed_index);
static uint8_t alloc_data_buf(void);
static void reclaim(uint8_t seed_set_index);
static void trickle_cb_data_out(void *ptr, uint8_t suppress);
static void trickle_cb_icmp_out(void *ptr, uint8_t suppress);
static void reset_control(uint8_t domain);
static void reset_data(uint8_t data);

static uint8_t last_seq;
static union seed_id_u my_seed_id;

#define UIP_BUF       (uip_buf)
#define UIP_IP_BUF    ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_EXT_BUF       ((struct uip_ext_hdr *)&uip_buf[UIP_LLH_LEN + UIP_IPH_LEN])
#define UIP_EXT_BUF_NEXT  ((uint8_t *)&uip_buf[UIP_LLH_LEN + UIP_IPH_LEN + HBHO_TOTAL_LEN])
#define UIP_EXT_MPL_HBHO  ((struct roll_mpl_hbho_hdr *)&uip_buf[UIP_LLH_LEN + UIP_IPH_LEN + 2])
#define UIP_ICMP_BUF      ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ICMP_PAYLOAD  ((unsigned char *)&uip_buf[uip_l2_l3_icmp_hdr_len])

#define SEQ_VAL_IS_EQ(i1, i2) ((i1) == (i2))
#define SEQ_VAL_IS_LT(i1, i2) \
  ( \
    ((i1) != (i2)) && \
    ((((i1) < (i2)) && ((int16_t)((i2) - (i1)) < 0x0100)) || \
     (((i1) > (i2)) && ((int16_t)((i1) - (i2)) > 0x0100))) \
  )
#define SEQ_VAL_IS_GT(i1, i2) \
( \
  ((i1) != (i2)) && \
  ((((i1) < (i2)) && ((int16_t)((i2) - (i1)) > 0x0100)) || \
   (((i1) > (i2)) && ((int16_t)((i1) - (i2)) < 0x0100))) \
)
#define SEQ_VAL_ADD(s, n) ((uint8_t)((uint16_t)((s) + (n)) % 0x0080))

/*---------------------------------------------------------------------------*/
/* invoked by uip6.c:uip_init() */
void
roll_mpl_init() 
{
  uint8_t i;

  /* Interface Set: uip_ds6.c:uip_ds6_if , the only interface */
  memset(mpl_domain_set, 0, sizeof(struct roll_mpl_domain_set)*MPL_MAX_DOMAINS);
  memset(mpl_seed_set, 0, sizeof(struct roll_mpl_seed_set)*MPL_MAX_SEEDS);

  for (i = 0;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
    mpl_message_buffer[i].inuse = 0;
  }
  last_seq = 0;

#if UIP_MCAST6_STATS
  memset(&roll_mpl_stat, 0, sizeof(struct roll_mpl_stats));
#endif

  memset(&my_seed_id, 0, sizeof(union seed_id_u));
  subscribe_domain(ALL_MPL_FORWARDERS);
  
  PRINTF("MPL: Engine started.\n");
}
/*---------------------------------------------------------------------------*/
/* invoked by uip6.c:uip_process() */
uint8_t
roll_mpl_is_subscribed(const uip_ipaddr_t const *a)
{
  PRINTF("MPL: is_subscribed: ");
  PRINT6ADDR(a);
  if (lookup_domain_set(a) != INDEX_NOT_FOUND) {
    PRINTF(" ... YES\n");
    return 1;
  }
  PRINTF(" ... NO\n");
  return 0;
}
/*---------------------------------------------------------------------------*/
static uint8_t
lookup_domain_set(const uip_ipaddr_t const *a)
{
  uint8_t i;
  uip_ipaddr_t link_local;
  PRINTF("MPL: lookup_domain_set: ");
  PRINT6ADDR(a);
  for (i = 0;i < MPL_MAX_DOMAINS;i ++) {
    if (mpl_domain_set[i].inuse == 0) {
      continue;
    }
    if (uip_ipaddr_cmp(&mpl_domain_set[i].domain_address, a)) {
      PRINTF(" ... %d\n", i);
      return i;
    }
    /* 5.1 : subscribe also to Link-Local Domain Address */
    if ((a->u8[1] & 0x0f) == 0x02) {
      uip_ipaddr_copy(&link_local, &mpl_domain_set[i].domain_address);
      link_local.u8[1] &= 0xf0;
      link_local.u8[1] |= 0x02;
      if (uip_ipaddr_cmp(&link_local, a)) {
        PRINTF(" ... %d\n", i);
        return i;
      }
    }
  }
  PRINTF(" ... ERR NOT_FOUND\n");
  return INDEX_NOT_FOUND;
}
/*---------------------------------------------------------------------------*/
void
subscribe_domain(const uip_ipaddr_t const *da)
{
  uint8_t i;

  /* NOTE: no check for higher scope with same group-id */
  if (lookup_domain_set(da) == INDEX_NOT_FOUND) {
    PRINTF("MPL: subscribe ");
    PRINT6ADDR(da);
    /* allocate a Domain Set entry */
    for (i = 0;i < MPL_MAX_DOMAINS;i ++) {
      if (mpl_domain_set[i].inuse == 0) {
        break;
      }
    }
    if (i >= MPL_MAX_DOMAINS) {
      PRINTF(" ... FAILED!\n");
      return; /* failed */
    }
    mpl_domain_set[i].inuse = 1;
    uip_ipaddr_copy(&mpl_domain_set[i].domain_address, da);
    PRINTF(" ... domain set %d ... ", i);
#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
    trickle_timer_config(&mpl_domain_set[i].control_timer, CONTROL_MESSAGE_IMIN,
      CONTROL_MESSAGE_IMAX, CONTROL_MESSAGE_K);
    void *p = mpl_domain_set;
    uint32_t size = (uint32_t)i * sizeof(struct roll_mpl_domain_set);
    p += size;
    trickle_timer_set(&mpl_domain_set[i].control_timer, trickle_cb_icmp_out,
                      p);
    PRINTF("... Control Trickle");
#endif
    PRINTF("\n");
  }
}
/*---------------------------------------------------------------------------*/
void
unsubscribe_domain(const uip_ipaddr_t const *da)
{
  unsubscribe_domain_index(lookup_domain_set(da));
}
/*---------------------------------------------------------------------------*/
static void
unsubscribe_domain_index(uint8_t domain_index)
{
  uint8_t i, j;

  if (domain_index >= MPL_MAX_DOMAINS ||
      mpl_domain_set[domain_index].inuse == 0) {
    PRINTF("MPL: ERR unsubscribing unused domain %d\n", domain_index);
    return;
  }

#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
  trickle_timer_stop(&(mpl_domain_set[domain_index].control_timer));
#endif
  mpl_domain_set[domain_index].inuse = 0;

  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 0
        || mpl_seed_set[i].domain_set_index != domain_index) {
      continue;
    }
    for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
      if (mpl_message_buffer[j].inuse != 0 &&
          mpl_message_buffer[j].seed_index == i) {
        mpl_message_buffer[j].inuse = 0;
        trickle_timer_stop(&(mpl_message_buffer[j].t));
      }
    }
    mpl_seed_set[i].inuse = 0;
  }
  PRINTF("MPL: unsubscribed domain #%d\n", domain_index);
}
/*---------------------------------------------------------------------------*/
/* invoked by uip6.c:uip_process()
 * both for inbound and seeded outbound messages.
 */
uint8_t
roll_mpl_in()
{
  static uint8_t domain_set_index;
  static union seed_id_u seed_id;
  static uint8_t seed_type;
  static uint8_t seed_set_index;
  static uint8_t message_index;
  static uint8_t is_new_seed = 0;

  if (uip_len == 0) {
    goto drop;
  }

  /* uip_process() invoked roll_mpl_is_subscribed() before
   * passing the packet to us, and we may only seed to subscribed domains,
   * so there must be a Domain Set. */
  domain_set_index = lookup_domain_set(&UIP_IP_BUF->destipaddr);
  if (domain_set_index == INDEX_NOT_FOUND) {
    PRINTF("MPL: mpl_in: Strange, Domain Set not found ?!\n");
    PRINTF("MPL:   dest:");
    PRINT6ADDR(&UIP_IP_BUF->destipaddr);
    PRINTF("\n");
    goto drop;
  }

  if (UIP_IP_BUF->proto == UIP_PROTO_ICMP6 &&
      UIP_ICMP_BUF->type == ICMP6_MPL) {
    if (UIP_IP_BUF->ttl == 255) {
      PRINTF("MPL: mpl_in: neighbor ICMP\n");
      goto accept;
    }
    PRINTF("MPL: mpl_in: non-neighbor ICMP\n");
    goto drop;
  }

  /* look for the MPL Option in Hop-by-Hop Options header */
  if (!is_valid_mpl_hdr(UIP_IP_BUF)) {
    goto drop;
  }

  if ((UIP_EXT_MPL_HBHO->flags & 0x10) != 0) {
    PRINTF("MPL: mpl_in: V flag set\n");
    goto drop;
  }

  /* determine and retrieve Seed ID from header */
  seed_type = extract_seed_id(UIP_BUF, &seed_id);
  PRINTF("MPL: mpl_in: Seq 0x%x ILen 0x%02x%02x PLen 0x%x F 0x%x\n",
    UIP_EXT_MPL_HBHO->sequence, UIP_IP_BUF->len[0], UIP_IP_BUF->len[1],
    UIP_EXT_MPL_HBHO->len, UIP_EXT_MPL_HBHO->flags);
  if ((seed_set_index = lookup_seed_set(seed_type, &seed_id))
       == INDEX_NOT_FOUND) {
    /* create a new Seed Set if needed */
    if ((seed_set_index = alloc_seed_set()) == INDEX_NOT_FOUND) {
      reclaim(INDEX_NOT_FOUND);
      if ((seed_set_index = alloc_seed_set()) == INDEX_NOT_FOUND) {
        PRINTF("MPL: mpl_in: ERR alloc Seed\n");
        goto drop;
      }
    }
    PRINTF("MPL: mpl_in: new Seed %d\n", seed_set_index);
    is_new_seed = 1;
    mpl_seed_set[seed_set_index].domain_set_index = domain_set_index;
    mpl_seed_set[seed_set_index].s = seed_type;
    seed_id_copy(&mpl_seed_set[seed_set_index].seed_id, &seed_id);
    mpl_seed_set[seed_set_index].min_sequence = 0;
    timer_set(&mpl_seed_set[seed_set_index].lifetime, SEED_SET_ENTRY_LIFETIME);
  } else {
    PRINTF("MPL: mpl_in: old Seed %d\n", seed_set_index);
    if (UIP_EXT_MPL_HBHO->sequence <
          mpl_seed_set[seed_set_index].min_sequence) {
      PRINTF("MPL: mpl_in: < min_sequence\n");
      goto drop;
    }
#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
    if ((UIP_EXT_MPL_HBHO->flags & 0x20) == 0x20 /* M flag */ &&
        UIP_EXT_MPL_HBHO->sequence < max_sequence_in_window(seed_set_index)) {
      /* the largest known sequence of a peer is less than that of us */
      reset_control(domain_set_index);
    }
#endif
    message_index = is_data_in_window(UIP_EXT_MPL_HBHO->sequence, seed_set_index);
    if (message_index != INDEX_NOT_FOUND) {
      trickle_timer_consistency(&mpl_message_buffer[message_index].t);
      PRINTF("MPL: mpl_in: old Data\n");
      goto drop;
    }
  }

  if ((message_index = alloc_data_buf()) == INDEX_NOT_FOUND) {
    reclaim(seed_set_index);
    if ((message_index = alloc_data_buf()) == INDEX_NOT_FOUND) {
      PRINTF("MPL: mpl_in: ERR alloc Data\n");
      if (is_new_seed) {
        mpl_seed_set[seed_set_index].inuse = 0;
      }
      goto drop;
    }
  }
  mpl_message_buffer[message_index].sequence = UIP_EXT_MPL_HBHO->sequence;
  mpl_message_buffer[message_index].seed_index = seed_set_index;
  mpl_message_buffer[message_index].len = uip_len;
  memcpy(mpl_message_buffer[message_index].data, UIP_IP_BUF, uip_len);
  mpl_seed_set[seed_set_index].count ++;

  PRINTF("MPL: mpl_in: new Data Loc %d Len %d\n", message_index, uip_len);

  timer_reset(&mpl_seed_set[seed_set_index].lifetime);
  trickle_timer_config(&mpl_message_buffer[message_index].t,
    DATA_MESSAGE_IMIN, DATA_MESSAGE_IMAX, DATA_MESSAGE_K);
#if PROACTIVE_FORWARDING
  /* A straightforward &mpl_message_buffer[message_index] will not work
     due to bits and signedness. */
  void *p = mpl_message_buffer;
  uint32_t size = (uint32_t)message_index * sizeof(struct roll_mpl_message_buffer);
  p += size;
  trickle_timer_set(&mpl_message_buffer[message_index].t,
    trickle_cb_data_out, p);
  mpl_message_buffer[message_index].timer_expires = 0;
#else
  /* For reactive forwarding we only configure the Trikle timer but
   * leave it disabled. This is used as a hint for reset_data() */
  mpl_message_buffer[message_index].timer_expires =
    DATA_MESSAGE_TIMER_EXPIRATIONS;
#endif /* PROACTIVE_FORWARDING */

#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
  reset_control(domain_set_index);
#endif

  accept:
    PRINTF("MPL: mpl_in: ACCEPT\n");
    STATS_ADD(data_in);
    return UIP_MCAST6_ACCEPT;

  drop:
    PRINTF("MPL: mpl_in: DROP\n");
    STATS_ADD(data_drop);
    return UIP_MCAST6_DROP;
}
/*---------------------------------------------------------------------------*/
/* invoked by uip-udp-packet.c:uip_udp_packet_send()
 * when we are seeding some messages. */
void
roll_mpl_out()
{
  uint8_t domain_set_index;

  if (uip_len == 0) {
    goto drop;
  }

  domain_set_index = lookup_domain_set(&UIP_IP_BUF->destipaddr);
  if (domain_set_index == INDEX_NOT_FOUND) {
    PRINTF("MPL: mpl_out: ERR unsubscribed group ");
    PRINT6ADDR(&UIP_IP_BUF->destipaddr);
    PRINTF("\n");
    goto drop;
  }

  if (uip_len + HBHO_TOTAL_LEN + UIP_LLH_LEN > UIP_BUFSIZE) {
    PRINTF("MPL: mpl_out: ERR HBHO space\n");
    goto drop;
  }
  memmove(UIP_EXT_BUF_NEXT, UIP_EXT_BUF, uip_len - UIP_IPH_LEN);
  memset(UIP_EXT_BUF, 0, HBHO_TOTAL_LEN);
  UIP_EXT_BUF->next = UIP_IP_BUF->proto;
  UIP_IP_BUF->proto = UIP_PROTO_HBHO;
  UIP_EXT_BUF->len = (uint8_t)(HBHO_TOTAL_LEN-8)>>3;
  UIP_EXT_MPL_HBHO->sequence = last_seq;
  last_seq = SEQ_VAL_ADD(last_seq, 1);
  UIP_EXT_MPL_HBHO->type = HBHO_MPL_OPT_TYPE;
#if MPL_OUR_SEED_ID_TYPE != 0
  seed_id_to_buf(&UIP_EXT_MPL_HBHO->seed_id, MPL_OUR_SEED_ID_TYPE, &my_seed_id);
#endif
  UIP_EXT_MPL_HBHO->len = HBHO_MPL_LEN;
  /* This is a new packet seeded by us, its sequence must be the largest known. */
  UIP_EXT_MPL_HBHO->flags = (HBHO_S_FLAG << 6) | 0x20; /* M flag */
#if MPL_OUR_SEED_ID_TYPE != 1
  /* Pad N option */
  *((uint8_t *)UIP_EXT_MPL_HBHO+HBHO_MPL_LEN) = 0x01;
  *((uint8_t *)UIP_EXT_MPL_HBHO+HBHO_MPL_LEN+1) = (uint8_t)(HBHO_TOTAL_LEN-HBHO_MPL_LEN-2);
#endif
  uip_len += HBHO_TOTAL_LEN;
  uip_ext_len += HBHO_TOTAL_LEN;
  UIP_IP_BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
  UIP_IP_BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);

  PRINTF("MPL: mpl_out: D %d Sq %d Len %d\n",
    domain_set_index, last_seq, uip_len);
  roll_mpl_in(); /* register the message */
  STATS_ADD(data_sent);

  return;

  drop:
  STATS_ADD(data_drop);
  uip_len = 0;
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_icmp_input()
{
  uint8_t domain_index = INDEX_NOT_FOUND;
  uint8_t i, j;
  uint8_t *set = NULL;
  uint8_t s, bm_len, min_sequence;
  uint8_t seed_set_index = INDEX_NOT_FOUND;
  union seed_id_u seed_id;
  uint8_t buffered = 0; /* same as in trickle_cb_icmp_out () */
  uint8_t m = 0; /* mask, same as buffered */
  uint16_t ip6_payload = 0;
  uint8_t in_seed_info[MPL_MAX_SEEDS];
  uint8_t is_consistent = 1;

  domain_index = lookup_domain_set(&UIP_IP_BUF->destipaddr);
  if (domain_index == INDEX_NOT_FOUND) {
    PRINTF("MPL: icmp_in: unknown domain\n");
    STATS_ADD(icmp_drop);
    return;
  }

  STATS_ADD(icmp_in);
  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    in_seed_info[i] = 0;
  }
  ip6_payload = ((uint16_t)UIP_IP_BUF->len[0] << 8) + UIP_IP_BUF->len[1];
  set = UIP_ICMP_PAYLOAD;
  while ((int)((void *)set - (void *)UIP_ICMP_BUF) < ip6_payload) {
    min_sequence = *set;
    bm_len = *(set+1) >> 2;
    s = *(set+1) & 0x03;
    seed_id_from_buf(set+2, s, &seed_id);
    set += 2 + seed_id_size(s);
    memcpy(&buffered, set, bm_len);

    PRINTF("MPL: icmp_in: MinSeq 0x%0x BL %d S %d BM ", min_sequence, bm_len, s);
    for (i = 0;i < bm_len;i ++) {
      PRINTF(" %02x ", *(set + i));
    }
    PRINTF(" ID ");
    PRINT6ADDR(&seed_id.ip6);
    PRINTF("\n");

    set += bm_len;

    seed_set_index = lookup_seed_set(s, &seed_id);
    if (seed_set_index == INDEX_NOT_FOUND) {
      is_consistent = 0;
      PRINTF("MPL: icmp_in: Seed for us\n");
    } else {
      in_seed_info[seed_set_index] = 1;
      for (i = 0;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
        if (mpl_message_buffer[i].inuse == 1 &&
            mpl_message_buffer[i].seed_index == seed_set_index &&
            mpl_message_buffer[i].sequence >= min_sequence &&
            /* NOTE: mind the size of `buffered' */
            (buffered & ((uint8_t)0x01 <<
                (mpl_message_buffer[i].sequence - min_sequence))) == 0) 
        {
          /* we have a Data Message that they do not have */
          reset_data(i);
          PRINTF("MPL: icmp_in: Data for them Sd %d Sq %d\n",
            seed_set_index, mpl_message_buffer[i].sequence);
          is_consistent = 0;
        }
      } /* end for - check ours against theirs */

#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
      for (i = 0,m = 1;m != 0;i++, m <<= 1) {
        if ((buffered & m) != 0) {
          for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
            if (mpl_message_buffer[j].inuse == 1 &&
                mpl_message_buffer[j].seed_index == seed_set_index &&
                mpl_message_buffer[j].sequence == i + min_sequence) {
              break;
            }
          }
          if (j == MPL_MAX_MESSAGE_BUFFER &&
              (i + min_sequence)
                >= mpl_seed_set[seed_set_index].min_sequence) {
            /* they have a Data Message that we do not have */
            PRINTF("MPL: icmp_in: Data for us Sd %d Sq 0x%0x\n",
              seed_set_index, i + min_sequence);
            is_consistent = 0;
          }
        }
      } /* end for - check theirs agains ours */
#endif /* CONTROL_MESSAGE_TIMER_EXPIRATIONS */
    } /* if (seed_set_index ...) ... else */
  } /* while */

  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 1 &&
        in_seed_info[i] == 0 &&
        mpl_seed_set[i].domain_set_index == domain_index) {
      PRINTF("MPL: icmp_in: Seed for them Sd %d\n", i);
      is_consistent = 0;
      for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
        if (mpl_message_buffer[j].inuse == 1 &&
            mpl_message_buffer[j].seed_index == i) {
          reset_data(i);
        }
      }
    }
  } /* end for - we have a seed for them */

#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
  if (is_consistent == 0) {
    reset_control(domain_index);
  } else {
    trickle_timer_consistency(&mpl_domain_set[domain_index].control_timer);
  }
#endif
}
/*---------------------------------------------------------------------------*/
static uint8_t
seed_id_is_equal(uint8_t s, union seed_id_u *a, union seed_id_u *b)
{
  s &= 0x03;
  if (s == 0 || s == 3) {
    return (memcmp(a->u128, b->u128, 16) == 0);
  } else if (s == 1) {
    return (memcmp(a->u16, b->u16, 2) == 0);
  } else if (s == 2) {
    return (memcmp(a->u64, b->u64, 8) == 0);
  }
  return 0; /* actually an error */
}
/*---------------------------------------------------------------------------*/
static void
seed_id_to_buf(void *buf, uint8_t s, union seed_id_u *id)
{
  s &= 0x03;
  if (s == 0 || s == 3) {
    memcpy(buf, id->u128, 16);
  } else if (s == 1) {
    memcpy(buf, id->u16, 2);
  } else if (s == 2) {
    memcpy(buf, id->u64, 8);
  }
}
/*---------------------------------------------------------------------------*/
static void
seed_id_from_buf(void *buf, uint8_t s, union seed_id_u *id)
{
  s &= 0x03;
  if (s == 0 || s == 3) {
    memcpy(id->u128, buf, 16);
  } else if (s == 1) {
    memcpy(id->u16, buf, 2);
  } else if (s == 2) {
    memcpy(id->u64, buf, 8);
  }
}
/*---------------------------------------------------------------------------*/
static uint8_t
lookup_seed_set(uint8_t s, union seed_id_u *id)
{
  uint8_t i;

  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 1 &&
        seed_id_is_equal(s, id, &mpl_seed_set[i].seed_id)) {
      return i;
    }
  }
  return INDEX_NOT_FOUND;
}
/*---------------------------------------------------------------------------*/
static uint8_t
alloc_seed_set(void)
{
  uint8_t i;
  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 0) {
      memset(&mpl_seed_set[i], 0, sizeof(struct roll_mpl_seed_set));
      mpl_seed_set[i].inuse = 1;
      mpl_seed_set[i].count = 0;
      PRINTF("MPL: alloc_seed_set: %d\n", i);
      return i;
    }
  }
  PRINTF("MPL: alloc_seed_set: FAILED\n");
  return INDEX_NOT_FOUND;
}
/*---------------------------------------------------------------------------*/
static uint8_t
is_data_in_window(uint8_t sequence, uint8_t seed_index)
{
  uint8_t i;
  for (i = 0;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
    if (mpl_message_buffer[i].inuse == 1 &&
        mpl_message_buffer[i].seed_index == seed_index &&
        mpl_message_buffer[i].sequence == sequence) {
      return i;
    }
  }
  return INDEX_NOT_FOUND;
}
/*---------------------------------------------------------------------------*/
static uint8_t
alloc_data_buf(void)
{
  uint8_t i;
  for (i = 0;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
    if (mpl_message_buffer[i].inuse == 0) {
      mpl_message_buffer[i].inuse = 1;
      PRINTF("MPL: alloc_data_buf: %d\n", i);
      return i;
    }
  }
  PRINTF("MPL: alloc_data_buf: FAILED\n");
  return INDEX_NOT_FOUND;
}
/*---------------------------------------------------------------------------*/
static void
reclaim(uint8_t seed_set_index)
{
  uint8_t i, j;
  uint8_t largest = INDEX_NOT_FOUND;
  uint8_t lowest = INDEX_NOT_FOUND;
  uint8_t f_bypass_r2 = 0;

  /* Round 1: delete an expired Seed Set */
  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 0 ||
        i == seed_set_index) {
      continue;
    }
    if (timer_expired(&mpl_seed_set[i].lifetime)) {
      mpl_seed_set[i].inuse = 0;
      PRINTF("MPL: reclaim: R1 Seed Set: %d\n", i);
      for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
        if (mpl_message_buffer[j].inuse == 1 &&
            mpl_message_buffer[j].seed_index == i) {
          trickle_timer_stop(&mpl_message_buffer[j].t);
          mpl_message_buffer[j].inuse = 0;
          PRINTF("MPL: reclaim: R1 S %d D %d\n", i, j);
          f_bypass_r2 = 1;
        }
      }
      break; /* only one is enough */
    }
  }
  /* already release a Seed Set and at least one Data Message, enough */
  if (f_bypass_r2 == 1) {
    PRINTF("MPL: reclaim: Break\n");
    return;
  }
  /* a Seed Set is needed but none has expired
   * no point going into round 2, since alloc_seed_set() will still fail. */
  if (i == MPL_MAX_SEEDS && seed_set_index == INDEX_NOT_FOUND) {
    PRINTF("MPL: reclaim: no Seed\n");
    return;
  }
  /* Round 2: delete a message from a largest Seed Set */
  /* find the Seed Set with most Data Messages */
  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 1) {
      largest = i;
      break;
    }
  }
  for (;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 1 &&
        mpl_seed_set[i].count > mpl_seed_set[largest].count) {
      largest = i;
    }
  }
  if (largest == INDEX_NOT_FOUND) {
    PRINTF("MPL: reclaim: ERR no Seed R2\n");
    return;
  }
  /* find a Message in the largest Seed Set */
  for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
    if (mpl_message_buffer[j].inuse == 1) {
      if (mpl_message_buffer[j].seed_index == largest) {
        lowest = j;
      }
      /* this is an error */
      if (mpl_message_buffer[j].sequence <
            mpl_seed_set[mpl_message_buffer[j].seed_index].min_sequence) {
        PRINTF("MPL: reclaim: ERR under min_sequence S %d Sq 0x%0x D %d\n",
          mpl_message_buffer[j].seed_index, mpl_message_buffer[j].sequence, j);
      }
    }
  }
  /* find the Message with lowest sequence in the largest Seed Set */
  for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
    if (mpl_message_buffer[j].inuse == 1 &&
        mpl_message_buffer[j].seed_index == largest &&
        mpl_message_buffer[j].sequence
          <= mpl_message_buffer[lowest].sequence) {
      lowest = j;
    }
  }
  if (lowest == INDEX_NOT_FOUND) {
    PRINTF("MPL: reclaim: ERR no Data R2 Si %d Sq 0x%0x Lg %d i %d\n",
      seed_set_index, mpl_seed_set[largest].min_sequence, largest, i);
    PRINTF("MPL: reclaim: ERR MSG");
    for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
      if (mpl_message_buffer[j].inuse == 0) { continue; }
      PRINTF(" %d:[S %d Q 0x%0x]", j, mpl_message_buffer[j].seed_index,
        mpl_message_buffer[j].sequence);
    }
    PRINTF("\n");
    return;
  }
  mpl_message_buffer[lowest].inuse = 0;
  trickle_timer_stop(&mpl_message_buffer[lowest].t);
  PRINTF("MPL: reclaim: R2 S %d D %d SQ 0x%0x\n",
   largest, i, mpl_message_buffer[lowest].sequence);
  mpl_seed_set[largest].min_sequence = mpl_message_buffer[lowest].sequence + 1;
  /* section 11.2 : increasing min_sequence is an 'event' */
#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
  reset_control(mpl_seed_set[largest].domain_set_index);
#endif
  mpl_seed_set[largest].count --;
}
/*---------------------------------------------------------------------------*/
static void
trickle_cb_data_out(void *ptr, uint8_t suppress)
{
  struct roll_mpl_message_buffer *msg;
  struct roll_mpl_hbho_hdr *hdr;
  uint8_t seed_type;
  uint8_t i;

  msg = (struct roll_mpl_message_buffer *)ptr;
  hdr = (struct roll_mpl_hbho_hdr *)&msg->data[UIP_IPH_LEN + 2];
  seed_type = is_valid_mpl_hdr(msg->data);
  msg->timer_expires ++;
  if (msg->timer_expires >= DATA_MESSAGE_TIMER_EXPIRATIONS) {
    ANNOTATE("MPL: cb_data(%p): going silent D #%d Sd #%d Sq 0x%0x\n", ptr,
      mpl_seed_set[msg->seed_index].domain_set_index,
      msg->seed_index, hdr->sequence);
    trickle_timer_stop(&msg->t);
  }
  if (suppress == 0) {
    ANNOTATE("MPL: cb_data(%p): suppressed D #%d Sd #%d Sq 0x%0x\n", msg,
      mpl_seed_set[msg->seed_index].domain_set_index,
      msg->seed_index, hdr->sequence);
    return;
  }
  PRINTF("MPL: cb_data(%p): Sup %x D #%d Sd #%d Sq 0x%0x Len 0x%x\n",
    ptr, suppress,
    mpl_seed_set[msg->seed_index].domain_set_index,
    msg->seed_index, hdr->sequence, msg->len);

  /* check and set M flag */
  for (i = 0;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
    if (mpl_message_buffer[i].inuse == 1 &&
        mpl_message_buffer[i].seed_index == msg->seed_index &&
        mpl_message_buffer[i].sequence > msg->sequence) {
      break; /* found one with a higher sequence */
    }
  }
  if (i == MPL_MAX_MESSAGE_BUFFER) {
    hdr->flags |= 0x20; /* set M flag */
  } else {
    hdr->flags &= 0xc0; /* keep only S */
  }

  hdr->flags &= 0xf0; /* clear lower 4 bits */

  memcpy(UIP_IP_BUF, msg->data, msg->len);
  uip_len = msg->len;
#ifdef MPL_PAYLOAD_TRACING
  roll_mpl_add_trace();
  memcpy(msg->data, UIP_IP_BUF, msg->len);
#endif
  ANNOTATE("TRAN cb_data %d\n", hdr->sequence);
  STATS_ADD(data_sent);
  tcpip_ipv6_output();
}
/*---------------------------------------------------------------------------*/
static void
trickle_cb_icmp_out(void *ptr, uint8_t suppress)
{
  struct roll_mpl_domain_set *dom;
  uint16_t payload_len = 0;
  uint8_t i, j;
  uint8_t buffered; /* same as in roll_mpl_icmp_input() */
  uint8_t *p;

  dom = (struct roll_mpl_domain_set *)ptr;
  dom->timer_expires ++;
  if (dom->timer_expires >= CONTROL_MESSAGE_TIMER_EXPIRATIONS) {
    ANNOTATE("MPL: cb_icmp(%p): going silent D %ld\n", ptr,
      dom - mpl_domain_set);
    trickle_timer_stop(&dom->control_timer);
  }
  if (suppress == 0) {
    ANNOTATE("MPL: cb_icmp(%p): suppressed D %ld\n", ptr,
       dom - mpl_domain_set);
    return;
  }
  memset(UIP_IP_BUF, 0, UIP_BUFSIZE - UIP_LLH_LEN);

  UIP_IP_BUF->vtc = 0x60;
  UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
  UIP_IP_BUF->ttl = 255;
  UIP_ICMP_BUF->type = ICMP6_MPL;

  for (i = 0;i < MPL_MAX_SEEDS;i ++) {
    if (mpl_seed_set[i].inuse == 0 ||
        mpl_seed_set[i].domain_set_index != (dom - mpl_domain_set)) {
      continue;
    }
    p = (uint8_t *)(UIP_ICMP_PAYLOAD + payload_len);
    /* min-seqno */
    *p = mpl_seed_set[i].min_sequence;
    /* bm-len */
    *(p+1) = sizeof(buffered) << 2;
    /* S */
    *(p+1) |= (mpl_seed_set[i].s & 0x03);
    /* seed_id */
    seed_id_to_buf(p+2, mpl_seed_set[i].s, &mpl_seed_set[i].seed_id);
    p += (2 + seed_id_size(mpl_seed_set[i].s));
    /* buffered MPL messages */
    buffered = 0;
    for (j = 0;j < MPL_MAX_MESSAGE_BUFFER;j ++) {
      if (mpl_message_buffer[j].seed_index == i) {
        buffered |=
          1 << (mpl_message_buffer[j].sequence - mpl_seed_set[i].min_sequence);
      }
    }
    memcpy(p, &buffered, sizeof(buffered));
    payload_len += (2 + seed_id_size(mpl_seed_set[i].s) + sizeof(buffered));
  }

  uip_ip6addr_copy(&UIP_IP_BUF->destipaddr, &dom->domain_address);
  /* Control Messages are Link-Scoped */
  UIP_IP_BUF->destipaddr.u8[1] &= 0xf0;
  UIP_IP_BUF->destipaddr.u8[1] |= 0x02;
  uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
  UIP_IP_BUF->len[0] = (UIP_ICMPH_LEN + payload_len) >> 8;
  UIP_IP_BUF->len[1] = (UIP_ICMPH_LEN + payload_len) & 0xff;
  UIP_ICMP_BUF->icmpchksum = ~ uip_icmp6chksum();
  uip_len = UIP_IPH_LEN + UIP_ICMPH_LEN + payload_len;

  PRINTF("MPL: cb_icmp(%p): Dom ", ptr);
  PRINT6ADDR(&dom->domain_address);
  PRINTF(" PLen %d\n", payload_len);
  ANNOTATE("TRAN cb_icmp D %ld\n", dom - mpl_domain_set);
  STATS_ADD(icmp_sent);
  tcpip_ipv6_output();
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_set_last_seq(uint8_t seq)
{
  last_seq = seq;
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_set_seed_id(const union seed_id_u const *id)
{
  seed_id_copy(&my_seed_id, id);
}
/*---------------------------------------------------------------------------*/
/* pkt : Packet WITHOUT Link-Layer Header */
static uint8_t
is_valid_mpl_hdr(const void const *pkt)
{
  struct uip_ip_hdr *iph;
  struct roll_mpl_hbho_hdr *hdr;

  iph = (struct uip_ip_hdr *)pkt;
  hdr = (struct roll_mpl_hbho_hdr *)&((uint8_t *)pkt)[UIP_IPH_LEN + 2];
  if (iph->proto != UIP_PROTO_HBHO ||
      hdr->type != HBHO_MPL_OPT_TYPE ||
      (hdr->flags & 0x10) != 0) {
    PRINTF("MPL: is_valid_mpl_hdr: invalid\n");
    return 0; /* FALSE */
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static uint8_t
extract_seed_id(const void const *pkt, union seed_id_u *id)
{
  struct roll_mpl_hbho_hdr *hdr;
  struct uip_ip_hdr *iph;
  uint8_t s = 0xff;

  memset(id, 0, sizeof(union seed_id_u));
  if (is_valid_mpl_hdr(pkt)) {
    hdr = (struct roll_mpl_hbho_hdr *)
           &((uint8_t *)pkt)[UIP_LLH_LEN + UIP_IPH_LEN + 2];
    s = (hdr->flags & 0xc0) >> 6;
    if (s == 0) {
      iph = (struct uip_ip_hdr *)&((uint8_t *)pkt)[UIP_LLH_LEN];
      memcpy(&id->ip6, &iph->srcipaddr, sizeof(uip_ip6addr_t));
    } else if (s == 1) {
      memcpy(&id->u16, &hdr->seed_id, 2);
    } else if (s == 2) {
      memcpy(&id->u64, &hdr->seed_id, 8);
    } else if (s == 3) {
      memcpy(&id->u128, &hdr->seed_id, 16);
    }
  }
  PRINTF("MPL: extract_seed_id: s = %x\n", s);
  return s;
}
/*---------------------------------------------------------------------------*/
static uint8_t
max_sequence_in_window(uint8_t seed_index)
{
  uint8_t i;
  uint8_t largest = INDEX_NOT_FOUND;

  for (i = 0;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
    if (mpl_message_buffer[i].seed_index == seed_index) {
      largest = i;
      break;
    }
  }

  for ( ;i < MPL_MAX_MESSAGE_BUFFER;i ++) {
    if (mpl_message_buffer[i].seed_index == seed_index &&
        mpl_message_buffer[i].sequence > mpl_message_buffer[largest].sequence) {
      largest = i;
    }
  }

  if (largest == INDEX_NOT_FOUND) {
    return 0;
  }
  return mpl_message_buffer[largest].sequence;
}
/*---------------------------------------------------------------------------*/
static void
reset_control(uint8_t domain)
{
#if CONTROL_MESSAGE_TIMER_EXPIRATIONS > 0
  void *p;
  uint32_t size;

  if (mpl_domain_set[domain].inuse == 0) {
    return;
  }
  if (mpl_domain_set[domain].timer_expires >=
        CONTROL_MESSAGE_TIMER_EXPIRATIONS) {
    /* the timer has expired, restart it */
    p = mpl_domain_set;
    size = (uint32_t)domain * sizeof(struct roll_mpl_domain_set);
    p += size;
    trickle_timer_set(&mpl_domain_set[domain].control_timer,
      trickle_cb_icmp_out, p);
    PRINTF("MPL: reset_control: restarted %d\n", domain);
  } else {
    trickle_timer_inconsistency(&mpl_domain_set[domain].control_timer);
    PRINTF("MPL: reset_control: inconsis %d\n", domain);
  }
  mpl_domain_set[domain].timer_expires = 0;
#endif /* CONTROL_MESSAGE_TIMER_EXPIRATIONS */
}
/*---------------------------------------------------------------------------*/
static void
reset_data(uint8_t data)
{
  void *p;
  uint32_t size;

  if (mpl_message_buffer[data].inuse == 0) {
    return;
  }
  if (mpl_message_buffer[data].timer_expires >=
        DATA_MESSAGE_TIMER_EXPIRATIONS) {
    /* the timer has expired (PROACTIVE_FORWARDING)
     * or is not running (reactive forwarding), restart it */
    p = mpl_message_buffer;
    size = (uint32_t)data*sizeof(struct roll_mpl_message_buffer);
    p += size;
    trickle_timer_set(&mpl_message_buffer[data].t,
                      trickle_cb_data_out, p);
    PRINTF("MPL: reset_data: restarted %d\n", data);
  } else {
    trickle_timer_inconsistency(&mpl_message_buffer[data].t);
    PRINTF("MPL: reset_data: inconsis %d\n", data);
  }
  mpl_message_buffer[data].timer_expires = 0;
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_set_payload(void *p, uint8_t sequence)
{
  struct roll_mpl_payload *payload = (struct roll_mpl_payload *)p;
  payload->sequence = sequence;
#ifdef MPL_PAYLOAD_TRACING
  payload->hops = 0;
  payload->path[0] = node_id & 0xff;
#endif
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_add_trace(void)
{
#ifdef MPL_PAYLOAD_TRACING
  struct roll_mpl_payload *payload =
    (struct roll_mpl_payload *)
     &(uip_buf[UIP_LLH_LEN + UIP_IPUDPH_LEN + HBHO_TOTAL_LEN]);

  if (payload->hops > MPL_MAX_TRACE) {
    PRINTF("MPL: ERR: S %d trace diameter\n", payload->sequence);
    return;
  }
  payload->hops ++;
  payload->path[payload->hops] = node_id;
#endif
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_show_payload(void *p)
{
  struct roll_mpl_payload *payload = (struct roll_mpl_payload *)p;
#ifdef MPL_PAYLOAD_TRACING
  uint8_t i;
#endif

  printf("RECV %d", payload->sequence);
#ifdef MPL_PAYLOAD_TRACING
  printf(" Tra %d :", payload->hops);
  for (i = 0;i <= payload->hops;i ++) {
    printf(" %d", payload->path[i]);
  }
#endif
  printf("\n");
}
/*---------------------------------------------------------------------------*/
void
roll_mpl_show_stat(void)
{
#if UIP_MCAST6_STATS
  printf("STAT Ci %d Cs %d Cd %d Di %d Ds %d Dd %d\n",
    roll_mpl_stat.icmp_in, roll_mpl_stat.icmp_sent, roll_mpl_stat.icmp_drop,
    roll_mpl_stat.data_in, roll_mpl_stat.data_sent, roll_mpl_stat.data_drop);
#endif
}
/*---------------------------------------------------------------------------*/
#endif /* __ROLL_MPL_H__ */
