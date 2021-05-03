#ifndef NODE_INFO_H_
#define NODE_INFO_H_

#include "contiki-conf.h"
#include "net/ipv6/uip.h"

#include <stdio.h>

#define IOTLAB_ROOT_ID                 1
#define NON_ROOT_NUM                    (NODE_NUM - 1)

#if WITH_IOTLAB
extern uint16_t iotlab_nodes[NODE_NUM][3];
uint16_t iotlab_node_id_from_uid(uint16_t uid);
#endif
void print_node_info();
uint16_t ost_node_id_from_ipaddr(const uip_ipaddr_t *addr);
uint16_t ost_node_id_from_linkaddr(const linkaddr_t *addr);



extern uint16_t root_info[3];
extern uint16_t non_root_info[NON_ROOT_NUM][3];
uint16_t non_root_index_from_addr(const uip_ipaddr_t *);

#endif /* NODE_INFO_H_ */