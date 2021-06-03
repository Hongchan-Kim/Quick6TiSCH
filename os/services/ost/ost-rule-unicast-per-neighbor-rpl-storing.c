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

#if WITH_OST
#include "sys/ctimer.h"
#include "sys/clock.h"
#include "net/queuebuf.h"
#include "net/mac/tsch/tsch-queue.h"
#include "node-info.h"
#endif

#include "sys/log.h"
#define LOG_MODULE "OST"
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
#if WITH_OST
static uint16_t channel_offset;
#else
static uint16_t local_channel_offset;
#endif
static struct tsch_slotframe *sf_unicast;

/* OST implementation */
/*---------------------------------------------------------------------------*/
#if WITH_OST
#if  OST_HANDLE_QUEUED_PACKETS
uint8_t bootstrap_period;
#endif
/*---------------------------------------------------------------------------*/
void
print_nbr(void)
{
  printf("[Neighbors]");
  printf(" r_nbr / my / nbr / num_tx / new_add / my_installable / rx_no_path / my_low_prr\n");

  uip_ds6_nbr_t *nbr = uip_ds6_nbr_head();
  while(nbr != NULL) {
    uint16_t nbr_id = OST_NODE_ID_FROM_IPADDR(&(nbr->ipaddr));

    printf("[ID:%u]", nbr_id);
    printf(" %u / ", ost_is_routing_nbr(nbr));
    printf("Tx %u,%u / Rx %u,%u / %u / %u / %u / %u / %u (%u, %u, %u, %u, %u)\n",
      nbr->ost_my_N, nbr->ost_my_t_offset, nbr->ost_nbr_N, nbr->ost_nbr_t_offset, nbr->ost_num_tx,
      nbr->ost_newly_added, nbr->ost_my_installable, nbr->ost_rx_no_path, nbr->ost_my_low_prr, 
      nbr->ost_num_tx_mac, nbr->ost_num_tx_succ_mac, nbr->ost_num_consecutive_tx_fail_mac, 
      nbr->ost_consecutive_my_N_inc, nbr->ost_consecutive_new_tx_schedule_request);

    nbr = uip_ds6_nbr_next(nbr);
  }
}
/*---------------------------------------------------------------------------*/
void
ost_reset_nbr(const linkaddr_t *addr, uint8_t newly_added, uint8_t rx_no_path)
{
  if(addr != NULL) {
    LOG_INFO("ost reset_nbr %u\n", OST_NODE_ID_FROM_LINKADDR((linkaddr_t *)addr));
    uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)addr);
    if(nbr != NULL) {

      nbr->ost_my_N = 5;
      ost_update_N_of_packets_in_queue(addr, nbr->ost_my_N);
      nbr->ost_my_t_offset = 0xffff;

      nbr->ost_nbr_N = 0xffff;
      nbr->ost_nbr_t_offset = 0xffff;

      nbr->ost_num_tx = 0;

      if(newly_added == 1) {
        nbr->ost_newly_added = 1;
      } else {
        nbr->ost_newly_added = 0;
      }

      nbr->ost_my_installable = 1;

      if(rx_no_path == 1 ) {
        nbr->ost_rx_no_path = 1;
      } else {
        nbr->ost_rx_no_path = 0;
      }

      nbr->ost_my_low_prr = 0;
      nbr->ost_num_tx_mac = 0;
      nbr->ost_num_tx_succ_mac = 0;
      nbr->ost_num_consecutive_tx_fail_mac = 0;
      nbr->ost_consecutive_my_N_inc = 0;
      nbr->ost_consecutive_new_tx_schedule_request = 0;      
    }
  }
}
/*---------------------------------------------------------------------------*/
uint16_t
ost_get_tx_sf_handle_from_id(const uint16_t id)
{ 
  return 2 * id + 1; 
}
/*---------------------------------------------------------------------------*/
uint16_t
ost_get_rx_sf_handle_from_id(const uint16_t id)
{
  return 2 * id + 2; 
}
/*---------------------------------------------------------------------------*/
uint16_t
ost_get_id_from_tx_sf_handle(const uint16_t handle)
{
  return (handle - 1) / 2; 
}
/*---------------------------------------------------------------------------*/
uint16_t
ost_get_id_from_rx_sf_handle(const uint16_t handle)
{
  return (handle - 2) / 2; 
}
/*---------------------------------------------------------------------------*/
/* Seungbeom Jeong added change_attr_in_tx_queue */
/* use shared slotframe instead of RB or ost slotframes */
#if OST_HANDLE_QUEUED_PACKETS
void 
change_attr_in_tx_queue(const linkaddr_t * dest)
{
  uint16_t timeslot = 0;
  uint16_t sf_handle = 2;

  int16_t get_index = 0;
  uint8_t num_elements = 0;

  if(!tsch_is_locked()) {
    struct tsch_neighbor *dest_nbr = tsch_queue_get_nbr(dest);
    if(dest_nbr == NULL) {
      /* we do not have packets to change */
      return;
    }

    tsch_queue_backoff_reset(dest_nbr);

    get_index = ringbufindex_peek_get(&dest_nbr->tx_ringbuf);
    num_elements = ringbufindex_elements(&dest_nbr->tx_ringbuf);
    
    uint8_t i;
    for(i = get_index; i < get_index + num_elements; i++) {
      int16_t index;

      if(i >= ringbufindex_size(&dest_nbr->tx_ringbuf)) { /* default size: 16 */
        index = i - ringbufindex_size(&dest_nbr->tx_ringbuf);
      } else {
        index = i;  
      }

      set_queuebuf_attr(dest_nbr->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_SLOTFRAME, sf_handle);
      set_queuebuf_attr(dest_nbr->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
    }
  }
}
#endif
#endif
/*---------------------------------------------------------------------------*/

/* hckim: original Orchestra implementation */
/*---------------------------------------------------------------------------*/
static uint16_t
get_node_timeslot(const linkaddr_t *addr)
{
  if(addr != NULL && ORCHESTRA_UNICAST_PERIOD > 0) {
    return ORCHESTRA_LINKADDR_HASH(addr) % ORCHESTRA_UNICAST_PERIOD;
  } else {
    return 0xffff;
  }
}
/*---------------------------------------------------------------------------*/
#if !WITH_OST
static uint16_t
get_node_channel_offset(const linkaddr_t *addr)
{
  if(addr != NULL && ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET >= ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET) {
    return ORCHESTRA_LINKADDR_HASH(addr) % (ORCHESTRA_UNICAST_MAX_CHANNEL_OFFSET - ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET + 1)
        + ORCHESTRA_UNICAST_MIN_CHANNEL_OFFSET;
  } else {
    return 0xffff;
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST
int
#else
static int
#endif
neighbor_has_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL && !linkaddr_cmp(linkaddr, &linkaddr_null)) {
    if((orchestra_parent_knows_us || !ORCHESTRA_UNICAST_SENDER_BASED)
       && linkaddr_cmp(&orchestra_parent_linkaddr, linkaddr)) {
      return 1;
    }
    if(nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)linkaddr) != NULL) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
add_uc_link(const linkaddr_t *linkaddr)
{
  if(linkaddr != NULL) {
    uint16_t timeslot = get_node_timeslot(linkaddr);
    uint8_t link_options = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_RX : LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG;

    if(timeslot == get_node_timeslot(&linkaddr_node_addr)) {
      /* This is also our timeslot, add necessary flags */
      link_options |= ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX;
    }

    /* Add/update link.
     * Always configure the link with the local node's channel offset.
     * If this is an Rx link, that is what the node needs to use.
     * If this is a Tx link, packet's channel offset will override the link's channel offset.
     */
#if WITH_OST
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
          timeslot, channel_offset, 1);
#else
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
          timeslot, local_channel_offset, 1);
#endif
  }
}
/*---------------------------------------------------------------------------*/
static void
remove_uc_link(const linkaddr_t *linkaddr)
{
  uint16_t timeslot;
  struct tsch_link *l;

  if(linkaddr == NULL) {
    return;
  }

  timeslot = get_node_timeslot(linkaddr);
#if WITH_OST
  l = tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot, channel_offset);
