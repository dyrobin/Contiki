ifeq ($(id),)
  id=1
endif

all: sender receiver

burnid:
	make clean && make burn-nodeid.upload nodeid=$(id) nodemac=$(id)
	make z1-reset && make login

APPS=serial-shell
CONTIKI=../../

WITH_UIP6=1
UIP_CONF_IPV6=1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

SMALL=1

include $(CONTIKI)/Makefile.include
