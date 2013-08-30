#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/multicast6/uip-mcast6.h"

#ifdef __PIM_H__

#include <string.h>

#include "net/uip-debug.h"
#define DEBUG DEBUG_NONE

#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF     ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ICMP_PAYLOAD ((unsigned char *)&uip_buf[uip_l2_l3_icmp_hdr_len])

/**
 *      Function to find next hop
 *      @addr is the IP address that you want to calculate the next hop to
 */
uip_ipaddr_t *
find_next_hop(uip_ipaddr_t *addr) 
{
    uip_ds6_route_t *locrt; 
    uip_ipaddr_t *nexthop;

    locrt = uip_ds6_route_lookup(addr);
    if (locrt == NULL) {
        nexthop = uip_ds6_defrt_choose();
    } else {
        nexthop = uip_ds6_route_nexthop(locrt);
    }

    return nexthop;
}

/**
 *      Function to handle PIM JOIN control packets
 */    
void 
pim_join_in(void)
{
    uip_ipaddr_t src_addr;
    uip_ipaddr_t sender_addr;
    uip_ipaddr_t group_addr;
    uip_ipaddr_t *nexthop;
    uip_mcast6_route_t *entry;
    uint8_t buffer_length;
    unsigned char *buffer;
    int pos;

    uip_ipaddr_copy(&src_addr, &UIP_IP_BUF->srcipaddr);
   
    buffer = UIP_ICMP_PAYLOAD;
    buffer_length = uip_len - uip_l3_icmp_hdr_len;

    pos = 0;
    memcpy(&sender_addr, buffer, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);
    memcpy(&group_addr, buffer + pos, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);
    
    PRINTF("MCAST: Received PIM Join from ");
    PRINT6ADDR(&src_addr);
    PRINTF(" to ");
    PRINT6ADDR(&sender_addr);
    PRINTF("\n");

    PRINTF("Data is S = ");
    PRINT6ADDR(&sender_addr);
    PRINTF(" G = ");
    PRINT6ADDR(&group_addr);
    PRINTF("\n");

    entry = uip_mcast6_route_find(&sender_addr, &group_addr);

    if(entry != NULL) {
        PRINTF("MCAST: State already exists, Updating N\n");
        entry->num_of_joins++;
    } else {
        //With MLD you might need a different logic to include devices that are not 
        //routers but wherein this router is designated router to those devices
        if(uip_ds6_is_my_addr(&sender_addr)) {
            entry = uip_mcast6_route_alloc();
            if (entry == NULL) {
                PRINTF("MCAST: No space, cannot add new state");
            } else {
                PRINTF("MCAST: Adding new state\n");
                entry->num_of_joins = 1;
                entry->update_time = clock_time();
                uip_ipaddr_copy(&entry->sender_addr, &sender_addr);
                uip_ipaddr_copy(&entry->mcast_grp, &group_addr);
                uip_ip6addr(&entry->pref_parent, 0, 0, 0, 0, 0, 0, 0, 0x01);
            }
        } else {
            entry = (nexthop = find_next_hop(&sender_addr)) ? uip_mcast6_route_alloc() : NULL;
            if((entry == NULL) || (nexthop == NULL)) {
                PRINTF("MCAST: No space or route not found, cannot add new state\n");
            } else {
                PRINTF("MCAST: Adding new state\n");
                entry->num_of_joins = 1;
                entry->update_time = clock_time();
                uip_ipaddr_copy(&entry->sender_addr, &sender_addr);
                uip_ipaddr_copy(&entry->mcast_grp, &group_addr);
                uip_ipaddr_copy(&entry->pref_parent, nexthop);
                PRINTF("MCAST: Forwarding the JOIN towards ");
                PRINT6ADDR(&sender_addr);
                PRINTF(" and next hop is ");
                PRINT6ADDR(nexthop);
                PRINTF("\n");
                uip_icmp6_send(nexthop, ICMP6_MULTICAST, PIM_JOIN, pos);
            }
        }
    }
}

/**
 *      Function to handle PIM PRUNE control packets
 */
