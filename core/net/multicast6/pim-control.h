#ifndef __PIM_CONTROL_H__
#define __PIM_CONTROL_H__

#include "net/uip.h"

#define PIM_JOIN            0x00 /*Join message*/
#define PIM_PRUNE           0x01 /*Prune message*/
#define PIM_UPDATE          0x02 /*Hearbbeat message*/

/*
    Functions to handle control packets
*/

void pim_join_in();
void pim_prune_in();
void pim_update_in();
void pim_control_input();

/*
    Functions to send control packets
*/
void mcast6_join_send(uip_ipaddr_t *sender_addr, uip_ipaddr_t *group_addr,
                      uint8_t mld_flag);
void mcast6_prune_send(uip_ipaddr_t *sender_addr, uip_ipaddr_t *group_addr);
void mcast6_update_send(uip_ipaddr_t *sender_addr, uip_ipaddr_t *group_addr,
                        uip_ipaddr_t *dest_addr);

/*
    Util functions
*/
uip_ipaddr_t *find_next_hop(uip_ipaddr_t *addr);


#endif /* __PIM_CONTROL_H__ */