#else
  l = tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot, local_channel_offset);
#endif
  if(l == NULL) {
    return;
  }
  /* Does our current parent need this timeslot? */
  if(timeslot == get_node_timeslot(&orchestra_parent_linkaddr)) {
    /* Yes, this timeslot is being used, return */
    return;
  }
  /* Does any other child need this timeslot?
   * (lookup all route next hops) */
  nbr_table_item_t *item = nbr_table_head(nbr_routes);
  while(item != NULL) {
    linkaddr_t *addr = nbr_table_get_lladdr(nbr_routes, item);
    if(timeslot == get_node_timeslot(addr)) {
      /* Yes, this timeslot is being used, return */
      return;
    }
    item = nbr_table_next(nbr_routes, item);
  }

  /* Do we need this timeslot? */
  if(timeslot == get_node_timeslot(&linkaddr_node_addr)) {
    /* This is our link, keep it but update the link options */
    uint8_t link_options = ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX;
#if WITH_OST
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
              timeslot, channel_offset, 1);
#else
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
              timeslot, local_channel_offset, 1);
#endif
  } else {
    /* Remove link */
    tsch_schedule_remove_link(sf_unicast, l);
  }
}
/*---------------------------------------------------------------------------*/
static void
child_added(const linkaddr_t *linkaddr)
{
#if WITH_OST
  ost_reset_nbr(linkaddr, 1, 0);
#endif
  add_uc_link(linkaddr);
}
/*---------------------------------------------------------------------------*/
static void
child_removed(const linkaddr_t *linkaddr)
{
#if WITH_OST
#if OST_HANDLE_QUEUED_PACKETS
  struct tsch_neighbor *nbr = tsch_queue_get_nbr(linkaddr);

  /* Seungbeom Jeong added this if and else code block */
  if(tsch_queue_is_empty(nbr) || nbr == NULL) {
    remove_uc_link(linkaddr); /* remove child immediately */
  } else {
    if(bootstrap_period == 1) {
      tsch_queue_flush_nbr_queue(nbr);
    } else {
      change_attr_in_tx_queue(linkaddr); /* use shared slotframe for queued packets */
    }
    remove_uc_link(linkaddr);
  }
#else
  remove_uc_link(linkaddr);
#endif

  uip_ds6_nbr_t *ds6_nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t*)linkaddr);  
  if(ds6_nbr != NULL) {
    /* if still routing nbr -> do not remove */
    if(ost_is_routing_nbr(ds6_nbr) == 0) { /* condition for prepvention of routing loop */
      ost_reset_nbr(linkaddr, 0, 0);
      /* it was deleted possibly already when no-path DAO rx */
      if(linkaddr != NULL) {
        ost_remove_tx((linkaddr_t *)linkaddr);
        ost_remove_rx(OST_NODE_ID_FROM_LINKADDR(linkaddr));
      }
    } else {
      /* do not remove this child, still needed */
    }
  }
