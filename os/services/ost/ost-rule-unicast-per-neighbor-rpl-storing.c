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
/* OST implementation */
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
static uint16_t local_channel_offset;
static struct tsch_slotframe *sf_unicast;

/* OST implementation */
/*---------------------------------------------------------------------------*/
#if WITH_OST_CHECK
void
print_nbr(void)
{
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_head();
  printf("\n[Neighbors]");
  printf(" r_nbr / my / nbr / num_tx / new_add / my_uninstallable / rx_no_path / my_low_prr\n");
  while(nbr != NULL) {
    uint16_t nbr_id = node_id_from_ipaddr(&(nbr->ipaddr));

    printf("[ID:%u]",nbr_id);
    printf(" %u / ",is_routing_nbr(nbr));
    printf("Tx %u,%u / Rx %u,%u / %u / %u / %u / %u / %u (%u, %u, %u, %u, %u)\n",
      nbr->my_N,nbr->my_t_offset,nbr->nbr_N,nbr->nbr_t_offset,nbr->num_tx,
      nbr->new_add, nbr->my_uninstallable, nbr->rx_no_path , nbr->my_low_prr, 
      nbr->num_tx_mac, nbr->num_tx_succ_mac, nbr->num_consecutive_tx_fail_mac, nbr->consecutive_my_N_inc, 
      nbr->consecutive_new_tx_request);

    nbr = uip_ds6_nbr_next(nbr);
  }
  printf("\n");
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_04
void
reset_nbr(const linkaddr_t *addr, uint8_t new_add, uint8_t rx_no_path)
{
  if(addr != NULL) {
    uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)addr);
    if(nbr != NULL) {
      //LOG_INFO("reset_nbr %u\n", nbr_id);

      nbr->my_N = 5;
      ost_change_queue_N_update(addr, nbr->my_N);

      nbr->my_t_offset = 0xFFFF;

      nbr->nbr_N = 0xFFFF;
      nbr->nbr_t_offset = 0xFFFF;

      nbr->num_tx = 0;

      if(new_add == 1) {
        nbr->new_add = 1;
      } else {
        nbr->new_add = 0;
      }

      nbr->my_uninstallable = 0;

      if(rx_no_path == 1 ) {
        nbr->rx_no_path = 1;
      } else {
        nbr->rx_no_path = 0;
      }

      nbr->my_low_prr = 0;
      nbr->num_tx_mac = 0;
      nbr->num_tx_succ_mac = 0;
      nbr->num_consecutive_tx_fail_mac = 0;
      nbr->consecutive_my_N_inc = 0;
      nbr->consecutive_new_tx_request = 0;      
    }
  }
  //print_nbr();
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_CHECK
uint16_t
get_tx_sf_handle_from_id(const uint16_t id)
{ 
  return 2 * id + 1; 
}
/*---------------------------------------------------------------------------*/
uint16_t
get_rx_sf_handle_from_id(const uint16_t id)
{
  return 2 * id + 2; 
}
/*---------------------------------------------------------------------------*/
uint16_t
get_id_from_tx_sf_handle(const uint16_t handle)
{
  return (handle - 1) / 2; 
}
/*---------------------------------------------------------------------------*/
uint16_t
get_id_from_rx_sf_handle(const uint16_t handle)
{
  return (handle - 2) / 2; 
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_CHECK
void 
change_attr_in_tx_queue(const linkaddr_t * dest, uint8_t is_adjust_tx_sf_size, uint8_t only_first_packet)
{
  uint16_t timeslot;
  uint16_t sf_handle;
  int16_t get_index = -100;
  int16_t put_index = -200;
  uint8_t num_elements = 0;

  if(is_adjust_tx_sf_size == 0) {
    //Use shared slot
    timeslot = 0;  //NOTE1
    sf_handle = 2;
  } else { 
    printf("ERROR: is_adjust_tx_sf_size cannot be 0 in Orchestra\n");
  }
  if(!tsch_is_locked()) {
    struct tsch_neighbor * n1 = tsch_queue_get_nbr(dest);
    if(n1 == NULL) {
      printf("change_attr_in_tx_queue: My child, Tx queue for him is empty, so n1==NULL\n");
      //thus, we don't have packets to change
      return ;
    }

    tsch_queue_backoff_reset(n1);

    get_index = ringbufindex_peek_get(&n1->tx_ringbuf);
    put_index = ringbufindex_peek_put(&n1->tx_ringbuf);
    num_elements= ringbufindex_elements(&n1->tx_ringbuf);
    printf("get_index: %d, put_index: %d, %u\n",get_index,put_index,num_elements);

    if(only_first_packet == 1 && num_elements > 0) {
      //printf("only first packet\n");
      num_elements = 1;      
    }

    uint8_t j;
    for(j = get_index; j < get_index + num_elements; j++) {
      int16_t index;

      if(j >= ringbufindex_size(&n1->tx_ringbuf)) { //16
        index = j - ringbufindex_size(&n1->tx_ringbuf);
      } else {
        index = j;  
      }
      set_queuebuf_attr(n1->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_SLOTFRAME, sf_handle);
      set_queuebuf_attr(n1->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
      //printf("index: %u, %u %u\n",j,queuebuf_attr(n1->tx_array[index]->qb,PACKETBUF_ATTR_TSCH_SLOTFRAME),queuebuf_attr(n1->tx_array[index]->qb,PACKETBUF_ATTR_TSCH_TIMESLOT));
    }
  } else {
    printf("ERROR: lock fail (change_attr_in_tx_queue)\n");
  }
}
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
/*---------------------------------------------------------------------------*/
#if WITH_OST_04
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
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
          timeslot, local_channel_offset, 1);
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
  l = tsch_schedule_get_link_by_timeslot(sf_unicast, timeslot, local_channel_offset);
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
    tsch_schedule_add_link(sf_unicast, link_options, LINK_TYPE_NORMAL, &tsch_broadcast_address,
              timeslot, local_channel_offset, 1);
  } else {
    /* Remove link */
    tsch_schedule_remove_link(sf_unicast, l);
  }
}
/*---------------------------------------------------------------------------*/
static void
child_added(const linkaddr_t *linkaddr)
{
#if WITH_OST_CHECK
  reset_nbr(linkaddr, 1, 0); /* OST implementation */
#endif
  add_uc_link(linkaddr);
}
/*---------------------------------------------------------------------------*/
static void
child_removed(const linkaddr_t *linkaddr)
{
#if WITH_OST_CHECK
  struct tsch_neighbor *nbr = tsch_queue_get_nbr(linkaddr);

  if(tsch_queue_is_empty(nbr) || nbr == NULL) {
    LOG_INFO("child_removed: immediately\n");
    remove_uc_link(linkaddr);
  } else {
/*
    if(bootstrap_period) {
      printf("child_removed: queued packets, flush (bootstrap)\n");
      tsch_queue_flush_nbr_queue(nbr);
    } else */ {
      LOG_INFO("child_removed: queued packets, use shared slot\n");
      change_attr_in_tx_queue(linkaddr, 0, 0);
    }  
    remove_uc_link(linkaddr);
  }

  uip_ds6_nbr_t *ds6_nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t*)linkaddr);  
  if(ds6_nbr != NULL) {
    // If still routing nbr? do not remove
    if(is_routing_nbr(ds6_nbr) == 0) { // Added this condition for the preparation of routing loop
      reset_nbr(linkaddr, 0, 0);

      //it was deleted possibly already when no-path DAO rx
      LOG_INFO("child_removed: remove_tx & remove_rx\n");
      if(linkaddr!=NULL) {
        remove_tx(node_id_from_linkaddr(linkaddr));
        remove_rx(node_id_from_linkaddr(linkaddr));
        //tsch_schedule_print_proposed();
      }
    } else {
      LOG_INFO("child_removed: do not remove this child (still needed)\n");
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
#if WITH_OST_CHECK

    /* OST implementation */
    uint16_t dest_id = node_id_from_linkaddr(dest);
    uint16_t tx_sf_handle = get_tx_sf_handle_from_id(dest_id);
    struct tsch_slotframe *tx_sf = tsch_schedule_get_slotframe_by_handle(tx_sf_handle);

    if(slotframe != NULL) {
      if(tx_sf != NULL) {
        LOG_INFO("select_packet: tx_sf exist\n");
        *slotframe = tx_sf_handle;
      } else {
        LOG_INFO("select_packet: use RB\n");
        *slotframe = slotframe_handle;
      }
    }

    if(timeslot != NULL) {
      if(tx_sf != NULL) { // hckim: use tx_sf
        struct tsch_link *l = list_head(tx_sf->links_list);
        /* Loop over all items. Assume there is max one link per timeslot */
        while(l != NULL) {
          if(l->slotframe_handle == tx_sf_handle) {
            break;
          } else {
            printf("ERROR: Weird slotframe_handle for Tx link\n");
          }
          l = list_item_next(l);
        }
        *timeslot = l->timeslot;
      } else { // hckim: use RB
        *timeslot = ORCHESTRA_UNICAST_SENDER_BASED ? get_node_timeslot(&linkaddr_node_addr) : get_node_timeslot(dest);
      }
    }

    // hckim ost final check: is 'set per-paket channel offset' needed?
    /* set per-packet channel offset */
/*
    if(channel_offset != NULL) {
      *channel_offset = get_node_channel_offset(dest);
    }
*/

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

#if WITH_OST_CHECK
    /* OST implementation */
    if(old_addr != NULL) {
      if(tsch_queue_is_empty(old) || old == NULL) {
        remove_uc_link(old_addr); // No-path DAO can be sent in shared slots
      } else {
/*
        if(bootstrap_period) {
          tsch_queue_flush_nbr_queue(old);
        } else */ {
          change_attr_in_tx_queue(old_addr, 0, 0);
        }  
        remove_uc_link(old_addr);
      }
    }

    reset_nbr(old_addr, 0, 0);
    reset_nbr(new_addr, 1, 0);

    LOG_INFO("new_time_source: remove_tx & remove_rx\n");
    if(old_addr != NULL) {
      remove_tx(node_id_from_linkaddr(old_addr));
      remove_rx(node_id_from_linkaddr(old_addr));
      // tsch_schedule_print_proposed();
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
  local_channel_offset = get_node_channel_offset(local_addr);
  /* Slotframe for unicast transmissions */
  sf_unicast = tsch_schedule_add_slotframe(slotframe_handle, ORCHESTRA_UNICAST_PERIOD);
  timeslot = get_node_timeslot(local_addr);
  tsch_schedule_add_link(sf_unicast,
            ORCHESTRA_UNICAST_SENDER_BASED ? LINK_OPTION_TX | UNICAST_SLOT_SHARED_FLAG: LINK_OPTION_RX,
            LINK_TYPE_NORMAL, &tsch_broadcast_address,
            timeslot, local_channel_offset, 1);
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
