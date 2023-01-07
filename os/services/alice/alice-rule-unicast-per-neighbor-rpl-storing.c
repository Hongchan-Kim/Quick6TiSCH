/*
 * Copyright (c) 2015, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *         Orchestra: a slotframe dedicated to unicast data transmission. Designed for
 *         RPL storing mode only, as this is based on the knowledge of the children (and parent).
 *         If receiver-based:
 *           Nodes listen at a timeslot defined as hash(MAC) % ORCHESTRA_SB_UNICAST_PERIOD
 *           Nodes transmit at: for each nbr in RPL children and RPL preferred parent,
 *                                             hash(nbr.MAC) % ORCHESTRA_SB_UNICAST_PERIOD
 *         If sender-based: the opposite
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki.h"
#include "orchestra.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/packetbuf.h"
#include "net/routing/routing.h"

#include "net/mac/tsch/tsch-schedule.h"
#include "net/mac/tsch/tsch.h"

#include "net/routing/rpl-classic/rpl.h"
#include "net/routing/rpl-classic/rpl-private.h"

#include "sys/log.h"
#define LOG_MODULE "ALICE"
#define LOG_LEVEL  LOG_LEVEL_MAC

/*
 * The body of this rule should be compiled only when "nbr_routes" is available,
 * otherwise a link error causes build failure. "nbr_routes" is compiled if
 * UIP_MAX_ROUTES != 0. See uip-ds6-route.c.
 */
#if UIP_MAX_ROUTES != 0

#if ORCHESTRA_UNICAST_SENDER_BASED && ORCHESTRA_COLLISION_FREE_HASH
#define UNICAST_SLOT_SHARED_FLAG    ((ORCHESTRA_UNICAST_PERIOD < (ORCHESTRA_MAX_HASH + 1)) ? LINK_OPTION_SHARED : 0)
#else
#define UNICAST_SLOT_SHARED_FLAG      LINK_OPTION_SHARED
#endif

static uint16_t slotframe_handle = 0;
static struct tsch_slotframe *sf_unicast;

static uint8_t alice_rx_link_option = LINK_OPTION_RX;
static uint8_t alice_tx_link_option = LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG; // If it is a shared link, backoff will be applied.

