/*
 * Path Maximum Payload Discovery Implementation
 *
 * written by Yang Deng, DCS-CSE, Aalto University
 */
#ifndef __PMPD_H__
#define __PMPD_H__

#include "net/uip.h"

extern process_event_t pmpd_event;

#ifdef NUM_CACHE_PMPD_CONF
#define NUM_CACHE_PMPD NUM_CACHE_PMPD_CONF
#else
#define NUM_CACHE_PMPD 4
#endif


#ifdef NUM_PROC_PMPD_CONF
#define NUM_PROC_PMPD NUM_PROC_PMPD_CONF
#else
#define NUM_PROC_PMPD 4
#endif


typedef struct pmpd_cache {
	uip_ip6addr_t dst_node_addr;
	uint8_t max_payload;
} pmpd_cache_t;

extern pmpd_cache_t net_pmpd_cache[];

uint8_t pmpd_set_max_payload(const uip_ip6addr_t* dst_addr, const uint8_t max_payload);
uint8_t pmpd_get_max_payload(const uip_ip6addr_t* dst_addr);

uint8_t pmpd_attach_process(struct process *p);
uint8_t pmpd_detach_process(struct process *p);

void pmpd_poll_proc();

#endif /*__PMPD_H__*/