void
pim_prune_in(void)
{
    uip_ipaddr_t src_addr;
    uip_ipaddr_t sender_addr;
    uip_ipaddr_t group_addr;
    uip_ipaddr_t *nexthop;
    uip_mcast6_route_t *entry;
    uint8_t buffer_length;
    unsigned char *buffer;
    int pos;
    
    uip_ipaddr_copy(&src_addr, &UIP_IP_BUF->srcipaddr);
   
    buffer = UIP_ICMP_PAYLOAD;
    buffer_length = uip_len - uip_l3_icmp_hdr_len;
    
    pos = 0;
    memcpy(&sender_addr, buffer, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);
    memcpy(&group_addr, buffer + pos, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);
    
    PRINTF("MCAST: Received PIM Prune from ");
    PRINT6ADDR(&src_addr);
    PRINTF(" to ");
    PRINT6ADDR(&sender_addr);
    PRINTF("\n");

    PRINTF("Data is S = ");
    PRINT6ADDR(&sender_addr);
    PRINTF(" G = ");
    PRINT6ADDR(&group_addr);
    PRINTF("\n");

    entry = uip_mcast6_route_find(&sender_addr, &group_addr);
    
    if(entry != NULL) {
        if(entry->num_of_joins > 1) {
            PRINTF("MCAST: Tree still in use, just decreasing the number of joins\n");
            entry->num_of_joins--;
        } else {
            PRINTF("MCAST: Tree no longer needed, deleting state \n");
            entry->used = 0;
            nexthop = find_next_hop(&sender_addr);
            if (nexthop != NULL) {
                PRINTF("MCAST: Forwarding the PRUNE towards ");
                PRINT6ADDR(&sender_addr);
                PRINTF(" and next hop is ");
                PRINT6ADDR(nexthop);
                PRINTF("\n");
                uip_icmp6_send(nexthop, ICMP6_MULTICAST, PIM_PRUNE, pos);
            } else {
                PRINTF("MCAST: Forwarding failed, route not found\n");
            }
        }
    } else {
        PRINTF("MCAST: State not found, wrong PRUNE?");
    }
}

/**
 *      Function to handle PIM UPDATE control packets
 */
void
pim_update_in()
{
    uip_ipaddr_t sender_addr;
    uip_ipaddr_t group_addr;
    uip_ipaddr_t *nexthop;
    uip_mcast6_route_t *entry;
    unsigned char *buffer;
    uint8_t buffer_length;
    int pos;
    
    buffer = UIP_ICMP_PAYLOAD;
    buffer_length = uip_len - uip_l3_icmp_hdr_len;
    
    pos = 0;
    memcpy(&sender_addr, buffer, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);
    memcpy(&group_addr, buffer + pos, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);

    PRINTF("Received PIM update from \n");
    PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
    PRINTF(" S = ");
    PRINT6ADDR(&sender_addr);
    PRINTF(" G = ");
    PRINT6ADDR(&group_addr);
    PRINTF("\n");
    
    entry = uip_mcast6_route_find(&sender_addr, &group_addr);
    if(entry != NULL) {
        entry->update_time = clock_time();
    } else {
        PRINTF("No state found, but update received, so sending a join\n");
        if(uip_ds6_is_my_addr(&sender_addr)) {
            entry = uip_mcast6_route_alloc();
            if(entry != NULL) {
                entry->update_time = clock_time();
                entry->num_of_joins = 1;
                uip_ipaddr_copy(&entry->sender_addr, &sender_addr);
                uip_ipaddr_copy(&entry->mcast_grp, &group_addr);
                uip_ip6addr(&entry->pref_parent, 0, 0, 0, 0, 0, 0, 0, 0x01);
            } else {    
                PRINTF("No space to add new state\n");
            }
        } else {
            entry = (nexthop = find_next_hop(&sender_addr)) ? uip_mcast6_route_alloc() : NULL;
            if((entry == NULL) || (nexthop == NULL)) {
                PRINTF("No space or route not found \n");
            } else {
                entry->update_time = clock_time();
                entry->num_of_joins = 1;
                uip_ipaddr_copy(&entry->sender_addr, &sender_addr);
                uip_ipaddr_copy(&entry->mcast_grp, &group_addr);
                uip_ipaddr_copy(&entry->pref_parent, nexthop);
                uip_icmp6_send(nexthop, ICMP6_MULTICAST, PIM_JOIN, pos);
            }
        }
    }
}

/**
 *      API to send join message to the sender
 *      @sender_addr is the sender to whom the join request is sent
 *      @group_addr is the multicast address that the sender will be using
 */