/*---------------------------------------------------------------------------*/
static uint16_t
get_node_timeslot(const linkaddr_t *addr1, const linkaddr_t *addr2)
{
  if(addr1 != NULL && addr2 != NULL && ORCHESTRA_UNICAST_PERIOD > 0) {
    /* ALICE: link-based timeslot determination */
    return alice_real_hash5(((uint32_t)ORCHESTRA_LINKADDR_HASH2(addr1, addr2) 
                             + (uint32_t)alice_lastly_scheduled_asfn), 
                            (ORCHESTRA_UNICAST_PERIOD));
  } else {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
static uint16_t
get_node_channel_offset(const linkaddr_t *addr1, const linkaddr_t *addr2)
{
  /* ALICE: except for EB channel offset (1) */
  int num_ch = (sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE) / sizeof(uint8_t)) - 1;
  if(addr1 != NULL && addr2 != NULL && num_ch > 0) {
    /* ALICE: link-based, except for EB channel offset (1) */
    return 1 + alice_real_hash5(((uint32_t)ORCHESTRA_LINKADDR_HASH2(addr1, addr2)
            + (uint32_t)alice_lastly_scheduled_asfn), num_ch); 
  } else {
    /* alice final check: should this be 0xffff ???? */
    return 1 + 0;
  }
}
/*---------------------------------------------------------------------------*/
static uint16_t
alice_is_root() /* alice final check: can be replaced with rpl_dag_root_is_root() function */
{
  rpl_instance_t *instance = rpl_get_default_instance();
  if(instance != NULL && instance->current_dag != NULL) {
    uint16_t min_hoprankinc = instance->min_hoprankinc;
    uint16_t rank = (uint16_t)instance->current_dag->rank;
    if(min_hoprankinc == rank) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Remove current slotframe scheduling and re-schedule this slotframe. */
static void
alice_schedule_unicast_slotframe(void)
{
    /* schedule for parent node */
  uint16_t upward_timeslot_for_parent, downward_timeslot_for_parent;
  uint16_t upward_channel_offset_for_parent, downward_channel_offset_for_parent;

  /* schedule for child node */
  uint16_t upward_timeslot_for_child, downward_timeslot_for_child;
  uint16_t upward_channel_offset_for_child, downward_channel_offset_for_child;

  /* link options */
  uint8_t upward_link_option, downward_link_option;

  /* Remove the whole links scheduled in the unicast slotframe */
  struct tsch_link *l;
  l = list_head(sf_unicast->links_list);
  while(l != NULL) {
    tsch_schedule_remove_link(sf_unicast, l);
    l = list_head(sf_unicast->links_list);
  }

  /* NO ROOT ONLY: Schedule the links for parent node */
  if(alice_is_root() != 1) {
    upward_timeslot_for_parent = get_node_timeslot(&linkaddr_node_addr, &orchestra_parent_linkaddr);
    upward_channel_offset_for_parent = get_node_channel_offset(&linkaddr_node_addr, &orchestra_parent_linkaddr);
    upward_link_option = alice_tx_link_option;
    alice_tsch_schedule_add_link(sf_unicast, upward_link_option, LINK_TYPE_NORMAL, 
                                &tsch_broadcast_address, upward_timeslot_for_parent, upward_channel_offset_for_parent,
                                HCK_GET_NODE_ID_FROM_LINKADDR(&orchestra_parent_linkaddr));

    downward_timeslot_for_parent = get_node_timeslot(&orchestra_parent_linkaddr, &linkaddr_node_addr);
    downward_channel_offset_for_parent = get_node_channel_offset(&orchestra_parent_linkaddr, &linkaddr_node_addr);
    downward_link_option = alice_rx_link_option;
    alice_tsch_schedule_add_link(sf_unicast, downward_link_option, LINK_TYPE_NORMAL, 
                                &tsch_broadcast_address, downward_timeslot_for_parent, downward_channel_offset_for_parent,
                                HCK_GET_NODE_ID_FROM_LINKADDR(&orchestra_parent_linkaddr));
  }

  /* Schedule the links for child nodes */
  nbr_table_item_t *item = nbr_table_head(nbr_routes);
  while(item != NULL) {
    linkaddr_t *addr = nbr_table_get_lladdr(nbr_routes, item);
#if ORCHESTRA_MODIFIED_CHILD_OPERATION && ORCHESTRA_UNICAST_SENDER_BASED
    struct uip_ds6_route_neighbor_routes *routes = nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)addr);
    struct uip_ds6_route_neighbor_route *neighbor_route;
    uint8_t true_child = 0;
    for(neighbor_route = list_head(routes->route_list);
        neighbor_route != NULL;
        neighbor_route = list_item_next(neighbor_route)) {
      if(ORCHESTRA_COMPARE_LINKADDR_AND_IPADDR(addr, &(neighbor_route->route->ipaddr))) {
        true_child = 1;
        break;
      }
    }
    if(true_child == 1) {
      upward_timeslot_for_child = get_node_timeslot(addr, &linkaddr_node_addr); 
      upward_channel_offset_for_child = get_node_channel_offset(addr, &linkaddr_node_addr);
      upward_link_option = alice_rx_link_option;
      alice_tsch_schedule_add_link(sf_unicast, upward_link_option, LINK_TYPE_NORMAL, 
                                  &tsch_broadcast_address, upward_timeslot_for_child, upward_channel_offset_for_child,
                                  HCK_GET_NODE_ID_FROM_LINKADDR(addr));

      downward_timeslot_for_child = get_node_timeslot(&linkaddr_node_addr, addr); 
      downward_channel_offset_for_child = get_node_channel_offset(&linkaddr_node_addr, addr);
      downward_link_option = alice_tx_link_option;
      alice_tsch_schedule_add_link(sf_unicast, downward_link_option, LINK_TYPE_NORMAL, 
                                  &tsch_broadcast_address, downward_timeslot_for_child, downward_channel_offset_for_child,
                                  HCK_GET_NODE_ID_FROM_LINKADDR(addr));
    }      
#else
    upward_timeslot_for_child = get_node_timeslot(addr, &linkaddr_node_addr);
    upward_channel_offset_for_child = get_node_channel_offset(addr, &linkaddr_node_addr);
    upward_link_option = alice_rx_link_option;
    alice_tsch_schedule_add_link(sf_unicast, upward_link_option, LINK_TYPE_NORMAL, 
                                &tsch_broadcast_address, upward_timeslot_for_child, upward_channel_offset_for_child,
                                HCK_GET_NODE_ID_FROM_LINKADDR(addr));

    downward_timeslot_for_child = get_node_timeslot(&linkaddr_node_addr, addr); 
    downward_channel_offset_for_child = get_node_channel_offset(&linkaddr_node_addr, addr);
    downward_link_option = alice_tx_link_option;
    alice_tsch_schedule_add_link(sf_unicast, downward_link_option, LINK_TYPE_NORMAL, 
                                &tsch_broadcast_address, downward_timeslot_for_child, downward_channel_offset_for_child,
                                HCK_GET_NODE_ID_FROM_LINKADDR(addr));
#endif

    /* move to the next item for while loop. */
    item = nbr_table_next(nbr_routes, item);
  }
}
/*---------------------------------------------------------------------------*/
static int
neighbor_has_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL && !linkaddr_cmp(linkaddr, &linkaddr_null)) {
    if((orchestra_parent_knows_us || !ORCHESTRA_UNICAST_SENDER_BASED)
       && linkaddr_cmp(&orchestra_parent_linkaddr, linkaddr)) {
      return 1;
    }

#if ORCHESTRA_MODIFIED_CHILD_OPERATION && ORCHESTRA_UNICAST_SENDER_BASED
    struct uip_ds6_route_neighbor_routes *routes;
    routes = nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)linkaddr);
    if(routes != NULL) {
      struct uip_ds6_route_neighbor_route *neighbor_route;
      for(neighbor_route = list_head(routes->route_list);
          neighbor_route != NULL;
          neighbor_route = list_item_next(neighbor_route)) {
        if(ORCHESTRA_COMPARE_LINKADDR_AND_IPADDR(linkaddr, &(neighbor_route->route->ipaddr))) {
          return 1;
        }
      }
    }      
