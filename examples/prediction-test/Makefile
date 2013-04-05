all: sender receiver
APPS=serial-shell
CONTIKI=../../

WITH_UIP6=1
UIP_CONF_IPV6=1
#CFLAGS+= -DUIP_CONF_IPV6_RPL

CFLAGS+= -DUIP_CONF_IPV6_RPL -DDIFF_DOMAIN
SMALL=1

include $(CONTIKI)/Makefile.include
