//#include "net/tcpip.h"
//#include "net/uip.h"
#include "contiki-net.h"
#include "net/uip-ds6.h"
//#include "net/uip-nd6.h"
#include "net/uip-icmp6.h"
//#include "net/packetbuf.h"
//#include "lib/memb.h"
#include "pim-control.h"
#include "pim.h"
#include "sys/clock.h"

//#include <limits.h>
#include <string.h>

#define DEBUG DEBUG_NONE

#include "net/uip-debug.h"

#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

/*
    Allocating array for multicast table
    ALERT! might need some other memory allocation routines such as MEMB
*/
static uip_mcast6_route_t uip_mcast6_table[MAX_NUM_OF_MCAST6_GROUPS];

/*
    Timer to implement the heartbeadt mechanism
 */
static struct ctimer mcast_timer;

/**
 *      Function to print multicast table
 *      ONLY FOR TESTING
 */ 
void
print_mcast_table() 
{
    uip_mcast6_route_t *entry;
    uip_mcast6_route_t *end;

    printf("Printing multicast table\n"); 
    for(entry = &uip_mcast6_table[0], end = entry + MAX_NUM_OF_MCAST6_GROUPS;
        entry < end; ++entry) {
            if(entry->used) {
                printf("S = ");
                uip_debug_ipaddr_print(&entry->sender_addr);
                printf(", G = ");
                uip_debug_ipaddr_print(&entry->mcast_grp);
                printf(", P = ");
                uip_debug_ipaddr_print(&entry->pref_parent);
                printf(", N = %d\n", entry->num_of_joins);
            }
    }
}

/**
 *      Function to calculate time difference with the current time
 *      used in the handle_mcast_timer
 */
static clock_time_t
time_diff(clock_time_t tm)
{
    clock_time_t max_time, diff, cur;
    
    max_time = ~0;
    cur = clock_time();

    if(cur < tm) {
        diff = max_time - tm + cur;
    } else {
        diff = cur - tm;
    }
    return diff;
}

/**
 *      Function to check if route to S has changed
 *      @entry is the route table entry
 *
 */
static int
check_route_changed(uip_mcast6_route_t *entry)
{
    uip_ipaddr_t *nexthop;
    uip_ds6_nbr_t *new;
    uip_ds6_nbr_t *old;

    if(uip_ds6_is_my_addr(&entry->sender_addr)) {
        return 0;
    }
    nexthop = find_next_hop(&entry->sender_addr);
    new = uip_ds6_nbr_lookup(nexthop);
    old = uip_ds6_nbr_lookup(&entry->pref_parent);
    if(memcmp(new->lladdr.addr, old->lladdr.addr, UIP_LLADDR_LEN) != 0) {
        uip_debug_ipaddr_print(nexthop);
        printf(" new parent this is \n");
        return 1;
    } else {
        return 0;
    }
}
     
    
/**
 *      Callback function to handle timer
 *      send updates, maintain states etc...
 */
void
handle_mcast_timer()
{
    uip_mcast6_route_t *entry;
    uip_mcast6_route_t *end;
    uip_ipaddr_t sender_addr;
    uip_ipaddr_t group_addr;
    clock_time_t diff;
    
    PRINTF("handle_mcast_timer is being called on time = %u\n", clock_time());
    for(entry = &uip_mcast6_table[0], end = entry + MAX_NUM_OF_MCAST6_GROUPS;
        entry < end; ++entry) {
            if(entry->used) {
                PRINT6ADDR(&entry->pref_parent);
                PRINTF(" this is P for now\n", entry->used);
                if(check_route_changed(entry)) {
                    PRINTF("route changed, has to send join\n");
                    uip_ipaddr_copy(&sender_addr, &entry->sender_addr);
                    uip_ipaddr_copy(&group_addr, &entry->mcast_grp);
                    if(entry->mld_flag) {//since MLD is not implemented
                        entry->used = 0;
                        mcast6_join_send(&sender_addr, &group_addr, 1);
                    } else {
                        entry->used = 0;
                        mcast6_join_send(&sender_addr, &group_addr, 0);
                    } 
                }
                diff =  time_diff(entry->update_time);
                if(diff > MCAST_THRESHOLD) {
                    //this if is a hack since there is no MLD for now
                    if(entry->mld_flag == 0) {
                    //delete state ALERT: delete state is must
                    PRINTF("no updates, sending prune\n");
                    entry->used = 0;
                    } //hack ends
                } else {
					if(uip_ds6_is_my_addr(&entry->sender_addr) == 0) {
						mcast6_update_send(&entry->sender_addr, &entry->mcast_grp, 
								&entry->pref_parent);
                    }
                }
                //again send update if used=2
                if(entry->mld_flag) {
                    mcast6_update_send(&entry->sender_addr, &entry->mcast_grp, 
                                   &entry->pref_parent);
                }//hack ends.. since MLD is not implemented
            }
    }
    ctimer_reset(&mcast_timer);
}

