#ifndef __PROJECT_PMPD_TEST_CONF_H__
#define __PROJECT_PMPD_TEST_CONF_H__

#undef 	PMPD_ENABLED
#define PMPD_ENABLED    1

#if PMPD_ENABLED == 0
#undef 	UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE 140
#endif

#undef 	DIFF_DOMAIN
#define DIFF_DOMAIN   1

#undef 	DIFF_DOMAIN_RECEIVER
#define DIFF_DOMAIN_RECEIVER 	0

#if DIFF_DOMAIN == 1 && DIFF_DOMAIN_RECEIVER == 1
#define SICSLOWPAN_CONF_ADDR_CONTEXT_0 {addr_contexts[0].prefix[0]=0xbb;addr_contexts[0].prefix[1]=0xbb;}
#endif

#endif /* __PROJECT_PMPD_TEST_CONF_H__ */
