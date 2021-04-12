/*
 * Copyright (c) 2014, SICS Swedish ICT.
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
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Per-neighbor packet queues for TSCH MAC.
 *         The list of neighbors uses the TSCH lock, but per-neighbor packet array are lock-free.
 *				 Read-only operation on neighbor and packets are allowed from interrupts and outside of them.
 *				 *Other operations are allowed outside of interrupt only.*
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 *         Beshr Al Nahas <beshr@sics.se>
 *         Domenico De Guglielmo <d.deguglielmo@iet.unipi.it >
 *         Atis Elsts <atis.elsts@edi.lv>
 */

/**
 * \addtogroup tsch
 * @{
*/

#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/queuebuf.h"
#include "net/mac/tsch/tsch.h"
#include "net/nbr-table.h"
#include <string.h>

#if WITH_OST /* checked */
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "sys/ctimer.h"
#include "orchestra.h"
#include "node-info.h"
#endif

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "TSCH Queue"
#define LOG_LEVEL LOG_LEVEL_MAC

/* Check if TSCH_QUEUE_NUM_PER_NEIGHBOR is power of two */
#if (TSCH_QUEUE_NUM_PER_NEIGHBOR & (TSCH_QUEUE_NUM_PER_NEIGHBOR - 1)) != 0
#error TSCH_QUEUE_NUM_PER_NEIGHBOR must be power of two
#endif

#if WITH_ALICE == 1 /* alice implementation */
#ifdef ALICE_F_PACKET_CELL_MATCHING_ON_THE_FLY /* alice packet cell matching on the fly */
int ALICE_F_PACKET_CELL_MATCHING_ON_THE_FLY(uint16_t* timeslot, uint16_t* channel_offset, const linkaddr_t rx_linkaddr);
#endif
#endif

/* We have as many packets are there are queuebuf in the system */
MEMB(packet_memb, struct tsch_packet, QUEUEBUF_NUM);
NBR_TABLE(struct tsch_neighbor, tsch_neighbors);

/* Broadcast and EB virtual neighbors */
struct tsch_neighbor *n_broadcast;
struct tsch_neighbor *n_eb;

