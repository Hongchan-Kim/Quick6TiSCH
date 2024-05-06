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
 *         Log functions for TSCH, meant for logging from interrupt
 *         during a timeslot operation. Saves ASN, slot and link information
 *         and adds the log to a ringbuf for later printout.
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 *
 */

/**
 * \addtogroup tsch
 * @{
*/

#include "contiki.h"
#include <stdio.h>
#include "net/mac/tsch/tsch.h"
#include "lib/ringbufindex.h"
#include "sys/log.h"

#if WITH_OST
#include "net/mac/tsch/tsch-log.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "net/mac/tsch/tsch-slot-operation.h"
#include "lib/ringbufindex.h"
#include "orchestra.h"
#include "node-info.h"
#endif

#if TSCH_LOG_PER_SLOT

PROCESS_NAME(tsch_pending_events_process);

/* Check if TSCH_LOG_QUEUE_LEN is a power of two */
#if (TSCH_LOG_QUEUE_LEN & (TSCH_LOG_QUEUE_LEN - 1)) != 0
#error TSCH_LOG_QUEUE_LEN must be power of two
#endif
static struct ringbufindex log_ringbuf;
static struct tsch_log_t log_array[TSCH_LOG_QUEUE_LEN];
static int log_dropped = 0;
static int log_active = 0;

/*---------------------------------------------------------------------------*/
/* Process pending log messages */
void
tsch_log_process_pending(void)
{
  static int last_log_dropped = 0;
  int16_t log_index;
  /* Loop on accessing (without removing) a pending input packet */
  if(log_dropped != last_log_dropped) {
    printf("[WARN: TSCH-LOG  ] logs dropped %u\n", log_dropped);
    last_log_dropped = log_dropped;
  }
  while((log_index = ringbufindex_peek_get(&log_ringbuf)) != -1) {
    struct tsch_log_t *log = &log_array[log_index];
    if(log->link == NULL) {
      printf("[INFO: TSCH-LOG  ] {asn %02x.%08lx link-NULL} ", log->asn.ms1b, log->asn.ls4b);
    } else {
      struct tsch_slotframe *sf = tsch_schedule_get_slotframe_by_handle(log->link->slotframe_handle);
      printf("[INFO: TSCH-LOG  ] {asn %02x.%08lx link %2u %3u %3u %2u %2u ch %2u} ",
             log->asn.ms1b, log->asn.ls4b,
             log->link->slotframe_handle, sf ? sf->size.val : 0,
             log->burst_count, log->timeslot, log->channel_offset, // hckim
             log->channel);
    }
    switch(log->type) {
      case tsch_log_tx:
        printf("%s-%u-%u tx ",
                linkaddr_cmp(&log->tx.dest, &linkaddr_null) ? "bc" : "uc", log->tx.is_data, log->tx.sec_level);
        log_lladdr_compact(&linkaddr_node_addr);
        printf("->");
        log_lladdr_compact(&log->tx.dest);
        printf(", len %3u, seq %3u, st %d %2d",
                log->tx.datalen, log->tx.seqno, log->tx.mac_tx_status, log->tx.num_tx);
        if(log->tx.drift_used) {
          printf(", dr %3d", log->tx.drift);
        }

#if HCK_LOG_TSCH_SLOT
        printf(", LOG T");
#endif
#if HCK_LOG_TSCH_SLOT_APP_SEQNO
        if(log->tx.app_magic == APP_DATA_MAGIC) {
          printf(" a_seq %lx", log->tx.app_seqno);
        }
#endif
#if HCK_FORMATION_PACKET_TYPE_INFO
        printf(" PT %u %lu %u %u %u %u", 
              log->tx.hck_packet_type,
              log->asn.ls4b,
              log->link->slotframe_handle,
              linkaddr_cmp(&log->tx.dest, &linkaddr_null) ? 0 : 1,
              log->tx.mac_tx_status,
              log->tx.num_tx);
#endif
#if HCK_FORMATION_BOOTSTRAP_STATE_INFO
        printf(" BS %u", log->tx.hck_bootstrap_state);
#endif
#if QUICK_LOG
        printf(" HN %u %u %u %u %u %u %u %u %u %u",
              log->tx.quick_collision_count,
              log->tx.quick_cssf_postponed_count,
              log->tx.quick_noack_count,
              log->tx.quick_backoff_exponent_before,
              log->tx.quick_backoff_window_before,
              log->tx.quick_backoff_exponent_after,
              log->tx.quick_backoff_window_after,
              log->tx.quick_state,
              log->tx.quick_state_based_offset,
              log->tx.quick_escalated_offset);
#endif
#if QUICK_SLOTFRAME_LEVEL_BACKOFF
      printf(" %u %u %u %u", 
              log->tx.quick_cssf_backoff_exponent_before,
              log->tx.quick_cssf_backoff_window_before,
              log->tx.quick_cssf_backoff_exponent_after,
              log->tx.quick_cssf_backoff_window_after);
#endif
#if HCK_LOG_TSCH_SLOT
        printf(" HK-T");
#endif
        printf("\n");
#if WITH_OST
        /* unicast packets only */
        if(!linkaddr_cmp(&log->tx.dest, &linkaddr_null)
          && !linkaddr_cmp(&log->tx.dest, &tsch_eb_address)
          && !linkaddr_cmp(&log->tx.dest, &tsch_broadcast_address)) {
          /* periodic provisioning tx slotframe only */
          if(log->link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET
            && log->link->slotframe_handle <= SSQ_SCHEDULE_HANDLE_OFFSET) {
            
            uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)&log->tx.dest);
            if(nbr != NULL) {
              nbr->ost_num_tx_mac++;
              if(log->tx.mac_tx_status == MAC_TX_OK) {
                nbr->ost_num_tx_succ_mac++;
                nbr->ost_num_consecutive_tx_fail_mac = 0;
              } else {
                nbr->ost_num_consecutive_tx_fail_mac++;
              }
              
              uint16_t prr = 100 * nbr->ost_num_tx_succ_mac / nbr->ost_num_tx_mac;
              if(prr <= PRR_THRES_TX_CHANGE && nbr->ost_num_tx_mac >= NUM_TX_MAC_THRES_TX_CHANGE) {
                nbr->ost_my_low_prr = 1;
                ost_update_N_of_packets_in_queue(&log->tx.dest, nbr->ost_my_N + OST_N_OFFSET_NEW_TX_REQUEST);
              }

              if(nbr->ost_num_consecutive_tx_fail_mac >= NUM_TX_FAIL_THRES) {
                struct tsch_neighbor *n = tsch_queue_get_nbr(&log->tx.dest);
                if(n != NULL) {
                  if(!tsch_queue_is_empty(n)) {
                    if(neighbor_has_uc_link(tsch_queue_get_nbr_address(n))) {
                      /* use autonomous RB slotframe */
                      ost_change_queue_select_packet(&log->tx.dest, 1, 
                                          ORCHESTRA_LINKADDR_HASH(&log->tx.dest) % ORCHESTRA_UNICAST_PERIOD);
                    } else {
                      /* use shared slotframe */
                      ost_change_queue_select_packet(&log->tx.dest, 2, 0);
                    }
                  }
                }
              }
            }
          }
        }
#endif
        break;
      case tsch_log_rx:
        printf("%s-%u-%u rx ",
                log->rx.is_unicast == 0 ? "bc" : "uc", log->rx.is_data, log->rx.sec_level);
        log_lladdr_compact(&log->rx.src);
        printf("->");
        log_lladdr_compact(log->rx.is_unicast ? &linkaddr_node_addr : NULL);
        printf(", len %3u, seq %3u",
                log->rx.datalen, log->rx.seqno);
        printf(", edr %3d", (int)log->rx.estimated_drift);
        if(log->rx.drift_used) {
          printf(", dr %3d", log->rx.drift);
        }
        printf(", rssi %3d", log->rx.rssi);

#if HCK_LOG_TSCH_SLOT
        printf(", LOG R");
#endif
#if HCK_LOG_TSCH_SLOT_APP_SEQNO
        if(log->rx.app_magic == APP_DATA_MAGIC) {
          printf(" a_seq %lx", log->rx.app_seqno);
        }
#endif
#if HCK_FORMATION_PACKET_TYPE_INFO
        printf(" PT %u %lu %u %u", 
              log->rx.hck_packet_type, 
              log->asn.ls4b, 
              log->link->slotframe_handle,
              log->rx.is_unicast == 0 ? 0 : 1);
#endif
#if HCK_FORMATION_BOOTSTRAP_STATE_INFO
        printf(" BS %u", log->rx.hck_bootstrap_state);
#endif
#if QUICK_LOG
        printf(" HN %u", log->rx.quick_offset);
#endif
#if HCK_LOG_TSCH_SLOT
        printf(" HK-T");
#endif
        printf("\n");
        break;
      case tsch_log_message:
        printf("%s\n", log->message);
        break;
    }
    /* Remove input from ringbuf */
    ringbufindex_get(&log_ringbuf);
  }
}
/*---------------------------------------------------------------------------*/
/* Prepare addition of a new log.
 * Returns pointer to log structure if success, NULL otherwise */
