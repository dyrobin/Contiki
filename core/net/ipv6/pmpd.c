/*
 * Path Maximum Payload Discovery Implementation
 *
 * Written by Yang Deng <yang.deng@aalto.fi>
 */


#include <stdio.h>
#include <string.h>
#include "pmpd.h"
#include "net/ip/uip-debug.h"

pmpd_cache_t net_pmpd_cache[NUM_CACHE_PMPD];

static struct process* net_pmpd_list[NUM_PROC_PMPD];

static uint8_t tail_pmpd_cache = 0;

#define DEFAULT_MAX_PAYLOAD_PMPD 97

static
uint8_t pmpd_find_pos(const uip_ip6addr_t* dst_addr)
{
	int i;
	for (i = 0; i < NUM_CACHE_PMPD; i++) {
		if (uip_ip6addr_cmp(&net_pmpd_cache[i].dst_node_addr, dst_addr)) {
			return i;
		}
	}
	return NUM_CACHE_PMPD;
}

//static
//void pmpd_debug_cache()
//{
//	int i;
//	for (i = 0; i < NUM_CACHE_PMPD; i++) {
//		printf("pmpd_cache[%d]: ", i);
//		uip_debug_ipaddr_print(&net_pmpd_cache[i].dst_node_addr);
//		printf(" with max_payload(%u)\n", net_pmpd_cache[i].max_payload);
//	}
//}

void pmpd_poll_proc()
{
	int i;
	for (i = 0; i < NUM_PROC_PMPD; i++) {
		if (net_pmpd_list[i] != NULL) {
//			printf ("pmpd: deliver pmpd event to process '%s'\n", PROCESS_NAME_STRING(net_pmpd_list[i]));
			process_post_synch(net_pmpd_list[i], pmpd_event, NULL);
		}
	}
}

uint8_t pmpd_set_max_payload(const uip_ip6addr_t* dst_addr, const uint8_t max_payload)
{
	printf ("pmpd: update ");
	uip_debug_ipaddr_print(dst_addr);
	printf("with max_payload(%d)\n", max_payload);

	uint8_t pos = pmpd_find_pos(dst_addr);
	if (pos < NUM_CACHE_PMPD) {
		if (max_payload < net_pmpd_cache[pos].max_payload ) {
			net_pmpd_cache[pos].max_payload = max_payload;
//			printf(" found in cache and update max_payload(%u)\n", max_payload);
			return 1;
		} else {
			printf("pmpd: update error for ");
			uip_debug_ipaddr_print(dst_addr);
			printf("\n");
			return 0;
		}
	}
	// add it to cache
	uip_ip6addr_copy(&net_pmpd_cache[tail_pmpd_cache].dst_node_addr, dst_addr);
	net_pmpd_cache[tail_pmpd_cache].max_payload = max_payload;
	tail_pmpd_cache = ( tail_pmpd_cache + 1 ) % NUM_CACHE_PMPD;
//	printf(" not found in cache then add max_payload(%u)\n", max_payload);
//	pmpd_debug_cache();
	return 1;
}

uint8_t pmpd_get_max_payload(const uip_ip6addr_t* dst_addr)
{
	printf ("pmpd: get ");
	uip_debug_ipaddr_print(dst_addr);

	uint8_t pos = pmpd_find_pos(dst_addr);
	if (pos < NUM_CACHE_PMPD) {
		printf(" max_payload(%u)\n", net_pmpd_cache[pos].max_payload);
		return net_pmpd_cache[pos].max_payload;
	}

	printf(" max_payload(%u) default\n", DEFAULT_MAX_PAYLOAD_PMPD);
	return DEFAULT_MAX_PAYLOAD_PMPD;
}

uint8_t pmpd_attach_process(struct process *p)
{
	int i;
	for (i = 0; i < NUM_PROC_PMPD; i++) {
		if (net_pmpd_list[i] == 0) {
			net_pmpd_list[i] = p;
			return 1;
		}
	}
	// Full
	printf("pmpd: proc_list is full!\n");
	return 0;
}


uint8_t pmpd_detach_process(struct process *p)
{
	int i;
	for (i = 0; i < NUM_PROC_PMPD; i++) {
		if (net_pmpd_list[i] == p) {
			net_pmpd_list[i] = NULL;
			return 1;
		}
	}
	// FETAL ERROR!
	printf("pmpd: fetal error!\n");
	return 0;
}