#if WITH_OST_01 /* checked */
static struct ctimer select_N_timer;
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_01 /* checked */
uint8_t is_routing_nbr(uip_ds6_nbr_t *nbr)
{
  // parent?
  if(linkaddr_cmp(&orchestra_parent_linkaddr, (linkaddr_t *)uip_ds6_nbr_get_ll(nbr))) {
    return 1;
  }

  // 1-hop child? i.e., existing next-hop
  nbr_table_item_t *item = nbr_table_get_from_lladdr(nbr_routes, (linkaddr_t *)uip_ds6_nbr_get_ll(nbr));
  if(item != NULL) {
    return 1;
  }

  return 0;    
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_03
void
ost_change_queue_N_update(const linkaddr_t *lladdr, uint16_t updated_N)
{
  struct tsch_neighbor *n = tsch_queue_get_nbr(lladdr);
  if(n != NULL) {
    if(!ringbufindex_empty(&n->tx_ringbuf)) {
      int16_t get_index = ringbufindex_peek_get(&n->tx_ringbuf);
      uint8_t num_elements = ringbufindex_elements(&n->tx_ringbuf);

//      LOG_INFO("change_queue_N_update: %u %u (nbr %u)\n", updated_N, num_elements, nbr_id);

      uint8_t j;
      for(j = get_index; j < get_index + num_elements; j++) {
        int8_t index;
        if(j >= ringbufindex_size(&n->tx_ringbuf)) { //16
          index = j - ringbufindex_size(&n->tx_ringbuf);
        } else {
          index = j;
        }
        uint8_t *packet = (uint8_t *)queuebuf_dataptr(n->tx_array[index]->qb);

#if WITH_OST_DBG
        uint8_t old_2 = packet[2];
        uint8_t old_3 = packet[3];
#endif

        packet[2] = updated_N & 0xff;
        packet[3] = (updated_N >> 8) & 0xff;

#if WITH_OST_DBG
        LOG_INFO("old %u %u new %u %u\n", old_2, old_3, packet[2], packet[3]);
#endif
      }
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_01 /* checked */
/* OST measure traffic load */
void select_N(void* ptr)
{
  uip_ds6_nbr_t *nbr;
  uint16_t traffic_load;
  int i;

  /* select N */
  nbr = uip_ds6_nbr_head();
  if(tsch_get_lock()) {
    while(nbr != NULL) {
      if(is_routing_nbr(nbr) && nbr->new_add == 0) {
        uint16_t num_slots = N_SELECTION_PERIOD * 1000 / 10;
        // unit: packet/slot multiplied by 2^N_MAX
        traffic_load = (nbr->num_tx) / num_slots * (1 << N_MAX);

        for(i = 1; i <= N_MAX; i++) {
          if((traffic_load >> i) < 1) {
            uint16_t old_N = nbr->my_N;
            uint16_t new_N = (N_MAX - i) + 1 - MORE_UNDER_PROVISION;

            if(old_N != new_N) {
              uint8_t change_N = 0;
              if(new_N > old_N) { //increase my_N
                nbr->consecutive_my_N_inc++;
                if(nbr->consecutive_my_N_inc >= THRES_CONSEQUTIVE_N_INC) {
                  nbr->consecutive_my_N_inc = 0;
                  change_N = 1;
                } else {
                  change_N = 0;
                }
              } else { //decrease my_N
                nbr->consecutive_my_N_inc = 0;
                change_N = 1;
              }
              if(change_N) {

#if WITH_OST_03
                ost_change_queue_N_update((linkaddr_t *)uip_ds6_nbr_get_ll(nbr), new_N);
#endif
                nbr->my_N = new_N;

#if WITH_OST_DBG
                LOG_INFO("select_N %u\n", new_N);
#endif
              } else {
              }
            } else { //No change
              nbr->consecutive_my_N_inc = 0;
            }
            break;
          }
        }
      } else {
        nbr->my_N = 5;
        if(nbr->new_add == 1) { 
          nbr->new_add = 0;
        }
      }
      nbr = uip_ds6_nbr_next(nbr);
    }

    // Reset all num_tx
    nbr = uip_ds6_nbr_head();
    while(nbr != NULL) {
      nbr->num_tx = 0;
      nbr = uip_ds6_nbr_next(nbr);
    }

    tsch_release_lock();
  }
  ctimer_reset(&select_N_timer);
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST_01 /* checked */
struct tsch_neighbor *
tsch_queue_get_nbr_from_id(const uint16_t id)
{
    struct tsch_neighbor *n = (struct tsch_neighbor *)nbr_table_head(tsch_neighbors);
    while(n != NULL) {
      if(ost_node_index_from_linkaddr(tsch_queue_get_nbr_address(n)) == id) {
        return n;
      }
      n = list_item_next(n);
    }
  return NULL;
}
#endif


/*---------------------------------------------------------------------------*/
/* Add a TSCH neighbor */
struct tsch_neighbor *
tsch_queue_add_nbr(const linkaddr_t *addr)
{
  struct tsch_neighbor *n = NULL;
  /* If we have an entry for this neighbor already, we simply update it */
  n = tsch_queue_get_nbr(addr);
  if(n == NULL) {
    if(tsch_get_lock()) {
      /* Allocate a neighbor */
      n = (struct tsch_neighbor *)nbr_table_add_lladdr(tsch_neighbors, addr, NBR_TABLE_REASON_MAC, NULL);
      if(n != NULL) {
        /* Do not allow to garbage collect this neighbor by external code!
         * The garbage collection is not aware of the tsch_lock, so is not interrupt safe.
         */
        nbr_table_lock(tsch_neighbors, n);
        /* Initialize neighbor entry */
        memset(n, 0, sizeof(struct tsch_neighbor));
        ringbufindex_init(&n->tx_ringbuf, TSCH_QUEUE_NUM_PER_NEIGHBOR);
        n->is_broadcast = linkaddr_cmp(addr, &tsch_eb_address)
          || linkaddr_cmp(addr, &tsch_broadcast_address);
        tsch_queue_backoff_reset(n);
      }
      tsch_release_lock();
    }
  }
  return n;
}
/*---------------------------------------------------------------------------*/
/* Get a TSCH neighbor */
struct tsch_neighbor *
tsch_queue_get_nbr(const linkaddr_t *addr)
{
  if(!tsch_is_locked()) {
    return (struct tsch_neighbor *)nbr_table_get_from_lladdr(tsch_neighbors, addr);
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Get a TSCH time source (we currently assume there is only one) */
struct tsch_neighbor *
tsch_queue_get_time_source(void)
{
  if(!tsch_is_locked()) {
    struct tsch_neighbor *curr_nbr = (struct tsch_neighbor *)nbr_table_head(tsch_neighbors);
    while(curr_nbr != NULL) {
      if(curr_nbr->is_time_source) {
        return curr_nbr;
      }
      curr_nbr = (struct tsch_neighbor *)nbr_table_next(tsch_neighbors, curr_nbr);
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
linkaddr_t *
tsch_queue_get_nbr_address(const struct tsch_neighbor *n)
{
  return nbr_table_get_lladdr(tsch_neighbors, n);
}
/*---------------------------------------------------------------------------*/
/* Update TSCH time source */
int
tsch_queue_update_time_source(const linkaddr_t *new_addr)
{
  if(!tsch_is_locked()) {
    if(!tsch_is_coordinator) {
      struct tsch_neighbor *old_time_src = tsch_queue_get_time_source();
      struct tsch_neighbor *new_time_src = NULL;

      if(new_addr != NULL) {
        /* Get/add neighbor, return 0 in case of failure */
        new_time_src = tsch_queue_add_nbr(new_addr);
        if(new_time_src == NULL) {
          return 0;
        }
      }

      if(new_time_src != old_time_src) {
        LOG_INFO("update time source: ");
        LOG_INFO_LLADDR(tsch_queue_get_nbr_address(old_time_src));
        LOG_INFO_(" -> ");
        LOG_INFO_LLADDR(tsch_queue_get_nbr_address(new_time_src));
        LOG_INFO_("\n");

        /* Update time source */
        if(new_time_src != NULL) {
          new_time_src->is_time_source = 1;
          /* (Re)set keep-alive timeout */
          tsch_set_ka_timeout(TSCH_KEEPALIVE_TIMEOUT);
        } else {
          /* Stop sending keepalives */
          tsch_set_ka_timeout(0);
        }

        if(old_time_src != NULL) {
          old_time_src->is_time_source = 0;
        }

        tsch_stats_reset_neighbor_stats();

#ifdef TSCH_CALLBACK_NEW_TIME_SOURCE
        TSCH_CALLBACK_NEW_TIME_SOURCE(old_time_src, new_time_src);
#endif
      }
#if WITH_OST_09
      else {
        //For First association (First DIO Rx) 
        reset_nbr(new_addr, 1, 0);
      }
#endif

      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Flush a neighbor queue */
#if WITH_OST_09
void
tsch_queue_flush_nbr_queue(struct tsch_neighbor *n)
#else
static void
tsch_queue_flush_nbr_queue(struct tsch_neighbor *n)
#endif
{
  while(!tsch_queue_is_empty(n)) {
    struct tsch_packet *p = tsch_queue_remove_packet_from_queue(n);
    if(p != NULL) {
      /* Set return status for packet_sent callback */
      p->ret = MAC_TX_ERR;
      LOG_WARN("! flushing packet\n");
      /* Call packet_sent callback */
      mac_call_sent_callback(p->sent, p->ptr, p->ret, p->transmissions);
      /* Free packet queuebuf */
      tsch_queue_free_packet(p);
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Remove TSCH neighbor queue */
static void
tsch_queue_remove_nbr(struct tsch_neighbor *n)
{
  if(n != NULL) {
    if(tsch_get_lock()) {

      tsch_release_lock();

      /* Flush queue */
      tsch_queue_flush_nbr_queue(n);

      /* Free neighbor */
      nbr_table_remove(tsch_neighbors, n);
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Add packet to neighbor queue. Use same lockfree implementation as ringbuf.c (put is atomic) */
struct tsch_packet *
tsch_queue_add_packet(const linkaddr_t *addr, uint8_t max_transmissions,
                      mac_callback_t sent, void *ptr)
{
  struct tsch_neighbor *n = NULL;
  int16_t put_index = -1;
  struct tsch_packet *p = NULL;

#ifdef TSCH_CALLBACK_PACKET_READY
  /* The scheduler provides a callback which sets the timeslot and other attributes */
  if(TSCH_CALLBACK_PACKET_READY() < 0) {
    /* No scheduled slots for the packet available; drop it early to save queue space. */
    LOG_DBG("tsch_queue_add_packet(): rejected by the scheduler\n");
    return NULL;
  }
#endif

#if WITH_OST_01 /* checked */
  /*
   * OST measure traffic load
   */
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)addr);
  if(nbr != NULL) {
    nbr->num_tx++; //even if q_loss
  }
#endif

  if(!tsch_is_locked()) {
    n = tsch_queue_add_nbr(addr);
    if(n != NULL) {
      put_index = ringbufindex_peek_put(&n->tx_ringbuf);
      if(put_index != -1) {
        p = memb_alloc(&packet_memb);
        if(p != NULL) {
          /* Enqueue packet */
          p->qb = queuebuf_new_from_packetbuf();
          if(p->qb != NULL) {
            p->sent = sent;
            p->ptr = ptr;
            p->ret = MAC_TX_DEFERRED;
            p->transmissions = 0;
            p->max_transmissions = max_transmissions;
            /* Add to ringbuf (actual add committed through atomic operation) */
            n->tx_array[put_index] = p;
            ringbufindex_put(&n->tx_ringbuf);
            LOG_DBG("packet is added put_index %u, packet %p\n",
                   put_index, p);
            return p;
          } else {
            memb_free(&packet_memb, p);
          }
        }
      }
    }
  }
  LOG_ERR("! add packet failed: %u %p %d %p %p\n", tsch_is_locked(), n, put_index, p, p ? p->qb : NULL);
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Returns the number of packets currently in any TSCH queue */
int
tsch_queue_global_packet_count(void)
{
  return QUEUEBUF_NUM - memb_numfree(&packet_memb);
}
/*---------------------------------------------------------------------------*/
/* Returns the number of packets currently in the queue */
int
tsch_queue_nbr_packet_count(const struct tsch_neighbor *n)
{
  if(n != NULL) {
    return ringbufindex_elements(&n->tx_ringbuf);
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
/* Remove first packet from a neighbor queue */
struct tsch_packet *
tsch_queue_remove_packet_from_queue(struct tsch_neighbor *n)
{
  if(!tsch_is_locked()) {
    if(n != NULL) {
      /* Get and remove packet from ringbuf (remove committed through an atomic operation */
      int16_t get_index = ringbufindex_get(&n->tx_ringbuf);
      if(get_index != -1) {
        return n->tx_array[get_index];
      } else {
        return NULL;
      }
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Free a packet */
void
tsch_queue_free_packet(struct tsch_packet *p)
{
  if(p != NULL) {
    queuebuf_free(p->qb);
    memb_free(&packet_memb, p);
  }
}
/*---------------------------------------------------------------------------*/
/* Updates neighbor queue state after a transmission */
int
tsch_queue_packet_sent(struct tsch_neighbor *n, struct tsch_packet *p,
                      struct tsch_link *link, uint8_t mac_tx_status)
{
  int in_queue = 1;
  int is_shared_link = link->link_options & LINK_OPTION_SHARED;
  int is_unicast = !n->is_broadcast;

  if(mac_tx_status == MAC_TX_OK) {
    /* Successful transmission */
    tsch_queue_remove_packet_from_queue(n);
    in_queue = 0;

    /* Update CSMA state in the unicast case */
    if(is_unicast) {
      if(is_shared_link || tsch_queue_is_empty(n)) {
        /* If this is a shared link, reset backoff on success.
         * Otherwise, do so only is the queue is empty */
        tsch_queue_backoff_reset(n);
      }
    }
  } else {
    /* Failed transmission */
    if(p->transmissions >= p->max_transmissions) {
      /* Drop packet */
      tsch_queue_remove_packet_from_queue(n);
      in_queue = 0;
    }
    /* Update CSMA state in the unicast case */
    if(is_unicast) {
      /* Failures on dedicated (== non-shared) leave the backoff
       * window nor exponent unchanged */
      if(is_shared_link) {
        /* Shared link: increment backoff exponent, pick a new window */
        tsch_queue_backoff_inc(n);
      }
    }
  }

  return in_queue;
}
/*---------------------------------------------------------------------------*/
/* Flush all neighbor queues */
void
tsch_queue_reset(void)
{
  /* Deallocate unneeded neighbors */
  if(!tsch_is_locked()) {
    struct tsch_neighbor *n = (struct tsch_neighbor *)nbr_table_head(tsch_neighbors);
    while(n != NULL) {
      struct tsch_neighbor *next_n = (struct tsch_neighbor *)nbr_table_next(tsch_neighbors, n);
      /* Flush queue */
      tsch_queue_flush_nbr_queue(n);
      /* Reset backoff exponent */
      tsch_queue_backoff_reset(n);
      n = next_n;
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Deallocate neighbors with empty queue */
void
tsch_queue_free_unused_neighbors(void)
{
  /* Deallocate unneeded neighbors */
  if(!tsch_is_locked()) {
    struct tsch_neighbor *n = (struct tsch_neighbor *)nbr_table_head(tsch_neighbors);
    while(n != NULL) {
      struct tsch_neighbor *next_n = (struct tsch_neighbor *)nbr_table_next(tsch_neighbors, n);
      /* Queue is empty, no tx link to this neighbor: deallocate.
       * Always keep time source and virtual broadcast neighbors. */
      if(!n->is_broadcast && !n->is_time_source && !n->tx_links_count
         && tsch_queue_is_empty(n)) {
        tsch_queue_remove_nbr(n);
      }
      n = next_n;
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Is the neighbor queue empty? */
int
tsch_queue_is_empty(const struct tsch_neighbor *n)
{
  return !tsch_is_locked() && n != NULL && ringbufindex_empty(&n->tx_ringbuf);
}
/*---------------------------------------------------------------------------*/
/* Returns the first packet from a neighbor queue */
struct tsch_packet *
tsch_queue_get_packet_for_nbr(const struct tsch_neighbor *n, struct tsch_link *link)
{
  if(!tsch_is_locked()) {
    int is_shared_link = link != NULL && link->link_options & LINK_OPTION_SHARED;
    if(n != NULL) {
      int16_t get_index = ringbufindex_peek_get(&n->tx_ringbuf);
      if(get_index != -1 &&
          !(is_shared_link && !tsch_queue_backoff_expired(n))) {    /* If this is a shared link,
                                                                    make sure the backoff has expired */
#if TSCH_WITH_LINK_SELECTOR
#if WITH_OST_CHECK && RESIDUAL_ALLOC //hckim??
        if(link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET && link->link_options == LINK_OPTION_TX) {
          uint16_t target_nbr_id = (link->slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
          if(ost_node_index_from_linkaddr(tsch_queue_get_nbr_address(n)) == target_nbr_id) {
            return n->tx_array[get_index];
          } else {
            return NULL;
          }
        }
#endif        

        int packet_attr_slotframe = queuebuf_attr(n->tx_array[get_index]->qb, PACKETBUF_ATTR_TSCH_SLOTFRAME);
        int packet_attr_timeslot = queuebuf_attr(n->tx_array[get_index]->qb, PACKETBUF_ATTR_TSCH_TIMESLOT);

#if WITH_ALICE == 1 /* alice implementation */
        int packet_attr_channel_offset = queuebuf_attr(n->tx_array[get_index]->qb, PACKETBUF_ATTR_TSCH_CHANNEL_OFFSET);

#ifdef ALICE_F_PACKET_CELL_MATCHING_ON_THE_FLY
        if(packet_attr_slotframe == ALICE_UNICAST_SF_ID) {
          linkaddr_t rx_linkaddr;
          linkaddr_copy(&rx_linkaddr, queuebuf_addr(n->tx_array[get_index]->qb, PACKETBUF_ADDR_RECEIVER));
          uint16_t packet_timeslot = link->timeslot; //alice final check
          uint16_t packet_channel_offset = link->channel_offset; //alice final check


          //this function calculates time offset and channel offset 
          //on the basis of the link-level packet destiation (rx_linkaddr) and the current SFID.
          //Decides packet_timeslot and packet_channel_offset, checks rpl neighbor relations.
          int r = ALICE_F_PACKET_CELL_MATCHING_ON_THE_FLY(&packet_timeslot, &packet_channel_offset, rx_linkaddr);

#if ENABLE_ALICE_PACKET_CELL_MATCHING_LOG
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ALICE p-c-m nbr %d pts %u pch %u lts %u lch %u", 
                  r, packet_timeslot, packet_channel_offset,
                  link->timeslot, link->channel_offset));
#endif

          if(r == 0) { //no RPL neighbor --> ALICE EARLY PACKET DROP
		        tsch_queue_free_packet(n->tx_array[get_index]);
            return NULL;
          } else { //RPL neighbor
            if(packet_attr_slotframe != 0xffff && link->slotframe_handle != ALICE_UNICAST_SF_ID) {
              return NULL;
            }
            if(packet_attr_timeslot != 0xffff && packet_timeslot != link->timeslot) {
              return NULL;
            }
            if(packet_attr_channel_offset != 0xffff && packet_channel_offset != link->channel_offset) {
              return NULL;
            }
          }
          return n->tx_array[get_index];

        } else { //EB or broadcast slotframe's packet
          if(packet_attr_slotframe != 0xffff && packet_attr_slotframe != link->slotframe_handle) {
            return NULL;
          }
          if(packet_attr_timeslot != 0xffff && packet_attr_timeslot != link->timeslot) {
            return NULL;
          }
          if(packet_attr_channel_offset != link->channel_offset) { 
            return NULL; 
          }
        }
#else /* ALICE_F_PACKET_CELL_MATCHING_ON_THE_FLY */
        if(packet_attr_slotframe != 0xffff && packet_attr_slotframe != link->slotframe_handle) {
          return NULL;
        }
        if(packet_attr_timeslot != 0xffff && packet_attr_timeslot != link->timeslot) {
          return NULL;
        }
        //alice final check, packet_attr_channel_offset != 0xffff?
        if(packet_attr_channel_offset != 0xffff && packet_attr_channel_offset != link->channel_offset) {
          return NULL;
        }
#endif /* ALICE_F_PACKET_CELL_MATCHING_ON_THE_FLY */
#else /* WITH_ALICE */
        if(packet_attr_slotframe != 0xffff && packet_attr_slotframe != link->slotframe_handle) {
          return NULL;
        }
        if(packet_attr_timeslot != 0xffff && packet_attr_timeslot != link->timeslot) {
          return NULL;
        }
#endif /* WITH_ALICE */

#endif /* TSCH_WITH_LINK_SELECTOR */
        return n->tx_array[get_index];
      }
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Returns the head packet from a neighbor queue (from neighbor address) */
struct tsch_packet *
tsch_queue_get_packet_for_dest_addr(const linkaddr_t *addr, struct tsch_link *link)
{
  if(!tsch_is_locked()) {
    return tsch_queue_get_packet_for_nbr(tsch_queue_get_nbr(addr), link);
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Returns the head packet of any neighbor queue with zero backoff counter.
 * Writes pointer to the neighbor in *n */
struct tsch_packet *
tsch_queue_get_unicast_packet_for_any(struct tsch_neighbor **n, struct tsch_link *link)
{
  if(!tsch_is_locked()) {
    struct tsch_neighbor *curr_nbr = (struct tsch_neighbor *)nbr_table_head(tsch_neighbors);
    struct tsch_packet *p = NULL;
    while(curr_nbr != NULL) {
      if(!curr_nbr->is_broadcast && curr_nbr->tx_links_count == 0) {
        /* Only look up for non-broadcast neighbors we do not have a tx link to */
        p = tsch_queue_get_packet_for_nbr(curr_nbr, link);
        if(p != NULL) {
          if(n != NULL) {
            *n = curr_nbr;
          }
          return p;
        }
      }
      curr_nbr = (struct tsch_neighbor *)nbr_table_next(tsch_neighbors, curr_nbr);
    }

#if WITH_OST_CHECK && RESIDUAL_ALLOC //hckim??
    if(link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET && link->link_options == LINK_OPTION_TX) {
      uint16_t target_nbr_id = (link->slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
      printf("ERROR: No ssq Tx packet (nbr %u)\n", target_nbr_id); // This could be printed when Tx occurs by RB before reserved ssq Tx

      struct tsch_neighbor *target_nbr = tsch_queue_get_nbr_from_id(target_nbr_id);
      if(target_nbr == NULL) {
        printf("ERROR: c1\n");
      } else {
        printf("ERROR: c2 %u %u\n", !target_nbr->is_broadcast, target_nbr->tx_links_count == 0);
        //both should be 1
        int16_t get_index = ringbufindex_peek_get(&target_nbr->tx_ringbuf);
        int is_shared_link = link != NULL && link->link_options & LINK_OPTION_SHARED;
        printf("ERROR: c3 %u %u\n", get_index != -1, is_shared_link != 0);
        //both should be 1
      }
    }
  } else {
    if(link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET && link->link_options == LINK_OPTION_TX) {
      printf("ERROR: Locked\n");
    }
#endif
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* May the neighbor transmit over a shared link? */
int
tsch_queue_backoff_expired(const struct tsch_neighbor *n)
{
  return n->backoff_window == 0;
}
/*---------------------------------------------------------------------------*/
/* Reset neighbor backoff */
void
tsch_queue_backoff_reset(struct tsch_neighbor *n)
{
  n->backoff_window = 0;
  n->backoff_exponent = TSCH_MAC_MIN_BE;
}
/*---------------------------------------------------------------------------*/
/* Increment backoff exponent, pick a new window */
void
tsch_queue_backoff_inc(struct tsch_neighbor *n)
{
  /* Increment exponent */
  n->backoff_exponent = MIN(n->backoff_exponent + 1, TSCH_MAC_MAX_BE);
  /* Pick a window (number of shared slots to skip). Ignore least significant
   * few bits, which, on some embedded implementations of rand (e.g. msp430-libc),
   * are known to have poor pseudo-random properties. */
  n->backoff_window = (random_rand() >> 6) % (1 << n->backoff_exponent);
  /* Add one to the window as we will decrement it at the end of the current slot
   * through tsch_queue_update_all_backoff_windows */
  n->backoff_window++;
}
/*---------------------------------------------------------------------------*/
/* Decrement backoff window for all queues directed at dest_addr */
void
tsch_queue_update_all_backoff_windows(const linkaddr_t *dest_addr)
{
  if(!tsch_is_locked()) {
    int is_broadcast = linkaddr_cmp(dest_addr, &tsch_broadcast_address);
    struct tsch_neighbor *n = (struct tsch_neighbor *)nbr_table_head(tsch_neighbors);
    while(n != NULL) {
      if(n->backoff_window != 0 /* Is the queue in backoff state? */
         && ((n->tx_links_count == 0 && is_broadcast)
             || (n->tx_links_count > 0 && linkaddr_cmp(dest_addr, tsch_queue_get_nbr_address(n))))) {
        n->backoff_window--;
      }
      n = (struct tsch_neighbor *)nbr_table_next(tsch_neighbors, n);
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Initialize TSCH queue module */
void
tsch_queue_init(void)
{
  nbr_table_register(tsch_neighbors, NULL);
  memb_init(&packet_memb);
  /* Add virtual EB and the broadcast neighbors */
  n_eb = tsch_queue_add_nbr(&tsch_eb_address);
  n_broadcast = tsch_queue_add_nbr(&tsch_broadcast_address);

#if WITH_OST_01 /* checked */
  /* 
   * start select_N_timer, this timer will call select_N func periodically
   * select_N func calculates N for OST
  */
  ctimer_set(&select_N_timer, N_SELECTION_PERIOD * CLOCK_SECOND, select_N, NULL);
#endif
}
/*---------------------------------------------------------------------------*/
/** @} */
