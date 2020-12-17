#ifndef NODE_INFO_H_
#define NODE_INFO_H_

#include "contiki-conf.h"
#include "net/ipv6/uip.h"

#include <stdio.h>

#if TESTBED_SITE == IOT_LAB_LYON
#define NODE_NUM        17
#define NON_ROOT_NUM    (NODE_NUM - 1)
#elif TESTBED_SITE == IOT_LAB_PARIS
#endif

extern uint16_t root_info[2];
extern uint16_t non_root_info[NON_ROOT_NUM][3];

void print_node_info();
uint16_t non_root_index_from_addr(const uip_ipaddr_t *);

#endif /* NODE_INFO_H_ */