void 
mcast6_join_send(uip_ipaddr_t *sender_addr, uip_ipaddr_t *group_addr, uint8_t mld_flag)
{
    uip_ipaddr_t *nexthop;
    unsigned char *buffer;
    uip_mcast6_route_t *entry;
    int pos;
    
    nexthop = find_next_hop(sender_addr);

    if (nexthop != NULL) {
        entry = uip_mcast6_route_find(sender_addr, group_addr);
        if(entry != NULL) {
            PRINTF("MCAST: State already exists, Updating N\n");
            entry->num_of_joins++;
        } else {
            entry = uip_mcast6_route_alloc();
            if(entry == NULL) {
                PRINTF("MCAST: No space, cannot add new state\n");
            } else {
                PRINTF("MCAST: Adding new state\n");
                entry->num_of_joins = 1;
                entry->update_time = clock_time();
                uip_ipaddr_copy(&entry->sender_addr, sender_addr);
                uip_ipaddr_copy(&entry->mcast_grp, group_addr);
                uip_ipaddr_copy(&entry->pref_parent, nexthop);
                //small hack since MLD is not implemented
                //or else the node sends a prune
                entry->mld_flag = mld_flag;
                //hack ends here
                PRINTF("MCAST: Sending JOIN to ");
                PRINT6ADDR(sender_addr);
                PRINTF(" and next hop is ");
                PRINT6ADDR(nexthop);
                PRINTF("\n");
        
                buffer = UIP_ICMP_PAYLOAD;
                pos = 0;
                memcpy(buffer, sender_addr, sizeof(uip_ipaddr_t));
                pos += sizeof(uip_ipaddr_t);
                memcpy(buffer + pos, group_addr, sizeof(uip_ipaddr_t));
                pos += sizeof(uip_ipaddr_t);
      
                uip_icmp6_send(nexthop, ICMP6_MULTICAST, PIM_JOIN, pos);
            }
        }
    } else {
        PRINTF("MCAST: Route not found to sender\n");
    } 
}


/**
 *      API to send prune message to the sender
 *      Arguments are similar to mcast6_join_send
 */
void 
mcast6_prune_send(uip_ipaddr_t *sender_addr, uip_ipaddr_t *group_addr)
{
    uip_ipaddr_t *nexthop;
    unsigned char *buffer;
    uip_mcast6_route_t *entry;
    int pos;
    
    nexthop = find_next_hop(sender_addr);
    
    if(nexthop != NULL) {
        entry = uip_mcast6_route_find(sender_addr, group_addr);
        if(entry != NULL) {
            if(entry->num_of_joins > 1) {
                PRINTF("MCAST: Tree still in use, just decreasing the number of joins\n");
                entry->num_of_joins--;
            } else {
                PRINTF("MCAST: Tree no longer needed, deleting state \n");
                entry->used = 0;
    
                PRINTF("MCAST: Sending PRUNE to ");
                PRINT6ADDR(sender_addr);
                PRINTF(" and next hop is ");
                PRINT6ADDR(nexthop);
                PRINTF("\n");        
        
                buffer = UIP_ICMP_PAYLOAD;
                pos = 0;
                memcpy(buffer, sender_addr, sizeof(uip_ipaddr_t));
                pos += sizeof(uip_ipaddr_t);
                memcpy(buffer + pos, group_addr, sizeof(uip_ipaddr_t));
                pos += sizeof(uip_ipaddr_t);
       
                uip_icmp6_send(nexthop, ICMP6_MULTICAST, PIM_PRUNE, pos);
            }
        } else {
           PRINTF("MCAST: State not found; Invalid Prune?\n");
        }
    } else {
        PRINTF("MCAST: Route not found to sender\n");
    } 
}

/**
 *      API to send heartbeat message to the the nexthop on the multicast tree
 *      @sender_addr is the sender address in the multicast state
 *      @group_addr is the multicast group address
 *      @dest_addr is the nexthop address towards sender in the multicast tree
 */
void 
mcast6_update_send(uip_ipaddr_t *sender_addr, uip_ipaddr_t *group_addr,
                   uip_ipaddr_t *dest_addr)
{
    unsigned char *buffer;
    int pos;

    buffer = UIP_ICMP_PAYLOAD;
    
    memcpy(buffer, sender_addr, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);
    memcpy(buffer + pos, group_addr, sizeof(uip_ipaddr_t));
    pos += sizeof(uip_ipaddr_t);

    uip_icmp6_send(dest_addr, ICMP6_MULTICAST, PIM_UPDATE, pos);
}
/**
 *      The entry function to handle PIM control messages
 *      and it invokes other function depending on the type of control packet
 *      This function is invoked by uip_process() in uip.c if type of ICMP
 *      is ICMP6_MULTICAST.
 */
void
pim_control_input(void)
{
    PRINTF("MCAST: Received a PIM control message\n");
    switch(UIP_ICMP_BUF->icode) {
    case PIM_JOIN:
        pim_join_in();
        break;
    case PIM_PRUNE:
        pim_prune_in();
        break;
    case PIM_UPDATE:
        pim_update_in();
        break;
    default:
        PRINTF("MCAST: PIM received unknown control code\n");
        break;
    }

    uip_len = 0;
}

#endif /* __PIM_H__ */

