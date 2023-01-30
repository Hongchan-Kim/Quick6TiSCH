#ifndef NODE_INFO_H_
#define NODE_INFO_H_

#include "contiki-conf.h"
#include "net/ipv6/uip.h"

#include <stdio.h>

#if WITH_IOTLAB
#define APP_ROOT_ID                    1
#define NON_ROOT_NUM                   (NODE_NUM - 1)

extern uint16_t iotlab_nodes[NODE_NUM][3];
uint16_t iotlab_node_id_from_uid(uint16_t uid);
void print_iotlab_node_info();
#endif

#endif /* NODE_INFO_H_ */