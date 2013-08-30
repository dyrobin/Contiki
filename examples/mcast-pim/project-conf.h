#ifndef __PROJECT_BASIC_SHELL_CONF_H__
#define __PROJECT_BASIC_SHELL_CONF_H__

#include "net/multicast6/uip-mcast6-engines.h"

#define UIP_MCAST6_CONF_ENGINE  UIP_MCAST6_ENGINE_PIM

#undef  UIP_CONF_IPV6
#undef  UIP_CONF_IPV6_RPL
#define UIP_CONF_IPV6     1
#define UIP_CONF_IPV6_RPL 1

#undef UIP_CONF_DS6_NBR_NBU
#define UIP_CONF_DS6_NBR_NBU     10
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_ROUTE_NBU   10

#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM               4

/* Duty-cycling algorithm selection. nullrdc_ or contikimac_
 * The default is contikimac_ for z1 */
#undef  NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC contikimac_driver

/* default is 0 for z1 */
#undef  NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK 0

#endif