#else /* original code */
    if(nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)linkaddr) != NULL) {
      return 1;
    }
#endif
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* packet_selection_callback. */
#ifdef ALICE_PACKET_CELL_MATCHING_ON_THE_FLY
int
alice_packet_cell_matching_on_the_fly(uint16_t *timeslot, uint16_t *channel_offset, const linkaddr_t *rx_linkaddr)
{
  /*
   * ALICE: First checks if rx_linkaddr is still the node's RPL neighbor,
   * and returns the current link's timeoffset and channel_offset.
   * Then, the caller function will check wether this packet can be transmitted at the current link.
   */
  int is_rpl_neighbor = 0;

  /* Check parent first */
  if(linkaddr_cmp(&orchestra_parent_linkaddr, rx_linkaddr)) {
    is_rpl_neighbor = 1;

    *timeslot = get_node_timeslot(&linkaddr_node_addr, rx_linkaddr);
    *channel_offset = get_node_channel_offset(&linkaddr_node_addr, rx_linkaddr); 

    return is_rpl_neighbor;
  }

  /* Check child next */
  nbr_table_item_t *item = nbr_table_get_from_lladdr(nbr_routes, rx_linkaddr);
#if ORCHESTRA_MODIFIED_CHILD_OPERATION && ORCHESTRA_UNICAST_SENDER_BASED
  if(item != NULL) {
    struct uip_ds6_route_neighbor_routes *routes;
    routes = nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)rx_linkaddr);
    struct uip_ds6_route_neighbor_route *neighbor_route;
    uint8_t true_child = 0;
    for(neighbor_route = list_head(routes->route_list);
        neighbor_route != NULL;
        neighbor_route = list_item_next(neighbor_route)) {
      if(ORCHESTRA_COMPARE_LINKADDR_AND_IPADDR(rx_linkaddr, &(neighbor_route->route->ipaddr))) {
        true_child = 1;
        break;
      }
    }
    if(true_child == 1) {
      is_rpl_neighbor = 1;

      *timeslot = get_node_timeslot(&linkaddr_node_addr, rx_linkaddr);
      *channel_offset = get_node_channel_offset(&linkaddr_node_addr, rx_linkaddr); 

      return is_rpl_neighbor;
    }
  }
#else
  if(item != NULL) {
    is_rpl_neighbor = 1;

    *timeslot = get_node_timeslot(&linkaddr_node_addr, rx_linkaddr);
    *channel_offset = get_node_channel_offset(&linkaddr_node_addr, rx_linkaddr); 

    return is_rpl_neighbor;
  }
#endif

  /* 
   * At this point, is_rpl_neighbor is zero
   * and this packet's receiver is not the node's RPL neighbor.
   */
  /* alice final check - what the return values shoud be? */
  *timeslot = 0;
  *channel_offset = ALICE_COMMON_SF_HANDLE;

  return is_rpl_neighbor; // returns 0 -> triggers ALICE_EARLY_PACKET_DROP
}
#endif
/*---------------------------------------------------------------------------*/
/* slotframe_callback. */
#ifdef ALICE_TIME_VARYING_SCHEDULING
void
alice_time_varying_scheduling()
{  
  alice_schedule_unicast_slotframe();
}
#endif
/*---------------------------------------------------------------------------*/
static void
child_added(const linkaddr_t *linkaddr)
{
  alice_schedule_unicast_slotframe();
}
/*---------------------------------------------------------------------------*/
static void
child_removed(const linkaddr_t *linkaddr)
{
#if HCK_ORCHESTRA_PACKET_OFFLOADING
  const struct tsch_neighbor *removed_child = tsch_queue_get_nbr(linkaddr);
  tsch_queue_change_attr_of_packets_in_queue(removed_child, ALICE_COMMON_SF_HANDLE, 0);
#endif
  alice_schedule_unicast_slotframe();
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME 
      && neighbor_has_uc_link(dest)) {
    if(slotframe != NULL) {
      *slotframe = slotframe_handle;
    }
    if(timeslot != NULL) {
      *timeslot = get_node_timeslot(&linkaddr_node_addr, dest);
    }
    if(channel_offset != NULL) {
      *channel_offset = get_node_channel_offset(&linkaddr_node_addr, dest);
    }
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  if(new != old) {
    const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
    if(new_addr != NULL) {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);    
    } else {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }

#if HCK_ORCHESTRA_PACKET_OFFLOADING
    tsch_queue_change_attr_of_packets_in_queue(old, ALICE_COMMON_SF_HANDLE, 0);
#endif

    alice_schedule_unicast_slotframe(); 
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  slotframe_handle = sf_handle;
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
}
/*---------------------------------------------------------------------------*/
struct orchestra_rule unicast_per_neighbor_rpl_storing = {
  init,
  new_time_source,
  select_packet,
  child_added,
  child_removed,
  "ALICE unicast per neighbor storing",
};

#endif /* UIP_MAX_ROUTES */