#else
  remove_uc_link(linkaddr);
#endif
}
/*---------------------------------------------------------------------------*/
static int
select_packet(uint16_t *slotframe, uint16_t *timeslot, uint16_t *channel_offset)
{
  /* Select data packets we have a unicast link to */
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE) == FRAME802154_DATAFRAME
    && neighbor_has_uc_link(dest)) {

#if WITH_OST
    uint16_t dest_id = OST_NODE_ID_FROM_LINKADDR(dest);
    uint16_t tx_sf_handle = ost_get_tx_sf_handle_from_id(dest_id);
    struct tsch_slotframe *tx_sf = tsch_schedule_get_slotframe_by_handle(tx_sf_handle);

    if(slotframe != NULL) {
      if(tx_sf != NULL) { /* ost tx sloftrame exists -> use ost tx slotframe */
        *slotframe = tx_sf_handle;
      } else { /* use RB orchestra slotframe */
        *slotframe = slotframe_handle;
      }
    }

    if(timeslot != NULL) {
      if(tx_sf != NULL) { /* ost tx sloftrame exists -> use ost tx slotframe */
        struct tsch_link *l = list_head(tx_sf->links_list);
        /* Loop over all items. Assume there is max one link per timeslot */
        while(l != NULL) {
          if(l->slotframe_handle == tx_sf_handle) {
            break;
          }
          l = list_item_next(l);
        }
        *timeslot = l->timeslot;
      } else { /* use RB orchestra slotframe */
        *timeslot = ORCHESTRA_UNICAST_SENDER_BASED ? get_node_timeslot(&linkaddr_node_addr) : get_node_timeslot(dest);
      }
    }
    return 1;
#else
    if(slotframe != NULL) {
      *slotframe = slotframe_handle;
    }
    if(timeslot != NULL) {
      *timeslot = ORCHESTRA_UNICAST_SENDER_BASED ? get_node_timeslot(&linkaddr_node_addr) : get_node_timeslot(dest);
    }
    /* set per-packet channel offset */
    if(channel_offset != NULL) {
      *channel_offset = get_node_channel_offset(dest);
    }
    return 1;
#endif
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
new_time_source(const struct tsch_neighbor *old, const struct tsch_neighbor *new)
{
  if(new != old) {
    const linkaddr_t *old_addr = tsch_queue_get_nbr_address(old);
    const linkaddr_t *new_addr = tsch_queue_get_nbr_address(new);
    if(new_addr != NULL) {
      linkaddr_copy(&orchestra_parent_linkaddr, new_addr);
    } else {
      linkaddr_copy(&orchestra_parent_linkaddr, &linkaddr_null);
    }

#if WITH_OST
#if OST_HANDLE_QUEUED_PACKETS
    /* Seungbeom Jeong added this if block */
    if(old_addr != NULL) {
      if(tsch_queue_is_empty(old) || old == NULL) {
        remove_uc_link(old_addr); /* No-path DAO can be sent in shared slots */
      } else {
        if(bootstrap_period == 1) {
          tsch_queue_flush_nbr_queue((struct tsch_neighbor *)old);
        } else {
          change_attr_in_tx_queue(old_addr);
        }
        remove_uc_link(old_addr);
      }
    }
#else
    remove_uc_link(old_addr);
#endif

    ost_reset_nbr(old_addr, 0, 0);
    ost_reset_nbr(new_addr, 1, 0);

    if(old_addr != NULL) {
      ost_remove_tx((linkaddr_t *)old_addr);
      ost_remove_rx(OST_NODE_ID_FROM_LINKADDR(old_addr));
    }
#else
    remove_uc_link(old_addr);
#endif
    add_uc_link(new_addr);
  }
}
/*---------------------------------------------------------------------------*/
static void
init(uint16_t sf_handle)
{
  uint16_t timeslot;
  linkaddr_t *local_addr = &linkaddr_node_addr;

  slotframe_handle = sf_handle;
#if WITH_OST
  channel_offset = sf_handle;
#else
  local_channel_offset = get_node_channel_offset(local_addr);
#endif
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  timeslot = get_node_timeslot(local_addr);
#if WITH_OST
  tsch_schedule_add_link(sf_unicast,
            ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX,
            LINK_TYPE_NORMAL, &tsch_broadcast_address,
            timeslot, channel_offset, 1);
#else
  tsch_schedule_add_link(sf_unicast,
            ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX,
            LINK_TYPE_NORMAL, &tsch_broadcast_address,
            timeslot, local_channel_offset, 1);
#endif
}
/*---------------------------------------------------------------------------*/
struct orchestra_rule unicast_per_neighbor_rpl_storing = {
  init,
  new_time_source,
  select_packet,
  child_added,
  child_removed,
  "unicast per neighbor storing",
};

#endif /* UIP_MAX_ROUTES */
