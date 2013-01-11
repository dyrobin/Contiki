all: root child
APPS=serial-shell
CONTIKI=../../

WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS+= -DUIP_CONF_IPV6_RPL
SMALL=1

include $(CONTIKI)/Makefile.include