struct tsch_log_t *
tsch_log_prepare_add(void)
{
  int log_index = ringbufindex_peek_put(&log_ringbuf);
  if(log_index != -1) {
    struct tsch_log_t *log = &log_array[log_index];
    log->asn = tsch_current_asn;
    log->link = current_link;
    log->burst_count = tsch_current_burst_count;
    log->channel = tsch_current_channel;
    log->channel_offset = tsch_current_channel_offset;
    log->timeslot = tsch_current_timeslot; // hckim
    return log;
  } else {
    log_dropped++;
    return NULL;
  }
}
/*---------------------------------------------------------------------------*/
/* Actually add the previously prepared log */
void
tsch_log_commit(void)
{
  if(log_active == 1) {
    ringbufindex_put(&log_ringbuf);
    process_poll(&tsch_pending_events_process);
  }
}
/*---------------------------------------------------------------------------*/
/* Initialize log module */
void
tsch_log_init(void)
{
  if(log_active == 0) {
    ringbufindex_init(&log_ringbuf, TSCH_LOG_QUEUE_LEN);
    log_active = 1;
  }
}
/*---------------------------------------------------------------------------*/
/* Stop log module */
void
tsch_log_stop(void)
{
  if(log_active == 1) {
    tsch_log_process_pending();
    log_active = 0;
  }
}

#endif /* TSCH_LOG_PER_SLOT */
/** @} */
