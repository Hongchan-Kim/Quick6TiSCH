#ifndef NODE_INFO_H_
#define NODE_INFO_H_

#include "contiki-conf.h"
#include "net/ipv6/uip.h"

#include <stdio.h>

#if TESTBED_SITE == IOT_LAB_LYON_2
#define NODE_NUM        2
#elif TESTBED_SITE == IOT_LAB_LYON_17
#define NODE_NUM        17
#elif TESTBED_SITE == IOT_LAB_LILLE_46
#define NODE_NUM        46
#endif

#define NON_ROOT_NUM    (NODE_NUM - 1)

extern uint16_t root_info[3];
extern uint16_t non_root_info[NON_ROOT_NUM][3];

void print_node_info();
uint16_t non_root_index_from_addr(const uip_ipaddr_t *);

#endif /* NODE_INFO_H_ */