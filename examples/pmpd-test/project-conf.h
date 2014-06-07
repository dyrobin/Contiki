#ifndef __PROJECT_PMPD_TEST_CONF_H__
#define __PROJECT_PMPD_TEST_CONF_H__

/*
 * change default configurations of contiki-conf.h
 */
#if WITH_UIP6

#undef  NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver

#define NULLRDC_802154_AUTOACK           1

#undef  QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM                6

//#undef  NBR_TABLE_CONF_MAX_NEIGHBORS
//#define NBR_TABLE_CONF_MAX_NEIGHBORS    5//

//#undef  UIP_CONF_MAX_ROUTES
//#define UIP_CONF_MAX_ROUTES   5

//#ifndef SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS
//#define SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS   3
//#endif /* SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS */

#undef SICSLOWPAN_CONF_FLOWFILTER
#define SICSLOWPAN_CONF_FLOWFILTER	1

#endif /* WITH_UIP6 */

#undef 	UIP_CONF_MAX_CONNECTIONS
#define UIP_CONF_MAX_CONNECTIONS 0

#undef	UIP_CONF_UDP_CONNS
#define UIP_CONF_UDP_CONNS	4

#undef  UIP_CONF_UDP_CHECKSUMS
#define UIP_CONF_UDP_CHECKSUMS   0

//#define UIP_CONF_IPV6_BULK_DATA	 1

/*
 * Define other configurations
 */
#undef  IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID   0xABAB

#undef 	UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE 512

#define PMPD_ENABLED    0

#define DIFF_DOMAIN   0

#define DIFF_DOMAIN_RECEIVER 	0

#if DIFF_DOMAIN == 1 && DIFF_DOMAIN_RECEIVER == 1
#define SICSLOWPAN_CONF_ADDR_CONTEXT_0 {addr_contexts[0].prefix[0]=0xbb;addr_contexts[0].prefix[1]=0xbb;}
#endif

#endif /* __PROJECT_PMPD_TEST_CONF_H__ */