/**
 *      Function to allocate an empty route entry in the multicast table
 *      If there are no available entries return NULL
 */
uip_mcast6_route_t *
uip_mcast6_route_alloc()
{
    uip_mcast6_route_t *entry;
    uip_mcast6_route_t *end;

    for(entry = &uip_mcast6_table[0], end = entry + MAX_NUM_OF_MCAST6_GROUPS;
        entry < end; ++entry) {
            if(entry->used == 0) {
                memset(entry, 0, sizeof(uip_mcast6_route_t));
                entry->used = 1;
                return entry;
            }
    }
    return NULL;
}

/**
 *      Function to find a route entry in the multicast table
 *      @sender_addr is the S variable in routing entry state
 *      @grp_addr is the G variable in routing entry state
 */
uip_mcast6_route_t *
uip_mcast6_route_find(uip_ipaddr_t *sender_addr, uip_ipaddr_t *grp_addr)
{
    uip_mcast6_route_t *entry;
    uip_mcast6_route_t *end;

    for(entry = &uip_mcast6_table[0], end = entry + MAX_NUM_OF_MCAST6_GROUPS;
        entry < end; ++entry) {
            if(entry->used) {
                if(uip_ipaddr_cmp(&entry->sender_addr, sender_addr) && 
                  uip_ipaddr_cmp(&entry->mcast_grp, grp_addr)) {
                    return entry;
                }                    
            }
    }
    return NULL;
}

/**
 *      Function called when a multicast data packet enters uip_process()
 */
uint8_t
pim_data_in(void)
{
    uip_mcast6_route_t *entry;
    uip_ipaddr_t src_addr;
    uip_ipaddr_t dest_addr;
    uip_ds6_nbr_t *p;
    char *appdata;

/* The following 2 lines of code are for debugging purpose
   and can be compiled only if DEBUG=1 is defined
 */
    appdata = (char *)&uip_buf[UIP_IPUDPH_LEN + UIP_LLH_LEN];
    appdata[uip_len - 48] = 0;
    uip_ipaddr_copy(&src_addr, &UIP_IP_BUF->srcipaddr);
    uip_ipaddr_copy(&dest_addr, &UIP_IP_BUF->destipaddr);

    entry = uip_mcast6_route_find(&src_addr, &dest_addr);

    if(entry == NULL) {
        PRINTF("MCAST:No state found: Packet rejected\n");
    } else {
        if((p = uip_ds6_nbr_lookup(&entry->pref_parent)) != NULL) {
            if(memcmp(p->lladdr.addr, packetbuf_addr(PACKETBUF_ADDR_SENDER), 
                UIP_LLADDR_LEN)) {
                PRINTF("MCAST: Not from preferred sender, reject\n");
            } else {
                printf("Data received: %s \n ", appdata);
                printf("MCAST: from preferred sender, broadcasting again\n");    
                return 1;
            }
        } else {
            PRINTF("MCAST: LL addr of P not found\n");
        }
    }
    
    return 0; 
}

void 
pim_data_out(void)
{
    /* TODO
        process the uip_buf before sending the packet
        Probably not necessary
    */
}

void
pim_init(void)
{
    ctimer_set(&mcast_timer, MCAST_INTERVAL*CLOCK_SECOND, 
               handle_mcast_timer, NULL);
}

