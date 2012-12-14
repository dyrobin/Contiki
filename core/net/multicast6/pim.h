#ifndef __PIM_H__
#define __PIM_H__

#include "pim-control.h"
#include "net/uip.h"

#ifndef MAX_NUM_OF_MCAST6_GROUPS
#define MAX_NUM_OF_MCAST6_GROUPS 8
#endif

#ifdef MCAST_INTERVAL_CONF
#define MCAST_INTERVAL MCAST_INTERVAL_CONF
#else
#define MCAST_INTERVAL 20
#endif

#ifdef MCAST_THRESHOLD_CONF
#define MCAST_THRESHOLD MCAST_THRESHOLD_CONF
#else
#define MCAST_THRESHOLD 10000
#endif

uint8_t pim_data_in(void);
void pim_data_out(void);

/* 
    Multicast routing table entry defined here. 
    The multicast routing table is an array defined in pim.c
    if (S,G,P,N) is the state in a multicast routing table, then
    used is just used to check if the state is valid or not
    @sender_addr is the S
    @mcast_grp is G
    @pref_parent is P
    @num_of_joins is N
    @update_time is the time the latest update packet was received
*/
struct uip_mcast6_route {
    uint8_t mld_flag; //just for now since there is no mld
    uint8_t used;
    uip_ipaddr_t sender_addr;
    uip_ipaddr_t mcast_grp;
    uip_ipaddr_t pref_parent;
    uint8_t num_of_joins;
    clock_time_t update_time;
};

typedef struct uip_mcast6_route uip_mcast6_route_t;

/*
    Routines for multicast table
*/
uip_mcast6_route_t *uip_mcast6_route_alloc();
uip_mcast6_route_t *uip_mcast6_route_find(uip_ipaddr_t *sender_addr, 
                                          uip_ipaddr_t *grp_addr);
void print_mcast_table(void);
void handle_mcast_timer();
void pim_init(void);

#endif /* __PIM_H__ */
