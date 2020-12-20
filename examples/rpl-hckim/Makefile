CONTIKI_PROJECT = udp-client udp-server
all: $(CONTIKI_PROJECT)

CONTIKI=../..

MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC
MAKE_MAC = MAKE_MAC_TSCH
MODULES += os/services/orchestra
MODULES += os/services/simple-energest

PROJECT_SOURCEFILES += node-info.c

include $(CONTIKI)/Makefile.include
