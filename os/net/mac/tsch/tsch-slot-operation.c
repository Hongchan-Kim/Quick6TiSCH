/*
 * Copyright (c) 2015, SICS Swedish ICT.
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
 *         TSCH slot operation implementation, running from interrupt.
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 *         Beshr Al Nahas <beshr@sics.se>
 *         Atis Elsts <atis.elsts@bristol.ac.uk>
 *
 */

/**
 * \addtogroup tsch
 * @{
*/

#include "dev/radio.h"
#include "contiki.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/mac/framer/framer-802154.h"
#include "net/mac/tsch/tsch.h"
#include "sys/critical.h"

#if WITH_OST
#include "node-info.h"
#include "orchestra.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "lib/random.h"
#if OST_ON_DEMAND_PROVISION
#include "net/mac/framer/frame802154.h"
#endif
#endif

#include "sys/log.h"
/* TSCH debug macros, i.e. to set LEDs or GPIOs on various TSCH
 * timeslot events */
#ifndef TSCH_DEBUG_INIT
#define TSCH_DEBUG_INIT()
#endif
#ifndef TSCH_DEBUG_INTERRUPT
#define TSCH_DEBUG_INTERRUPT()
#endif
#ifndef TSCH_DEBUG_RX_EVENT
#define TSCH_DEBUG_RX_EVENT()
#endif
#ifndef TSCH_DEBUG_TX_EVENT
#define TSCH_DEBUG_TX_EVENT()
#endif
#ifndef TSCH_DEBUG_SLOT_START
#define TSCH_DEBUG_SLOT_START()
#endif
#ifndef TSCH_DEBUG_SLOT_END
#define TSCH_DEBUG_SLOT_END()
#endif

/* Check if TSCH_MAX_INCOMING_PACKETS is power of two */
#if (TSCH_MAX_INCOMING_PACKETS & (TSCH_MAX_INCOMING_PACKETS - 1)) != 0
#error TSCH_MAX_INCOMING_PACKETS must be power of two
#endif

/* Check if TSCH_DEQUEUED_ARRAY_SIZE is power of two and greater or equal to QUEUEBUF_NUM */
#if TSCH_DEQUEUED_ARRAY_SIZE < QUEUEBUF_NUM
#error TSCH_DEQUEUED_ARRAY_SIZE must be greater or equal to QUEUEBUF_NUM
#endif
#if (TSCH_DEQUEUED_ARRAY_SIZE & (TSCH_DEQUEUED_ARRAY_SIZE - 1)) != 0
#error TSCH_DEQUEUED_ARRAY_SIZE must be power of two
#endif

/* Truncate received drift correction information to maximum half
 * of the guard time (one fourth of TSCH_DEFAULT_TS_RX_WAIT) */
#define SYNC_IE_BOUND ((int32_t)US_TO_RTIMERTICKS(tsch_timing_us[tsch_ts_rx_wait] / 4))

/* By default: check that rtimer runs at >=32kHz and use a guard time of 10us */
#if RTIMER_SECOND < (32 * 1024)
#error "TSCH: RTIMER_SECOND < (32 * 1024)"
#endif
#if CONTIKI_TARGET_COOJA
/* Use 0 usec guard time for Cooja Mote with a 1 MHz Rtimer*/
#define RTIMER_GUARD 0u
#elif RTIMER_SECOND >= 200000
#define RTIMER_GUARD (RTIMER_SECOND / 100000)
#else
#define RTIMER_GUARD 2u
#endif

enum tsch_radio_state_on_cmd {
  TSCH_RADIO_CMD_ON_START_OF_TIMESLOT,
  TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT,
  TSCH_RADIO_CMD_ON_FORCE,
};

enum tsch_radio_state_off_cmd {
  TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT,
  TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT,
  TSCH_RADIO_CMD_OFF_FORCE,
};

/* A ringbuf storing outgoing packets after they were dequeued.
 * Will be processed layer by tsch_tx_process_pending */
struct ringbufindex dequeued_ringbuf;
struct tsch_packet *dequeued_array[TSCH_DEQUEUED_ARRAY_SIZE];
/* A ringbuf storing incoming packets.
 * Will be processed layer by tsch_rx_process_pending */
struct ringbufindex input_ringbuf;
struct input_packet input_array[TSCH_MAX_INCOMING_PACKETS];

/* Updates and reads of the next two variables must be atomic (i.e. both together) */
/* Last time we received Sync-IE (ACK or data packet from a time source) */
static struct tsch_asn_t last_sync_asn;
clock_time_t tsch_last_sync_time; /* Same info, in clock_time_t units */

/* A global lock for manipulating data structures safely from outside of interrupt */
static volatile int tsch_locked = 0;
/* As long as this is set, skip all slot operation */
static volatile int tsch_lock_requested = 0;

/* Last estimated drift in RTIMER ticks
 * (Sky: 1 tick = 30.517578125 usec exactly) */
static int32_t drift_correction = 0;
/* Is drift correction used? (Can be true even if drift_correction == 0) */
static uint8_t is_drift_correction_used;

/* Used from tsch_slot_operation and sub-protothreads */
static rtimer_clock_t volatile current_slot_start;

/* Are we currently inside a slot? */
static volatile int tsch_in_slot_operation = 0;

/* If we are inside a slot, these tell the current channel and channel offset */
uint8_t tsch_current_channel;
uint8_t tsch_current_channel_offset;
uint16_t tsch_current_timeslot;

/* Info about the link, packet and neighbor of
 * the current (or next) slot */
struct tsch_link *current_link = NULL;
/* A backup link with Rx flag, overlapping with current_link.
 * If the current link is Tx-only and the Tx queue
 * is empty while executing the link, fallback to the backup link. */
static struct tsch_link *backup_link = NULL;
static struct tsch_packet *current_packet = NULL;
static struct tsch_neighbor *current_neighbor = NULL;

#if !WITH_OST && !WITH_ALICE
/* Indicates whether an extra link is needed to handle the current burst */
static int burst_link_scheduled = 0;
#endif
/* Counts the length of the current burst */
int tsch_current_burst_count = 0;

/* Protothread for association */
PT_THREAD(tsch_scan(struct pt *pt));
/* Protothread for slot operation, called from rtimer interrupt
 * and scheduled from tsch_schedule_slot_operation */
static PT_THREAD(tsch_slot_operation(struct rtimer *t, void *ptr));
static struct pt slot_operation_pt;
/* Sub-protothreads of tsch_slot_operation */
static PT_THREAD(tsch_tx_slot(struct pt *pt, struct rtimer *t));
static PT_THREAD(tsch_rx_slot(struct pt *pt, struct rtimer *t));

#if WITH_OST
uint8_t ost_flag_failed_to_select_t_offset;
uint8_t ost_todo_consecutive_new_tx_request;

static uint8_t todo_rx_schedule_change;
static uint8_t todo_tx_schedule_change;

static uip_ds6_nbr_t *prN_nbr;
static uint16_t prN_new_N;
static uint16_t prN_new_t_offset;

typedef struct ost_t_offset_candidate {
  uint8_t available;
} ost_t_offset_candidate_t;

static uint8_t todo_N_update;
static uint8_t todo_N_inc;

static uip_ds6_nbr_t *prt_nbr;
static uint16_t prt_new_t_offset;
static uint8_t changed_t_offset_default;

uint16_t
hash_ftn(uint16_t value, uint16_t mod) {
  uint32_t input = (uint32_t)value;
  uint32_t a = input;
  
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);

  return (uint16_t)a % mod;
}
#if OST_ON_DEMAND_PROVISION
struct ssq_schedule_t ssq_schedule_list[16];
/*---------------------------------------------------------------------------*/
uint16_t
select_matching_schedule(uint16_t rx_schedule_info)
{
  uint16_t my_schedule = tsch_schedule_get_subsequent_schedule(&tsch_current_asn);
  uint16_t compare_schedule = my_schedule | rx_schedule_info;

  uint8_t i;
  for(i = 0; i < 16; i++) {
    if((compare_schedule >> i) % 2 == 0) { /* Find the earliest 0 */
      return i + 1;
    }
  }
  return 0xffff; /* 0xffff */
}
/*---------------------------------------------------------------------------*/
void
print_ssq_schedule_list(void)
{
/*
  PRINTF("[SSQ_SCHEDULE] index / asn / option\n");
  uint8_t i;
  uint16_t nbr_id;
  for(i = 0; i < 16; i++) {
    if(ssq_schedule_list[i].asn.ls4b == 0 && ssq_schedule_list[i].asn.ms1b == 0) {
    } else {
      if(ssq_schedule_list[i].link.link_options == LINK_OPTION_TX) {
        nbr_id = (ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
        //PRINTF("[ID:%u] %u / %x.%lx / Tx %u\n", nbr_id, i, ssq_schedule_list[i].asn.ms1b, ssq_schedule_list[i].asn.ls4b,ssq_schedule_list[i].link.slotframe_handle);
      } else {
        nbr_id = (ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 2) / 2;
        //PRINTF("[ID:%u] %u / %x.%lx / Rx %u\n", nbr_id, i, ssq_schedule_list[i].asn.ms1b, ssq_schedule_list[i].asn.ls4b, ssq_schedule_list[i].link.slotframe_handle);
      }
    }
  }
*/
}
/*---------------------------------------------------------------------------*/
uint8_t
exist_matching_slot(struct tsch_asn_t *target_asn)
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ssq_schedule_list[i].asn.ls4b == target_asn->ls4b 
      && ssq_schedule_list[i].asn.ms1b == target_asn->ms1b) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
remove_matching_slot(void)
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ssq_schedule_list[i].asn.ls4b == tsch_current_asn.ls4b 
      && ssq_schedule_list[i].asn.ms1b == tsch_current_asn.ms1b) {
      ssq_schedule_list[i].asn.ls4b = 0;
      ssq_schedule_list[i].asn.ms1b = 0;
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
add_matching_slot(uint16_t matching_slot, uint8_t is_tx, uint16_t nbr_id)
{  
  if(1 <= matching_slot && matching_slot <= 16) {
    uint8_t i;
    for(i = 0; i < 16; i++) {
      if(ssq_schedule_list[i].asn.ls4b == 0 && ssq_schedule_list[i].asn.ms1b == 0) {
        ssq_schedule_list[i].asn.ls4b = tsch_current_asn.ls4b;
        ssq_schedule_list[i].asn.ms1b = tsch_current_asn.ms1b;
        TSCH_ASN_INC(ssq_schedule_list[i].asn, matching_slot);

        ssq_schedule_list[i].link.next = NULL;        //not used
        ssq_schedule_list[i].link.handle = 0xffff;    //not used
        ssq_schedule_list[i].link.timeslot = 0xffff;  //not used

        ssq_schedule_list[i].link.channel_offset = 3; //will be updated using hash before TX or RX

        ssq_schedule_list[i].link.link_type = LINK_TYPE_NORMAL;
        ssq_schedule_list[i].link.data = NULL;

        linkaddr_copy(&(ssq_schedule_list[i].link.addr), &tsch_broadcast_address);

        if(is_tx) {
          ssq_schedule_list[i].link.slotframe_handle = SSQ_SCHEDULE_HANDLE_OFFSET + 2 * nbr_id + 1;
          ssq_schedule_list[i].link.link_options = LINK_OPTION_TX;

#if WITH_OST_LOG
          TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "ost odp: add tx %u", ssq_schedule_list[i].link.slotframe_handle);
        );
#endif

        } else {
          ssq_schedule_list[i].link.slotframe_handle = SSQ_SCHEDULE_HANDLE_OFFSET + 2 * nbr_id + 2;
          ssq_schedule_list[i].link.link_options = LINK_OPTION_RX;

#if WITH_OST_LOG
          TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "ost odp: add rx %u", ssq_schedule_list[i].link.slotframe_handle);
        );
#endif

        }

        break;        
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
uint16_t 
process_rx_schedule_info(frame802154_t* frame) 
{
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)(&(frame->src_addr)));
#if !WITH_OST_OID
  uint16_t nbr_id = ost_node_id_from_linkaddr((linkaddr_t *)(&(frame->src_addr))); // chk
#else
  uint16_t nbr_id = nbr->ds6_ost_id;
#endif
  if(nbr != NULL
    && !frame802154_is_broadcast_addr((frame->fcf).dest_addr_mode, frame->dest_addr)
    && ost_is_routing_nbr(nbr) == 1) {
    if((frame->fcf).frame_pending) {
      uint16_t time_to_matching_slot = select_matching_schedule(frame->ost_pigg2);

      if(time_to_matching_slot == 0xffff) { /* 0xffff */
        /* no matching slots */
      } else if(1 <= time_to_matching_slot && (time_to_matching_slot - 1) < 16) {
        /* matching slot at time_to_matching slot */
        add_matching_slot(time_to_matching_slot, 0, nbr_id);
        return time_to_matching_slot;
      } else {
        /* ERROR: time_to_matching_slot */
      }
    } else if(!((frame->fcf).frame_pending) && frame->ost_pigg2 != 0xffff) {
      /* ERROR: schedule info is updated only when pending bit is set */
    }
  }
  return 0xffff;
}
/*---------------------------------------------------------------------------*/
void
process_rx_matching_slot(frame802154_t* frame)
{
  linkaddr_t *eack_src = tsch_queue_get_nbr_address(current_neighbor);
#if !WITH_OST_OID
  uint16_t src_id = ost_node_id_from_linkaddr(eack_src); // chk
#else
  uint16_t src_id = current_neighbor->tsch_ost_id;
#endif
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)eack_src);
  if(nbr != NULL && ost_is_routing_nbr(nbr) == 1) {
    add_matching_slot(frame->ost_pigg2, 1, src_id);
  }
}
#endif /* OST_ON_DEMAND_PROVISION */
/*---------------------------------------------------------------------------*/
/* Rx N from Data -> Change Rx schedule */
void
ost_add_rx(uint16_t id, uint16_t N, uint16_t t_offset)
{
  if(N != 0xffff && t_offset != 0xffff) {
    uint16_t handle = ost_get_rx_sf_handle_from_id(id);
    uint16_t size = (1 << N);
    uint16_t channel_offset = 3;

    if(tsch_schedule_get_slotframe_by_handle(handle) != NULL) {
      return;
    }

    struct tsch_slotframe *sf;
    sf = tsch_schedule_add_slotframe(handle, size);

    if(sf != NULL) {
      tsch_schedule_add_link(sf, LINK_OPTION_RX, LINK_TYPE_NORMAL, 
                            &tsch_broadcast_address, t_offset, channel_offset, 1);
    }
  }
}
/*---------------------------------------------------------------------------*/
void
ost_remove_rx(uint16_t id)
{
  struct tsch_slotframe *rm_sf;
  uint16_t rm_sf_handle = ost_get_rx_sf_handle_from_id(id);
  rm_sf = tsch_schedule_get_slotframe_by_handle(rm_sf_handle);

  if(rm_sf != NULL) {
    tsch_schedule_remove_slotframe(rm_sf);
  }
}
/*---------------------------------------------------------------------------*/
void
eliminate_overlap_toc(ost_t_offset_candidate_t *toc, uint16_t target_N, uint16_t used_N, uint16_t used_t_offset)
{
  if(target_N < used_N) { /* lower-tier used */
    uint16_t parent_N = used_N - 1;
    uint16_t parent_t_offset = used_t_offset % (1 << (used_N - 1)); 

    eliminate_overlap_toc(toc, target_N, parent_N, parent_t_offset);

  } else if (target_N > used_N) { /* higher-tier used */
    uint16_t child_N = used_N + 1;
    uint16_t left_child_t_offset = used_t_offset;
    uint16_t right_child_t_offset = used_t_offset + (1 << used_N);

    eliminate_overlap_toc(toc, target_N, child_N, left_child_t_offset);
    eliminate_overlap_toc(toc, target_N, child_N, right_child_t_offset);

  } else { /* target_N == used_N */
    if(used_t_offset >= (1 << target_N)) {
      /* ERR: too big used t_offset */
      return;
    }
    toc[used_t_offset].available = 0;
  }
}
/*---------------------------------------------------------------------------*/
uint32_t 
select_t_offset(uint16_t target_id, uint16_t N)  /* similar with tx_installable */
{
  ost_t_offset_candidate_t toc[1 << N_MAX];

  /* initialize 2^N toc */
  uint16_t i;
  for(i = 0; i < (1 << N); i++) {
    toc[i].available = 1;
  }

  /* Check resource overlap */
  struct tsch_slotframe *sf = ost_tsch_schedule_get_slotframe_head();
  while(sf != NULL) {
    if(sf->handle > 2 && sf->handle != ost_get_rx_sf_handle_from_id(target_id)) {
      uint16_t n;
      for(n = 1; n <= N_MAX; n++) {
        if((sf->size.val >> n) == 1) {
          uint16_t used_N = n;
          struct tsch_link *l = list_head(sf->links_list);

          if(l == NULL) {
            /* ERR: link is null */
          } else {
            uint16_t used_t_offset = l->timeslot;
            if(used_t_offset >= (1 << used_N)) {
              /* ERR: too big used t_offsete */
            }
            eliminate_overlap_toc(toc, N, used_N, used_t_offset);
          }

          break;

        } else if (n == N_MAX) {
          /* ERR: weird size of slotframe */
        }
      }
    }
    sf = list_item_next(sf);
  }

  uint32_t rand = random_rand() % (1 << 31); /* OST check later */
  uint32_t j = 0;

  for(i = 0; i < (1 << N); i++) {
    j = (i + rand) % (1 << N);
    if(toc[j].available == 1) {
      break;
    }
  }

  if(i == (1 << N)) {
#if WITH_OST_LOG
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost select_t_offset: (%u, %u) -> failed", target_id, N);
  );
#endif
    return 0xffff + 1;
  } else {
#if WITH_OST_LOG
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost select_t_offset: (%u, %u) -> %lu", target_id, N, j);
  );
#endif
    return j;
  }
}
/*---------------------------------------------------------------------------*/
 /* in short, prN */
void
process_rx_N(frame802154_t *frame)
{
  if(tsch_is_locked()) {
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "process_rx_N: locked");
    );
    return;
  }

  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)(&(frame->src_addr)));
  if(nbr != NULL
    && !frame802154_is_broadcast_addr((frame->fcf).dest_addr_mode, frame->dest_addr)
    && ost_is_routing_nbr(nbr) == 1
    && nbr->ost_rx_no_path == 0) {
    /* No need to allocate rx for nbr who sent no-path dao */
#if !WITH_OST_OID
    uint16_t nbr_id = ost_node_id_from_linkaddr((linkaddr_t *)(&(frame->src_addr))); // chk
#else
    uint16_t nbr_id = nbr->ds6_ost_id;
#endif

#if WITH_OST_LOG
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost rcvd N: nbr %u, %u -> %u", nbr_id, nbr->ost_nbr_N, frame->ost_pigg1);
    );
#endif

    if(nbr->ost_nbr_N != frame->ost_pigg1) {
      /* To be used in post_process_rx_N */
      if(frame->ost_pigg1 >= INC_N_NEW_TX_REQUEST) {
        prN_new_N = (frame->ost_pigg1) - INC_N_NEW_TX_REQUEST;
        nbr->ost_consecutive_new_tx_request++;

        if(nbr->ost_consecutive_new_tx_request >= THRES_CONSECUTIVE_NEW_TX_REQUEST) {
          nbr->ost_consecutive_new_tx_request = 0;
          ost_todo_consecutive_new_tx_request = 1;
          return;
        }

#if WITH_OST_LOG
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prN: uninstallable/low PRR (%u, %u)", 
                    nbr->ost_consecutive_new_tx_request, ost_todo_consecutive_new_tx_request);
        );
#endif

      } else {
        prN_new_N = frame->ost_pigg1;
        nbr->ost_consecutive_new_tx_request = 0;
      }

      uint32_t result = select_t_offset(nbr_id, prN_new_N);

      if(result <= 0xffff) {
        todo_rx_schedule_change = 1;
        prN_new_t_offset = (uint16_t)result;
        prN_nbr = nbr;
        nbr->ost_nbr_N = prN_new_N; 
        nbr->ost_nbr_t_offset = prN_new_t_offset;        
        return;  
      } else {
        ost_flag_failed_to_select_t_offset = 1;
        return;
      }
    } else {
      nbr->ost_consecutive_new_tx_request = 0;
    }
  }
}
/*---------------------------------------------------------------------------*/
/* called from tsch_rx_process_pending (after slot_operation finished) */
void
post_process_rx_N(void)
{
  if(todo_rx_schedule_change == 1) {
    todo_rx_schedule_change = 0;

    if(prN_nbr != NULL) {
#if !WITH_OST_OID
      uint16_t nbr_id = ost_node_id_from_ipaddr(&(prN_nbr->ipaddr)); // chk
#else
      uint16_t nbr_id = prN_nbr->ds6_ost_id;
#endif
      ost_remove_rx(nbr_id);
      ost_add_rx(nbr_id, prN_new_N, prN_new_t_offset);

#if WITH_OST_LOG
      TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ost post_prN: nbr %u, (%u, %u)", nbr_id, prN_new_N, prN_new_t_offset);
      );
#endif
    }
  }

  if(ost_todo_consecutive_new_tx_request == 1) {
    ost_todo_consecutive_new_tx_request = 0;
  }

  if(ost_flag_failed_to_select_t_offset == 1) {
    ost_flag_failed_to_select_t_offset = 0;
  }
}
/*---------------------------------------------------------------------------*/
uint8_t ost_get_ost_flag_failed_to_select_t_offset()
{
  return ost_flag_failed_to_select_t_offset;
}
/*---------------------------------------------------------------------------*/
uint8_t ost_get_todo_consecutive_new_tx_request()
{
  return ost_todo_consecutive_new_tx_request;
}
/*---------------------------------------------------------------------------*/
/* Rx t_offset from EACK -> Change Tx schedule */
void ost_change_queue_select_packet(linkaddr_t *nbr_lladdr, uint16_t handle, uint16_t timeslot)
{
  struct tsch_neighbor *n = tsch_queue_get_nbr(nbr_lladdr);

  if(!tsch_is_locked() && n !=NULL) {
    if(!ringbufindex_empty(&n->tx_ringbuf)) {
      int16_t get_index = ringbufindex_peek_get(&n->tx_ringbuf);
      uint8_t num_elements = ringbufindex_elements(&n->tx_ringbuf);

      uint8_t j;
      for(j = get_index; j < get_index + num_elements; j++) {
        int16_t index;

        if(j >= ringbufindex_size(&n->tx_ringbuf)) {
          index = j - ringbufindex_size(&n->tx_ringbuf);
        } else {
          index = j;
        }

        set_queuebuf_attr(n->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_SLOTFRAME, handle);
        set_queuebuf_attr(n->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void 
ost_add_tx(linkaddr_t *nbr_lladdr, uint16_t N, uint16_t t_offset)
{
  if(N != 0xffff && t_offset != 0xffff) {
#if !WITH_OST_OID
    uint16_t id = ost_node_id_from_linkaddr(nbr_lladdr); // chk
#else
    uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)nbr_lladdr);
    uint16_t id = nbr->ds6_ost_id;
#endif
    uint16_t handle = ost_get_tx_sf_handle_from_id(id);
    uint16_t size = (1 << N);
    uint16_t channel_offset = 3;

    struct tsch_slotframe *sf;
    struct tsch_link *l;

    if(tsch_schedule_get_slotframe_by_handle(handle) != NULL) {
      return;
    }

    sf = tsch_schedule_add_slotframe(handle, size);

    if(sf != NULL) {
      l = tsch_schedule_add_link(sf, LINK_OPTION_TX, LINK_TYPE_NORMAL, 
                                &tsch_broadcast_address, t_offset, channel_offset, 1);
      if(l != NULL) {
        ost_change_queue_select_packet(nbr_lladdr, handle, t_offset);
      }
#if !WITH_OST_OID
      uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)nbr_lladdr);
#endif
      if(nbr != NULL) {
        if(nbr->ost_my_low_prr == 1) {
          ost_change_queue_N_update(nbr_lladdr, nbr->ost_my_N);
        }

        nbr->ost_my_low_prr = 0;
        nbr->ost_num_tx_mac = 0;
        nbr->ost_num_tx_succ_mac = 0;
        nbr->ost_num_consecutive_tx_fail_mac = 0;
        nbr->ost_consecutive_my_N_inc = 0;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void 
ost_remove_tx(linkaddr_t *nbr_lladdr)
{
#if !WITH_OST_OID
  uint16_t id = ost_node_id_from_linkaddr(nbr_lladdr); / chk
#else
  struct tsch_neighbor *n = tsch_queue_get_nbr(nbr_lladdr);
  uint16_t id = n->tsch_ost_id;
#endif
  struct tsch_slotframe *rm_sf;
  uint16_t rm_sf_handle = ost_get_tx_sf_handle_from_id(id);
  rm_sf = tsch_schedule_get_slotframe_by_handle(rm_sf_handle);

  if(rm_sf != NULL) {
    tsch_schedule_remove_slotframe(rm_sf);
#if !WITH_OST_OID
    struct tsch_neighbor *n = tsch_queue_get_nbr(nbr_lladdr);
#endif
    if(n != NULL) {
      if(!tsch_queue_is_empty(n)) {
        if(neighbor_has_uc_link(tsch_queue_get_nbr_address(n))) {
          ost_change_queue_select_packet(nbr_lladdr, 1, id % ORCHESTRA_CONF_UNICAST_PERIOD); //Use RB
        } else {
          ost_change_queue_select_packet(nbr_lladdr, 2, 0); //Use shared slot
        }
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
has_my_N_changed(uip_ds6_nbr_t * nbr)
{
  /* check whether to match nbr->my_N and installed tx_sf */
#if !WITH_OST_OID
  uint16_t nbr_id = ost_node_id_from_ipaddr(&(nbr->ipaddr)); //chk
#else
  uint16_t nbr_id = nbr->ds6_ost_id;
#endif
  uint16_t tx_sf_handle = ost_get_tx_sf_handle_from_id(nbr_id);
  struct tsch_slotframe *tx_sf = tsch_schedule_get_slotframe_by_handle(tx_sf_handle);
  uint16_t tx_sf_size;

  if(tx_sf != NULL) {
    tx_sf_size = tx_sf->size.val;

    if(tx_sf_size != 1 << nbr->ost_my_N) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
int8_t
tx_installable(uint16_t target_id, uint16_t N, uint16_t t_offset)
{
  /* similar with select_t_offset */
  if(tsch_is_locked()) {
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "tx_installable: locked");
    );
    return -1;
  }

  ost_t_offset_candidate_t toc[1 << N_MAX];

  if(t_offset == 0xffff) {
    return -2;
  }

  if(t_offset < (1 << N) ) {
    toc[t_offset].available = 1;
  } else {
    return -1;
  }

  /* check resource overlap */
  struct tsch_slotframe *sf = ost_tsch_schedule_get_slotframe_head();
  while(sf != NULL) {
    if(sf->handle > 2 && sf->handle != ost_get_tx_sf_handle_from_id(target_id)) {
      uint16_t n;
      for(n = 1; n <= N_MAX; n++) {
        if((sf->size.val >> n) == 1) {
          uint16_t used_N = n;

          struct tsch_link *l = list_head(sf->links_list);
          if(l == NULL) {
            /* ERROR: link is null (tx_installable) */
          } else {
            uint16_t used_t_offset = l->timeslot;
            eliminate_overlap_toc(toc, N, used_N, used_t_offset);
          }

          break;
        } else if (n == N_MAX) {
          /* ERROR: weird size of slotframe */
        }
      }
    }
    sf = list_item_next(sf);
  }
  if(toc[t_offset].available == 1) {
    return 1;
  } else {
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
/* In short, prt */
void
process_rx_t_offset(frame802154_t* frame)
{
  linkaddr_t *eack_src = tsch_queue_get_nbr_address(current_neighbor);
#if !WITH_OST_OID
  uint16_t nbr_id = ost_node_id_from_linkaddr(eack_src); // chk
#else
  uint16_t nbr_id = current_neighbor->tsch_ost_id;
#endif
  
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)eack_src);
  if(nbr != NULL && ost_is_routing_nbr(nbr) == 1) {

    if(nbr->ost_my_t_offset != frame->ost_pigg1 || has_my_N_changed(nbr)) {
      /* To be used in post_process_rx_t_offset */
      prt_new_t_offset = frame->ost_pigg1;
      prt_nbr = nbr;

      if(frame->ost_pigg1 == T_OFFSET_ALLOCATION_FAIL) {
        todo_N_inc = 1;

#if WITH_OST_LOG
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: t_offset alloc fail");
        );
#endif

        return;
      }

      if(frame->ost_pigg1 == T_OFFSET_CONSECUTIVE_NEW_TX_REQUEST) {
        todo_N_inc = 1; 

#if WITH_OST_LOG
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: consecutive request");
        );
#endif

        return;          
      }

      nbr->ost_my_t_offset = prt_new_t_offset;

      /* 1: installable, -1: non-installable */
      int8_t result = tx_installable(nbr_id, nbr->ost_my_N, nbr->ost_my_t_offset);

      if(result == 1) { /* installable */
        if(nbr->ost_my_uninstallable != 0) {
          nbr->ost_my_uninstallable = 0;
          todo_N_update = 1;
        }
        changed_t_offset_default = 0;
        todo_tx_schedule_change = 1;

#if WITH_OST_LOG
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: installable (%u, %u, %u, %u)", 
                    nbr->ost_my_uninstallable, todo_N_update, 
                    changed_t_offset_default, todo_tx_schedule_change);
        );
#endif
        return;
      } else if(result == -1) { /* non-installable, request different t_offset */
        if(nbr->ost_my_uninstallable != 1) {
          nbr->ost_my_uninstallable = 1;
          todo_N_update = 1;
        }
        changed_t_offset_default = 0;
        todo_tx_schedule_change = 1;  /* do not add, just remove */
        
#if WITH_OST_LOG
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: non-installable (%u, %u, %u, %u)", 
                    nbr->ost_my_uninstallable, todo_N_update, 
                    changed_t_offset_default, todo_tx_schedule_change);
        );
#endif
        return;
      } else if(result == -2) { /* denial from nbr (when t_offset == 65535) */
        if(nbr->ost_my_uninstallable != 0) {
          nbr->ost_my_uninstallable = 0;
          todo_N_update = 1;
        }
        changed_t_offset_default = 1;   
        todo_tx_schedule_change = 1;  /* do not add, just remove */

#if WITH_OST_LOG
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: denial (%u, %u, %u, %u)", 
                    nbr->ost_my_uninstallable, todo_N_update, 
                    changed_t_offset_default, todo_tx_schedule_change);
        );
#endif
        return;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
/* called from tsch_rx_process_pending (after slot_operation) */
void
post_process_rx_t_offset(void)
{
  linkaddr_t *nbr_lladdr = (linkaddr_t *)uip_ds6_nbr_get_ll(prt_nbr);

  if(todo_N_inc == 1) {
    todo_N_inc = 0;

    if(prt_nbr->ost_my_N < N_MAX) {

#if WITH_OST_LOG
      TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ost post_prT: inc N %u -> %u", prt_nbr->ost_my_N, (prt_nbr->ost_my_N) + 1);
      );
#endif

      (prt_nbr->ost_my_N)++;
      ost_change_queue_N_update(nbr_lladdr, prt_nbr->ost_my_N);
    } else {

#if WITH_OST_LOG
      TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ost post_prT: inc N MAX");
      );
#endif
    }

  } else if(todo_tx_schedule_change == 1) {
    todo_tx_schedule_change = 0;

    if(prt_nbr != NULL) {
      ost_remove_tx(nbr_lladdr);

      if(prt_nbr->ost_my_uninstallable == 0) {
        if(changed_t_offset_default == 1) {
          /* New t_offset 65535 */
        } else {
          ost_add_tx(nbr_lladdr, prt_nbr->ost_my_N, prt_new_t_offset);

#if WITH_OST_LOG
#if !WITH_OST_OID
          uint16_t nbr_id = ost_node_id_from_ipaddr(&(prt_nbr->ipaddr)); //chk
#else
          uint16_t nbr_id = prt_nbr->ds6_ost_id;
#endif
          TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "ost post_prT: nbr %u, (%u, %u)", nbr_id, prt_nbr->ost_my_N, prt_new_t_offset);
          );
#endif
        }
      } else {

#if WITH_OST_LOG
#if !WITH_OST_OID
        uint16_t nbr_id = ost_node_id_from_ipaddr(&(prt_nbr->ipaddr)); //chk
#else
        uint16_t nbr_id = prt_nbr->ds6_ost_id;
#endif
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost post_prT: uninstallable nbr %u", nbr_id);
        );
#endif

      }

      if(todo_N_update == 1) {
        todo_N_update = 0;

        if(prt_nbr->ost_my_uninstallable == 0) {
          ost_change_queue_N_update(nbr_lladdr, prt_nbr->ost_my_N);
        } else {
          ost_change_queue_N_update(nbr_lladdr, prt_nbr->ost_my_N + INC_N_NEW_TX_REQUEST);
        }
      }
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* TSCH locking system. TSCH is locked during slot operations */

/* Is TSCH locked? */
int
tsch_is_locked(void)
{
  return tsch_locked;
}

/* Lock TSCH (no slot operation) */
int
tsch_get_lock(void)
{
  if(!tsch_locked) {
    rtimer_clock_t busy_wait_time;
    int busy_wait = 0; /* Flag used for logging purposes */
    /* Make sure no new slot operation will start */
    tsch_lock_requested = 1;
    /* Wait for the end of current slot operation. */
    if(tsch_in_slot_operation) {
      busy_wait = 1;
      busy_wait_time = RTIMER_NOW();
      while(tsch_in_slot_operation) {
        watchdog_periodic();
      }
      busy_wait_time = RTIMER_NOW() - busy_wait_time;
    }
    if(!tsch_locked) {
      /* Take the lock if it is free */
      tsch_locked = 1;
      tsch_lock_requested = 0;
      if(busy_wait) {
        /* Issue a log whenever we had to busy wait until getting the lock */
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "!get lock delay %u", (unsigned)busy_wait_time);
        );
      }
      return 1;
    }
  }
  TSCH_LOG_ADD(tsch_log_message,
      snprintf(log->message, sizeof(log->message),
                      "!failed to lock");
          );
  return 0;
}

/* Release TSCH lock */
void
tsch_release_lock(void)
{
  tsch_locked = 0;
}

/*---------------------------------------------------------------------------*/
/* Channel hopping utility functions */

/* Return the channel offset to use for the current slot */
static uint8_t
tsch_get_channel_offset(struct tsch_link *link, struct tsch_packet *p)
{
#if WITH_ALICE /* alice-implementation */
  return link->channel_offset;
#endif

#if WITH_OST
  return link->channel_offset;
#endif

#if TSCH_WITH_LINK_SELECTOR
  if(p != NULL) {
    uint16_t packet_channel_offset = queuebuf_attr(p->qb, PACKETBUF_ATTR_TSCH_CHANNEL_OFFSET);
    if(packet_channel_offset != 0xffff) {
      /* The schedule specifies a channel offset for this one; use it */
      return packet_channel_offset;
    }
  }
#endif
  return link->channel_offset;
}

/**
 * Returns a 802.15.4 channel from an ASN and channel offset. Basically adds
 * The offset to the ASN and performs a hopping sequence lookup.
 *
 * \param asn A given ASN
 * \param channel_offset Given channel offset
 * \return The resulting channel
 */
static uint8_t
tsch_calculate_channel(struct tsch_asn_t *asn, uint16_t channel_offset)
{
  uint16_t index_of_0, index_of_offset;
  index_of_0 = TSCH_ASN_MOD(*asn, tsch_hopping_sequence_length);
  index_of_offset = (index_of_0 + channel_offset) % tsch_hopping_sequence_length.val;
  return tsch_hopping_sequence[index_of_offset];
}

/*---------------------------------------------------------------------------*/
/* Timing utility functions */

/* Checks if the current time has passed a ref time + offset. Assumes
 * a single overflow and ref time prior to now. */
static uint8_t
check_timer_miss(rtimer_clock_t ref_time, rtimer_clock_t offset, rtimer_clock_t now)
{
  rtimer_clock_t target = ref_time + offset;
  int now_has_overflowed = now < ref_time;
  int target_has_overflowed = target < ref_time;

  if(now_has_overflowed == target_has_overflowed) {
    /* Both or none have overflowed, just compare now to the target */
    return target <= now;
  } else {
    /* Either now or target of overflowed.
     * If it is now, then it has passed the target.
     * If it is target, then we haven't reached it yet.
     *  */
    return now_has_overflowed;
  }
}
/*---------------------------------------------------------------------------*/
/* Schedule a wakeup at a specified offset from a reference time.
 * Provides basic protection against missed deadlines and timer overflows
 * A return value of zero signals a missed deadline: no rtimer was scheduled. */
static uint8_t
tsch_schedule_slot_operation(struct rtimer *tm, rtimer_clock_t ref_time, rtimer_clock_t offset, const char *str)
{
  rtimer_clock_t now = RTIMER_NOW();
  int r;
  /* Subtract RTIMER_GUARD before checking for deadline miss
   * because we can not schedule rtimer less than RTIMER_GUARD in the future */
  int missed = check_timer_miss(ref_time, offset - RTIMER_GUARD, now);
  if(missed) {
    TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "!dl-miss %s %d %d",
                        str, (int)(now-ref_time), (int)offset);
    );
  } else {
    r = rtimer_set(tm, ref_time + offset, 1, (void (*)(struct rtimer *, void *))tsch_slot_operation, NULL);
    if(r == RTIMER_OK) {
      return 1;
    }
  }

  /* block until the time to schedule comes */
  RTIMER_BUSYWAIT_UNTIL_ABS(0, ref_time, offset);
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Schedule slot operation conditionally, and YIELD if success only.
 * Always attempt to schedule RTIMER_GUARD before the target to make sure to wake up
 * ahead of time and then busy wait to exactly hit the target. */
#define TSCH_SCHEDULE_AND_YIELD(pt, tm, ref_time, offset, str) \
  do { \
    if(tsch_schedule_slot_operation(tm, ref_time, offset - RTIMER_GUARD, str)) { \
      PT_YIELD(pt); \
      RTIMER_BUSYWAIT_UNTIL_ABS(0, ref_time, offset); \
    } \
  } while(0);
/*---------------------------------------------------------------------------*/
/* Get EB, broadcast or unicast packet to be sent, and target neighbor. */
static struct tsch_packet *
get_packet_and_neighbor_for_link(struct tsch_link *link, struct tsch_neighbor **target_neighbor)
{
  struct tsch_packet *p = NULL;
  struct tsch_neighbor *n = NULL;

  /* Is this a Tx link? */
  if(link->link_options & LINK_OPTION_TX) {
    /* is it for advertisement of EB? */
    if(link->link_type == LINK_TYPE_ADVERTISING || link->link_type == LINK_TYPE_ADVERTISING_ONLY) {
      /* fetch EB packets */
      n = n_eb;
      p = tsch_queue_get_packet_for_nbr(n, link);
    }
    if(link->link_type != LINK_TYPE_ADVERTISING_ONLY) {
      /* NORMAL link or no EB to send, pick a data packet */
      if(p == NULL) {
        /* Get neighbor queue associated to the link and get packet from it */
        n = tsch_queue_get_nbr(&link->addr);
        p = tsch_queue_get_packet_for_nbr(n, link);
        /* if it is a broadcast slot and there were no broadcast packets, pick any unicast packet */
        if(p == NULL && n == n_broadcast) {
          p = tsch_queue_get_unicast_packet_for_any(&n, link);
        }
      }
    }
  }
  /* return nbr (by reference) */
  if(target_neighbor != NULL) {
    *target_neighbor = n;
  }

  return p;
}
/*---------------------------------------------------------------------------*/
uint64_t
tsch_get_network_uptime_ticks(void)
{
  uint64_t uptime_asn;
  uint64_t uptime_ticks;
  int_master_status_t status;

  if(!tsch_is_associated) {
    /* not associated, network uptime is not known */
    return (uint64_t)-1;
  }

  status = critical_enter();

  uptime_asn = last_sync_asn.ls4b + ((uint64_t)last_sync_asn.ms1b << 32);
  /* first calculate the at the uptime at the last sync in rtimer ticks */
  uptime_ticks = uptime_asn * tsch_timing[tsch_ts_timeslot_length];
  /* then convert to clock ticks (assume that CLOCK_SECOND divides RTIMER_ARCH_SECOND) */
  uptime_ticks /= (RTIMER_ARCH_SECOND / CLOCK_SECOND);
  /* then add the ticks passed since the last timesync */
  uptime_ticks += (clock_time() - tsch_last_sync_time);

  critical_exit(status);

  return uptime_ticks;
}
/*---------------------------------------------------------------------------*/
/**
 * This function turns on the radio. Its semantics is dependent on
 * the value of TSCH_RADIO_ON_DURING_TIMESLOT constant:
 * - if enabled, the radio is turned on at the start of the slot
 * - if disabled, the radio is turned on within the slot,
 *   directly before the packet Rx guard time and ACK Rx guard time.
 */
static void
tsch_radio_on(enum tsch_radio_state_on_cmd command)
{
  int do_it = 0;
  switch(command) {
  case TSCH_RADIO_CMD_ON_START_OF_TIMESLOT:
    if(TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT:
    if(!TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_ON_FORCE:
    do_it = 1;
    break;
  }
  if(do_it) {
    NETSTACK_RADIO.on();
  }
}
/*---------------------------------------------------------------------------*/
/**
 * This function turns off the radio. In the same way as for tsch_radio_on(),
 * it depends on the value of TSCH_RADIO_ON_DURING_TIMESLOT constant:
 * - if enabled, the radio is turned off at the end of the slot
 * - if disabled, the radio is turned off within the slot,
 *   directly after Tx'ing or Rx'ing a packet or Tx'ing an ACK.
 */
static void
tsch_radio_off(enum tsch_radio_state_off_cmd command)
{
  int do_it = 0;
  switch(command) {
  case TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT:
    if(TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT:
    if(!TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_OFF_FORCE:
    do_it = 1;
    break;
  }
  if(do_it) {
    NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/
#if WITH_OST && OST_ON_DEMAND_PROVISION
uint8_t
reserved_ssq(uint16_t nbr_id)
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ssq_schedule_list[i].asn.ls4b == 0 && ssq_schedule_list[i].asn.ms1b == 0) {
      /* do nothing */
    } else if(!(ssq_schedule_list[i].asn.ls4b == tsch_current_asn.ls4b 
              && ssq_schedule_list[i].asn.ms1b == tsch_current_asn.ms1b)) {
      if(ssq_schedule_list[i].link.link_options == LINK_OPTION_TX) {
        uint16_t id = (ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
        if(nbr_id == id) {
          // printf("reserved for %u\n", id);
          return 1;
        }
      }
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
remove_reserved_ssq(uint16_t nbr_id) //hckim
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ssq_schedule_list[i].asn.ls4b == 0 && ssq_schedule_list[i].asn.ms1b == 0) {

    } else if(!(ssq_schedule_list[i].asn.ls4b == tsch_current_asn.ls4b && ssq_schedule_list[i].asn.ms1b == tsch_current_asn.ms1b)) {
      if(ssq_schedule_list[i].link.link_options == LINK_OPTION_TX) {
        uint16_t id = (ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
        if(nbr_id == id) {
          ssq_schedule_list[i].asn.ls4b = 0;
          ssq_schedule_list[i].asn.ms1b = 0;
        }
      }
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/
static
PT_THREAD(tsch_tx_slot(struct pt *pt, struct rtimer *t))
{
  /**
   * TX slot:
   * 1. Copy packet to radio buffer
   * 2. Perform CCA if enabled
   * 3. Sleep until it is time to transmit
   * 4. Wait for ACK if it is a unicast packet
   * 5. Extract drift if we received an E-ACK from a time source neighbor
   * 6. Update CSMA parameters according to TX status
   * 7. Schedule mac_call_sent_callback
   **/

  /* tx status */
  static uint8_t mac_tx_status;
  /* is the packet in its neighbor's queue? */
  uint8_t in_queue;
  static int dequeued_index;
  static int packet_ready = 1;

  PT_BEGIN(pt);

  TSCH_DEBUG_TX_EVENT();

  ++tsch_unlocked_scheduled_tx_cell_count; //hckim

  /* First check if we have space to store a newly dequeued packet (in case of
   * successful Tx or Drop) */
  dequeued_index = ringbufindex_peek_put(&dequeued_ringbuf);
  if(dequeued_index != -1) {
    if(current_packet == NULL || current_packet->qb == NULL) {
      mac_tx_status = MAC_TX_ERR_FATAL;
    } else {
      /* packet payload */
      static void *packet;
#if LLSEC802154_ENABLED
      /* encrypted payload */
      static uint8_t encrypted_packet[TSCH_PACKET_MAX_LEN];
#endif /* LLSEC802154_ENABLED */
      /* packet payload length */
      static uint8_t packet_len;
      /* packet seqno */
      static uint8_t seqno;
      /* wait for ack? */
      static uint8_t do_wait_for_ack;
      static rtimer_clock_t tx_start_time;
#if !WITH_OST && !WITH_ALICE
      /* Did we set the frame pending bit to request an extra burst link? */
      static int burst_link_requested;
#endif

#if TSCH_CCA_ENABLED
      static uint8_t cca_status;
#endif /* TSCH_CCA_ENABLED */

      /* get payload */
      packet = queuebuf_dataptr(current_packet->qb);
      packet_len = queuebuf_datalen(current_packet->qb);
      /* if is this a broadcast packet, don't wait for ack */
      do_wait_for_ack = !current_neighbor->is_broadcast;
#if !WITH_OST && !WITH_ALICE
      /* Unicast. More packets in queue for the neighbor? */
      burst_link_requested = 0;
      if(do_wait_for_ack
             && tsch_current_burst_count + 1 < TSCH_BURST_MAX_LEN
             && tsch_queue_nbr_packet_count(current_neighbor) > 1) {
        burst_link_requested = 1;
        tsch_packet_set_frame_pending(packet, packet_len);
      }
#endif
      /* read seqno from payload */
      seqno = ((uint8_t *)(packet))[2];

#if WITH_OST
#if OST_ON_DEMAND_PROVISION
      frame802154_fcf_t fcf;
      frame802154_parse_fcf((uint8_t *)(packet), &fcf);

      int queued_pkts = ringbufindex_elements(&current_neighbor->tx_ringbuf);
#if !WITH_OST_OID
      uint16_t nbr_id = ost_node_id_from_linkaddr(tsch_queue_get_nbr_address(current_neighbor)); // chk
#else
      uint16_t nbr_id = current_neighbor->tsch_ost_id;
#endif

      if(fcf.ack_required) {
        if(queued_pkts > 1) {
          if(reserved_ssq(nbr_id) == 0) { /* if 1, there exists at least one reserved ssq for nbr_id */
            fcf.frame_pending = 1;

            uint16_t ssq_schedule = tsch_schedule_get_subsequent_schedule(&tsch_current_asn);

            /* To disable ssq bit after periodic provision Tx */
            uint16_t tx_sf_handle = ost_get_tx_sf_handle_from_id(nbr_id);
            /* Periodic provision */
            struct tsch_slotframe *sf = tsch_schedule_get_slotframe_by_handle(tx_sf_handle);

            if(sf != NULL) {
              uint16_t timeslot = TSCH_ASN_MOD(tsch_current_asn, sf->size);
              struct tsch_link *l = list_head(sf->links_list);
              if(l != NULL) {
                uint16_t time_to_timeslot =
                  l->timeslot > timeslot ?
                  l->timeslot - timeslot :
                  sf->size.val + l->timeslot - timeslot;

                if((time_to_timeslot - 1) < 16) {
                  uint8_t i;
                  for(i = time_to_timeslot - 1; i < 16; i++) {
                    ssq_schedule = ssq_schedule | (1 << i);
                  }
                }
              } else {
                /* "ERROR: No tx link in tx sf */
              }
            }            

#if WITH_OST_LOG
            TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                "ost odp: ssq_sched %u", ssq_schedule)
            );
#endif

            /* put ssq_schedule into the position of pigg2 */
            ((uint8_t *)(packet))[4] = ssq_schedule & 0xff;
            ((uint8_t *)(packet))[5] = (ssq_schedule >> 8) & 0xff;

          } else { /* there already exists reserved ssq for nbr_id */
            fcf.frame_pending = 0;
            ((uint8_t *)(packet))[4] = 255; /* full of 1 bits */
            ((uint8_t *)(packet))[5] = 255; /* full of 1 bits */
          }
        } else if(queued_pkts == 1) { /* only one packet is queued -> no need for ssq */
          fcf.frame_pending = 0;
          ((uint8_t *)(packet))[4] = 255; /* full of 1 bits */
          ((uint8_t *)(packet))[5] = 255; /* full of 1 bits */
        } else {
          /* ERROR: No packet in Tx queue */
        }
        frame802154_create_fcf(&fcf, (uint8_t *)(packet)); /* why here ??? maybe due to fcf change??? */
      } else {
        ((uint8_t *)(packet))[4] = 255;
        ((uint8_t *)(packet))[5] = 255;
      }

      seqno = ((uint8_t *)(packet))[6];

#else /* OST_ON_DEMAND_PROVISION */

      seqno = ((uint8_t *)(packet))[4];

#endif /* OST_ON_DEMAND_PROVISION */
#endif /* WITH_OST */

      /* if this is an EB, then update its Sync-IE */
      if(current_neighbor == n_eb) {
        packet_ready = tsch_packet_update_eb(packet, packet_len, current_packet->tsch_sync_ie_offset);
      } else {
        packet_ready = 1;
      }

#if LLSEC802154_ENABLED
      if(tsch_is_pan_secured) {
        /* If we are going to encrypt, we need to generate the output in a separate buffer and keep
         * the original untouched. This is to allow for future retransmissions. */
        int with_encryption = queuebuf_attr(current_packet->qb, PACKETBUF_ATTR_SECURITY_LEVEL) & 0x4;
        packet_len += tsch_security_secure_frame(packet, with_encryption ? encrypted_packet : packet, current_packet->header_len,
            packet_len - current_packet->header_len, &tsch_current_asn);
        if(with_encryption) {
          packet = encrypted_packet;
        }
      }
#endif /* LLSEC802154_ENABLED */

      /* prepare packet to send: copy to radio buffer */
      if(packet_ready && NETSTACK_RADIO.prepare(packet, packet_len) == 0) { /* 0 means success */
        static rtimer_clock_t tx_duration;

#if TSCH_CCA_ENABLED
        cca_status = 1;
        /* delay before CCA */
        TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_cca_offset], "cca");
        TSCH_DEBUG_TX_EVENT();
        tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
        /* CCA */
        RTIMER_BUSYWAIT_UNTIL_ABS(!(cca_status &= NETSTACK_RADIO.channel_clear()),
                           current_slot_start, tsch_timing[tsch_ts_cca_offset] + tsch_timing[tsch_ts_cca]);
        TSCH_DEBUG_TX_EVENT();
        /* there is not enough time to turn radio off */
        /*  NETSTACK_RADIO.off(); */
        if(cca_status == 0) {
          mac_tx_status = MAC_TX_COLLISION;
        } else
#endif /* TSCH_CCA_ENABLED */
        {
          /* delay before TX */
          TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_tx_offset] - RADIO_DELAY_BEFORE_TX, "TxBeforeTx");
          TSCH_DEBUG_TX_EVENT();
          /* send packet already in radio tx buffer */
          mac_tx_status = NETSTACK_RADIO.transmit(packet_len);
          tx_count++;
          /* Save tx timestamp */
          tx_start_time = current_slot_start + tsch_timing[tsch_ts_tx_offset];
          /* calculate TX duration based on sent packet len */
          tx_duration = TSCH_PACKET_DURATION(packet_len);
          /* limit tx_time to its max value */
          tx_duration = MIN(tx_duration, tsch_timing[tsch_ts_max_tx]);
          /* turn tadio off -- will turn on again to wait for ACK if needed */
          tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

          if(mac_tx_status == RADIO_TX_OK) {
            if(do_wait_for_ack) {
              uint8_t ackbuf[TSCH_PACKET_MAX_LEN];
              int ack_len;
              rtimer_clock_t ack_start_time;
              int is_time_source;
              struct ieee802154_ies ack_ies;
              uint8_t ack_hdrlen;
              frame802154_t frame;

#if TSCH_HW_FRAME_FILTERING
              radio_value_t radio_rx_mode;
              /* Entering promiscuous mode so that the radio accepts the enhanced ACK */
              NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
              NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode & (~RADIO_RX_MODE_ADDRESS_FILTER));
#endif /* TSCH_HW_FRAME_FILTERING */
              /* Unicast: wait for ack after tx: sleep until ack time */
              TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start,
                  tsch_timing[tsch_ts_tx_offset] + tx_duration + tsch_timing[tsch_ts_rx_ack_delay] - RADIO_DELAY_BEFORE_RX, "TxBeforeAck");
              TSCH_DEBUG_TX_EVENT();
              tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
              /* Wait for ACK to come */
              RTIMER_BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),
                  tx_start_time, tx_duration + tsch_timing[tsch_ts_rx_ack_delay] + tsch_timing[tsch_ts_ack_wait] + RADIO_DELAY_BEFORE_DETECT);
              TSCH_DEBUG_TX_EVENT();

              ack_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

              /* Wait for ACK to finish */
              RTIMER_BUSYWAIT_UNTIL_ABS(!NETSTACK_RADIO.receiving_packet(),
                                 ack_start_time, tsch_timing[tsch_ts_max_ack]);
              TSCH_DEBUG_TX_EVENT();
              tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

#if TSCH_HW_FRAME_FILTERING
              /* Leaving promiscuous mode */
              NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
              NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode | RADIO_RX_MODE_ADDRESS_FILTER);
#endif /* TSCH_HW_FRAME_FILTERING */

              /* Read ack frame */
              ack_len = NETSTACK_RADIO.read((void *)ackbuf, sizeof(ackbuf));

              is_time_source = 0;
              /* The radio driver should return 0 if no valid packets are in the rx buffer */
              if(ack_len > 0) {
                is_time_source = current_neighbor != NULL && current_neighbor->is_time_source;

                if(tsch_packet_parse_eack(ackbuf, ack_len, seqno,
                    &frame, &ack_ies, &ack_hdrlen) == 0) {
                  ack_len = 0;
                }

#if WITH_OST_LOG
#if !WITH_OST_OID
                uint16_t nbr_id = ost_node_id_from_linkaddr(tsch_queue_get_nbr_address(current_neighbor)); // chk
#else
                uint16_t nbr_id = current_neighbor->tsch_ost_id;
#endif
                TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                    "ost rcvd t_offset: nbr %u, %u", nbr_id, frame.ost_pigg1)
                );
#endif

#if WITH_OST
                todo_tx_schedule_change = 0;
                todo_N_update = 0;
                todo_N_inc = 0;

                if(ack_len != 0) {
                  process_rx_t_offset(&frame);
#if OST_ON_DEMAND_PROVISION
                  process_rx_matching_slot(&frame);
#endif
                }
#endif

#if LLSEC802154_ENABLED
                if(ack_len != 0) {
                  if(!tsch_security_parse_frame(ackbuf, ack_hdrlen, ack_len - ack_hdrlen - tsch_security_mic_len(&frame),
                      &frame, tsch_queue_get_nbr_address(current_neighbor), &tsch_current_asn)) {
                    TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                        "!failed to authenticate ACK"));
                    ack_len = 0;
                  }
                } else {
                  TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                      "!failed to parse ACK"));
                }
#endif /* LLSEC802154_ENABLED */
              }
              if(ack_len != 0) {
                if(is_time_source) {
                  int32_t eack_time_correction = US_TO_RTIMERTICKS(ack_ies.ie_time_correction);
                  int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
                  if(eack_time_correction > SYNC_IE_BOUND) {
                    drift_correction = SYNC_IE_BOUND;
                  } else if(eack_time_correction < -SYNC_IE_BOUND) {
                    drift_correction = -SYNC_IE_BOUND;
                  } else {
                    drift_correction = eack_time_correction;
                  }
                  if(drift_correction != eack_time_correction) {
                    TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "!truncated dr %d %d", (int)eack_time_correction, (int)drift_correction);
                    );
                  }
                  tsch_stats_on_time_synchronization(eack_time_correction);
                  is_drift_correction_used = 1;
                  tsch_timesync_update(current_neighbor, since_last_timesync, drift_correction);
                  /* Keep track of sync time */
                  last_sync_asn = tsch_current_asn;
                  tsch_last_sync_time = clock_time();
                  tsch_schedule_keepalive(0);
                }
                mac_tx_status = MAC_TX_OK;

#if !WITH_OST && !WITH_ALICE
                /* We requested an extra slot and got an ack. This means
                the extra slot will be scheduled at the received */
                if(burst_link_requested) {
                  burst_link_scheduled = 1;
                }
#endif
              } else {
                mac_tx_status = MAC_TX_NOACK;
              }
            } else {
              mac_tx_status = MAC_TX_OK;
            }
          } else {
            mac_tx_status = MAC_TX_ERR;
          }
        }
      } else {
        mac_tx_status = MAC_TX_ERR;
      }
    }

    tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

    current_packet->transmissions++;
    current_packet->ret = mac_tx_status;

    /* Post TX: Update neighbor queue state */
    in_queue = tsch_queue_packet_sent(current_neighbor, current_packet, current_link, mac_tx_status);

    /* The packet was dequeued, add it to dequeued_ringbuf for later processing */
    if(in_queue == 0) {
      dequeued_array[dequeued_index] = current_packet;
      ringbufindex_put(&dequeued_ringbuf);
    }

    /* If this is an unicast packet to timesource, update stats */
    if(current_neighbor != NULL && current_neighbor->is_time_source) {
      tsch_stats_tx_packet(current_neighbor, mac_tx_status, tsch_current_channel);
    }

    /* Log every tx attempt */
    TSCH_LOG_ADD(tsch_log_tx,
        log->tx.mac_tx_status = mac_tx_status;
        log->tx.num_tx = current_packet->transmissions;
        log->tx.datalen = queuebuf_datalen(current_packet->qb);
        log->tx.drift = drift_correction;
        log->tx.drift_used = is_drift_correction_used;
        log->tx.is_data = ((((uint8_t *)(queuebuf_dataptr(current_packet->qb)))[0]) & 7) == FRAME802154_DATAFRAME;
#if LLSEC802154_ENABLED
        log->tx.sec_level = queuebuf_attr(current_packet->qb, PACKETBUF_ATTR_SECURITY_LEVEL);
#else /* LLSEC802154_ENABLED */
        log->tx.sec_level = 0;
#endif /* LLSEC802154_ENABLED */
        linkaddr_copy(&log->tx.dest, queuebuf_addr(current_packet->qb, PACKETBUF_ADDR_RECEIVER));
        log->tx.seqno = queuebuf_attr(current_packet->qb, PACKETBUF_ATTR_MAC_SEQNO);
    );

    ++tsch_unlocked_scheduled_tx_operation_count; //hckim

    /* Poll process for later processing of packet sent events and logs */
    process_poll(&tsch_pending_events_process);
  }

#if WITH_OST && OST_ON_DEMAND_PROVISION
  if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET) {
    remove_matching_slot();
  }
  if(current_link->slotframe_handle == 1) { /* RB */
    int queued_pkts = ringbufindex_elements(&current_neighbor->tx_ringbuf);
#if !WITH_OST_OID
    uint16_t nbr_id = ost_node_id_from_linkaddr(tsch_queue_get_nbr_address(current_neighbor)); // chk
#else
    uint16_t nbr_id = current_neighbor->tsch_ost_id;
#endif

    if(reserved_ssq(nbr_id) && queued_pkts == 0) { /* Tx occurs by RB before reserved ssq Tx */
      /* Remove Tx ssq by RB */
      remove_reserved_ssq(nbr_id);
    } 
  }
#endif


  TSCH_DEBUG_TX_EVENT();

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(tsch_rx_slot(struct pt *pt, struct rtimer *t))
{
  /**
   * RX slot:
   * 1. Check if it is used for TIME_KEEPING
   * 2. Sleep and wake up just before expected RX time (with a guard time: TS_LONG_GT)
   * 3. Check for radio activity for the guard time: TS_LONG_GT
   * 4. Prepare and send ACK if needed
   * 5. Drift calculated in the ACK callback registered with the radio driver. Use it if receiving from a time source neighbor.
   **/

  struct tsch_neighbor *n;
  static linkaddr_t source_address;
  static linkaddr_t destination_address;
  static int16_t input_index;
  static int input_queue_drop = 0;

  PT_BEGIN(pt);

  TSCH_DEBUG_RX_EVENT();

  ++tsch_unlocked_scheduled_rx_cell_count; //hckim

  input_index = ringbufindex_peek_put(&input_ringbuf);
  if(input_index == -1) {
    input_queue_drop++;
  } else {
    static struct input_packet *current_input;
    /* Estimated drift based on RX time */
    static int32_t estimated_drift;
    /* Rx timestamps */
    static rtimer_clock_t rx_start_time;
    static rtimer_clock_t expected_rx_time;
    static rtimer_clock_t packet_duration;
    uint8_t packet_seen;

    expected_rx_time = current_slot_start + tsch_timing[tsch_ts_tx_offset];
    /* Default start time: expected Rx time */
    rx_start_time = expected_rx_time;

    current_input = &input_array[input_index];

    /* Wait before starting to listen */
    TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");
    TSCH_DEBUG_RX_EVENT();

    /* Start radio for at least guard time */
    tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
    packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();

    if(!packet_seen) {
      /* Check if receiving within guard time */
      RTIMER_BUSYWAIT_UNTIL_ABS((packet_seen = (NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet())),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + RADIO_DELAY_BEFORE_DETECT);
    }
    if(!packet_seen) {
      /* no packets on air */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_FORCE);

#if ENABLE_LOG_TSCH_SLOT_LEVEL_RX_LOG
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "!no packet seen"));
#endif

    } else {
      TSCH_DEBUG_RX_EVENT();
      /* Save packet timestamp */
      rx_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

      /* Wait until packet is received, turn radio off */
      RTIMER_BUSYWAIT_UNTIL_ABS(!NETSTACK_RADIO.receiving_packet(),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + tsch_timing[tsch_ts_max_tx]);
      TSCH_DEBUG_RX_EVENT();
      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()) {
        static int frame_valid;
        static int header_len;
        static frame802154_t frame;
        radio_value_t radio_last_rssi;
        radio_value_t radio_last_lqi;

        /* Read packet */
        current_input->len = NETSTACK_RADIO.read((void *)current_input->payload, TSCH_PACKET_MAX_LEN);
        NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_RSSI, &radio_last_rssi);
        current_input->rx_asn = tsch_current_asn;
        current_input->rssi = (signed)radio_last_rssi;
        current_input->channel = tsch_current_channel;
        header_len = frame802154_parse((uint8_t *)current_input->payload, current_input->len, &frame);
        frame_valid = header_len > 0 &&
          frame802154_check_dest_panid(&frame) &&
          frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

#if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
        /* At the end of the reception, get an more accurate estimate of SFD arrival time */
        NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
#endif

        packet_duration = TSCH_PACKET_DURATION(current_input->len);
        /* limit packet_duration to its max value */
        packet_duration = MIN(packet_duration, tsch_timing[tsch_ts_max_tx]);

        if(!frame_valid) {
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "!failed to parse frame %u %u", header_len, current_input->len));
        }

        if(frame_valid) {
          if(frame.fcf.frame_type != FRAME802154_DATAFRAME
            && frame.fcf.frame_type != FRAME802154_BEACONFRAME) {
              TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                  "!discarding frame with type %u, len %u", frame.fcf.frame_type, current_input->len));
              frame_valid = 0;
          }
        }

#if LLSEC802154_ENABLED
        /* Decrypt and verify incoming frame */
        if(frame_valid) {
          if(tsch_security_parse_frame(
               current_input->payload, header_len, current_input->len - header_len - tsch_security_mic_len(&frame),
               &frame, &source_address, &tsch_current_asn)) {
            current_input->len -= tsch_security_mic_len(&frame);
          } else {
            TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                "!failed to authenticate frame %u", current_input->len));
            frame_valid = 0;
          }
        }
#endif /* LLSEC802154_ENABLED */

        if(frame_valid) {
          /* Check that frome is for us or broadcast, AND that it is not from
           * ourselves. This is for consistency with CSMA and to avoid adding
           * ourselves to neighbor tables in case frames are being replayed. */
          if((linkaddr_cmp(&destination_address, &linkaddr_node_addr)
               || linkaddr_cmp(&destination_address, &linkaddr_null))
             && !linkaddr_cmp(&source_address, &linkaddr_node_addr)) {
            int do_nack = 0;
            rx_count++;
            estimated_drift = RTIMER_CLOCK_DIFF(expected_rx_time, rx_start_time);
            tsch_stats_on_time_synchronization(estimated_drift);

#if TSCH_TIMESYNC_REMOVE_JITTER
            /* remove jitter due to measurement errors */
            if(ABS(estimated_drift) <= TSCH_TIMESYNC_MEASUREMENT_ERROR) {
              estimated_drift = 0;
            } else if(estimated_drift > 0) {
              estimated_drift -= TSCH_TIMESYNC_MEASUREMENT_ERROR;
            } else {
              estimated_drift += TSCH_TIMESYNC_MEASUREMENT_ERROR;
            }
#endif

#ifdef TSCH_CALLBACK_DO_NACK
            if(frame.fcf.ack_required) {
              do_nack = TSCH_CALLBACK_DO_NACK(current_link,
                  &source_address, &destination_address);
            }
#endif

            if(frame.fcf.ack_required) {
              static uint8_t ack_buf[TSCH_PACKET_MAX_LEN];
              static int ack_len;

#if WITH_OST
              /* before generate EACK */
              todo_rx_schedule_change = 0;
              ost_flag_failed_to_select_t_offset = 0;
              ost_todo_consecutive_new_tx_request = 0;

              process_rx_N(&frame);
#if OST_ON_DEMAND_PROVISION              
              uint16_t matching_slot = process_rx_schedule_info(&frame);

#if WITH_OST_LOG
              TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                  "ost odp: matching_slot %u", matching_slot)
              );
#endif
#endif
#endif

#if WITH_OST && OST_ON_DEMAND_PROVISION
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack, matching_slot);
#else
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack);
#endif
              if(ack_len > 0) {
#if LLSEC802154_ENABLED
                if(tsch_is_pan_secured) {
                  /* Secure ACK frame. There is only header and header IEs, therefore data len == 0. */
                  ack_len += tsch_security_secure_frame(ack_buf, ack_buf, ack_len, 0, &tsch_current_asn);
                }
#endif /* LLSEC802154_ENABLED */

                /* Copy to radio buffer */
                NETSTACK_RADIO.prepare((const void *)ack_buf, ack_len);

                /* Wait for time to ACK and transmit ACK */
                TSCH_SCHEDULE_AND_YIELD(pt, t, rx_start_time,
                                        packet_duration + tsch_timing[tsch_ts_tx_ack_delay] - RADIO_DELAY_BEFORE_TX, "RxBeforeAck");
                TSCH_DEBUG_RX_EVENT();
                NETSTACK_RADIO.transmit(ack_len);
                tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);
#if !WITH_OST && !WITH_ALICE
                /* Schedule a burst link iff the frame pending bit was set */
                burst_link_scheduled = tsch_packet_get_frame_pending(current_input->payload, current_input->len);
#endif
              }
            }

            /* If the sender is a time source, proceed to clock drift compensation */
            n = tsch_queue_get_nbr(&source_address);
            if(n != NULL && n->is_time_source) {
              int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
              /* Keep track of last sync time */
              last_sync_asn = tsch_current_asn;
              tsch_last_sync_time = clock_time();
              /* Save estimated drift */
              drift_correction = -estimated_drift;
              is_drift_correction_used = 1;
              sync_count++;
              tsch_timesync_update(n, since_last_timesync, -estimated_drift);
              tsch_schedule_keepalive(0);
            }

            /* Add current input to ringbuf */
            ringbufindex_put(&input_ringbuf);

            /* If the neighbor is known, update its stats */
            if(n != NULL) {
              NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_LINK_QUALITY, &radio_last_lqi);
              tsch_stats_rx_packet(n, current_input->rssi, radio_last_lqi, tsch_current_channel);
            }

            /* Log every reception */
            TSCH_LOG_ADD(tsch_log_rx,
              linkaddr_copy(&log->rx.src, (linkaddr_t *)&frame.src_addr);
              log->rx.is_unicast = frame.fcf.ack_required;
              log->rx.datalen = current_input->len;
              log->rx.drift = drift_correction;
              log->rx.drift_used = is_drift_correction_used;
              log->rx.is_data = frame.fcf.frame_type == FRAME802154_DATAFRAME;
              log->rx.sec_level = frame.aux_hdr.security_control.security_level;
              log->rx.estimated_drift = estimated_drift;
              log->rx.seqno = frame.seq;
            );
          }

          ++tsch_unlocked_scheduled_rx_operation_count; //hckim

          /* Poll process for processing of pending input and logs */
          process_poll(&tsch_pending_events_process);
        }
      }

      tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

#if ENABLE_LOG_TSCH_SLOT_LEVEL_RX_LOG
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "!no pending packet"));
#endif

    }

    if(input_queue_drop != 0) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
              "!queue full skipped %u", input_queue_drop);
      );
      input_queue_drop = 0;
    }
  }

#if WITH_OST && OST_ON_DEMAND_PROVISION
  if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET) {
    remove_matching_slot();
  }
#endif  

  TSCH_DEBUG_RX_EVENT();

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
/* Protothread for slot operation, called from rtimer interrupt
 * and scheduled from tsch_schedule_slot_operation */
static
PT_THREAD(tsch_slot_operation(struct rtimer *t, void *ptr))
{
  TSCH_DEBUG_INTERRUPT();
  PT_BEGIN(&slot_operation_pt);

  /* Loop over all active slots */
  while(tsch_is_associated) {

    if(current_link == NULL || tsch_lock_requested) { /* Skip slot operation if there is no link
                                                          or if there is a pending request for getting the lock */
      /* Issue a log whenever skipping a slot */
      TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "!skipped slot %u %u %u",
                            tsch_locked,
                            tsch_lock_requested,
                            current_link == NULL);
      );

#if WITH_OST && OST_ON_DEMAND_PROVISION
      if(exist_matching_slot(&tsch_current_asn)) {
        remove_matching_slot();
      }
#endif 

    } else {
      ++tsch_unlocked_scheduled_cell_count; //hckim

      int is_active_slot;
      TSCH_DEBUG_SLOT_START();
      tsch_in_slot_operation = 1;
      /* Measure on-air noise level while TSCH is idle */
      tsch_stats_sample_rssi();
      /* Reset drift correction */
      drift_correction = 0;
      is_drift_correction_used = 0;
      /* Get a packet ready to be sent */
      current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);

#if WITH_OST && OST_ON_DEMAND_PROVISION
      if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET 
        && current_link->link_options == LINK_OPTION_TX) {
        if(current_packet == NULL) {
          if(tsch_is_locked()) { /* tsch-schedule is being changing, so locked - skip this slot */
            remove_matching_slot();
          } else {
            /* ERROR: ssq Tx schedule, but no packets to Tx */
          }
        }
      }
#endif

      /* There is no packet to send, and this link does not have Rx flag. Instead of doing
       * nothing, switch to the backup link (has Rx flag) if any. */
      if(current_packet == NULL && !(current_link->link_options & LINK_OPTION_RX) && backup_link != NULL) {
        current_link = backup_link;
        current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
      }

#if WITH_OST
      /* Seungbeom Jeong added this else if block */
      else if(current_packet == NULL && (current_link->link_options & LINK_OPTION_RX) && backup_link != NULL) {
        if(current_link->slotframe_handle > backup_link->slotframe_handle) {
          /* There could be Tx option in backup link */
          current_link = backup_link;
          current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
        }
      }
#endif

      is_active_slot = current_packet != NULL || (current_link->link_options & LINK_OPTION_RX);
      if(is_active_slot) {

#if WITH_OST
        uint16_t rx_id = 0;

        if(current_link->slotframe_handle >= 3 && current_link->slotframe_handle <= SSQ_SCHEDULE_HANDLE_OFFSET) {
          if(current_link->link_options & LINK_OPTION_TX) {
            rx_id = ost_get_id_from_tx_sf_handle(current_link->slotframe_handle);
          } else if(current_link->link_options & LINK_OPTION_RX) {
#if !WITH_OST_OID
            rx_id = ost_node_id_from_linkaddr(&linkaddr_node_addr); // chk
#else
            rx_id = my_tsch_ost_id;
#endif
          }

          struct tsch_slotframe *sf = tsch_schedule_get_slotframe_by_handle(current_link->slotframe_handle);
          if(sf != NULL) {
            uint64_t ASN = (uint64_t)(tsch_current_asn.ls4b) + ((uint64_t)(tsch_current_asn.ms1b) << 32);
            uint64_t ASFN = ASN / sf->size.val;
            uint16_t hash_input = (uint16_t)(rx_id + ASFN);
            uint16_t minus_c_offset = hash_ftn(hash_input, 2) ; /* 0 or 1 */
            current_link->channel_offset = 3; /* default: 3 */
            current_link->channel_offset = current_link->channel_offset - minus_c_offset; /* 3 - 0 or 3 - 1 */
          } else {
            goto ost_donothing;
          }
        }
#if OST_ON_DEMAND_PROVISION 
        else if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET) {
          if(current_link->link_options & LINK_OPTION_TX) {
            rx_id = (current_link->slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
            if(current_packet == NULL) {
              /* ERROR: multi_channel 4 */
            }
          } else if(current_link->link_options & LINK_OPTION_RX) {
#if !WITH_OST_OID
            rx_id = ost_node_id_from_linkaddr(&linkaddr_node_addr); // chk
#else
            rx_id = my_tsch_ost_id;
#endif

          } else {
            /* ERROR: multi_channel 5 */
          }
          uint64_t ASN = (uint64_t)(tsch_current_asn.ls4b) + ((uint64_t)(tsch_current_asn.ms1b) << 32);
          if(ASN % ORCHESTRA_CONF_COMMON_SHARED_PERIOD == 0) {
            // No action????
            /* Shared slotframe should have not been overlapped.
            Of course, not allowed to overlaped with the other slotframes */
            /* ERROR: multi_channel 6 */
          }
          uint16_t hash_input = rx_id + (uint16_t)ASN;
          uint16_t minus_c_offset = hash_ftn(hash_input, 2) ; // 0 or 1
          current_link->channel_offset = 3; // default
          current_link->channel_offset = current_link->channel_offset - minus_c_offset; // 3-0 or 3-1

#if WITH_OST_LOG
          TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "ost odp: operation");
          );
#endif

        }
#endif
#endif

        /* If we are in a burst, we stick to current channel instead of
         * doing channel hopping, as per IEEE 802.15.4-2015 */
        tsch_current_timeslot = current_link->timeslot; // hckim
#if !WITH_OST && !WITH_ALICE
        if(burst_link_scheduled) {
          /* Reset burst_link_scheduled flag. Will be set again if burst continue. */
          burst_link_scheduled = 0;
        } else 
#endif
        {
          /* Hop channel */
          tsch_current_channel_offset = tsch_get_channel_offset(current_link, current_packet);
          tsch_current_channel = tsch_calculate_channel(&tsch_current_asn, tsch_current_channel_offset);
        }
        NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, tsch_current_channel);
        /* Turn the radio on already here if configured so; necessary for radios with slow startup */
        tsch_radio_on(TSCH_RADIO_CMD_ON_START_OF_TIMESLOT);
        /* Decide whether it is a TX/RX/IDLE or OFF slot */
        /* Actual slot operation */
        if(current_packet != NULL) {
          /* We have something to transmit, do the following:
           * 1. send
           * 2. update_backoff_state(current_neighbor)
           * 3. post tx callback
           **/
          static struct pt slot_tx_pt;
          PT_SPAWN(&slot_operation_pt, &slot_tx_pt, tsch_tx_slot(&slot_tx_pt, t));
        } else {
          /* Listen */
          static struct pt slot_rx_pt;
          PT_SPAWN(&slot_operation_pt, &slot_rx_pt, tsch_rx_slot(&slot_rx_pt, t));
        }
      } else {
        ++tsch_unlocked_scheduled_idle_cell_count; //hckim
#if !WITH_OST && !WITH_ALICE
        /* Make sure to end the burst in cast, for some reason, we were
         * in a burst but now without any more packet to send. */
        burst_link_scheduled = 0;
#endif
      }

#if WITH_OST
ost_donothing:
#endif

      TSCH_DEBUG_SLOT_END();
    }

    /* End of slot operation, schedule next slot or resynchronize */

    if(tsch_is_coordinator) {
      /* Update the `last_sync_*` variables to avoid large errors
       * in the application-level time synchronization */
      last_sync_asn = tsch_current_asn;
      tsch_last_sync_time = clock_time();
    }

    /* Do we need to resynchronize? i.e., wait for EB again */
    if(!tsch_is_coordinator && (TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn) >
        (100 * TSCH_CLOCK_TO_SLOTS(TSCH_DESYNC_THRESHOLD / 100, tsch_timing[tsch_ts_timeslot_length])))) {
      TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "! leaving the network, last sync %u",
                          (unsigned)TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn));
      );
      tsch_disassociate();
    } else {
      /* backup of drift correction for printing debug messages */
      /* int32_t drift_correction_backup = drift_correction; */
      uint16_t timeslot_diff = 0;
      rtimer_clock_t prev_slot_start;
      /* Time to next wake up */
      rtimer_clock_t time_to_next_active_slot;
      /* Schedule next wakeup skipping slots if missed deadline */
      do {
        if(current_link != NULL
            && current_link->link_options & LINK_OPTION_TX
            && current_link->link_options & LINK_OPTION_SHARED) {
          /* Decrement the backoff window for all neighbors able to transmit over
           * this Tx, Shared link. */
          tsch_queue_update_all_backoff_windows(&current_link->addr);
        }

#if !WITH_OST && !WITH_ALICE
        /* A burst link was scheduled. Replay the current link at the
        next time offset */
        if(burst_link_scheduled && current_link != NULL) {
          timeslot_diff = 1;
          backup_link = NULL;
          /* Keep track of the number of repetitions */
          tsch_current_burst_count++;
        } else {
          /* Get next active link */
          current_link = tsch_schedule_get_next_active_link(&tsch_current_asn, &timeslot_diff, &backup_link);
          if(current_link == NULL) {
            /* There is no next link. Fall back to default
             * behavior: wake up at the next slot. */
            timeslot_diff = 1;
          } else {
            /* Reset burst index now that the link was scheduled from
              normal schedule (as opposed to from ongoing burst) */
            tsch_current_burst_count = 0;
          }
        }
#else
        /* Get next active link */
        current_link = tsch_schedule_get_next_active_link(&tsch_current_asn, &timeslot_diff, &backup_link);
        if(current_link == NULL) {
          /* There is no next link. Fall back to default
           * behavior: wake up at the next slot. */
          timeslot_diff = 1;
        } else {
          /* Reset burst index now that the link was scheduled from
            normal schedule (as opposed to from ongoing burst) */
          tsch_current_burst_count = 0;
        }
#endif

        /* Update ASN */
        TSCH_ASN_INC(tsch_current_asn, timeslot_diff);

#if WITH_OST && OST_ON_DEMAND_PROVISION
        if(current_link == NULL && tsch_is_locked() 
          && exist_matching_slot(&tsch_current_asn)) { /* tsch-schedule is being changing, so locked */
          remove_matching_slot();
        }
#endif        

        /* Time to next wake up */
        time_to_next_active_slot = timeslot_diff * tsch_timing[tsch_ts_timeslot_length] + drift_correction;
        time_to_next_active_slot += tsch_timesync_adaptive_compensate(time_to_next_active_slot);
        drift_correction = 0;
        is_drift_correction_used = 0;
        /* Update current slot start */
        prev_slot_start = current_slot_start;
        current_slot_start += time_to_next_active_slot;
      } while(!tsch_schedule_slot_operation(t, prev_slot_start, time_to_next_active_slot, "main"));
    }

    tsch_in_slot_operation = 0;
    PT_YIELD(&slot_operation_pt);
  }

  PT_END(&slot_operation_pt);
}
/*---------------------------------------------------------------------------*/
/* Set global time before starting slot operation,
 * with a rtimer time and an ASN */
void
tsch_slot_operation_start(void)
{
  static struct rtimer slot_operation_timer;
  rtimer_clock_t time_to_next_active_slot;
  rtimer_clock_t prev_slot_start;
  TSCH_DEBUG_INIT();
  do {
    uint16_t timeslot_diff;
    /* Get next active link */
    current_link = tsch_schedule_get_next_active_link(&tsch_current_asn, &timeslot_diff, &backup_link);
    if(current_link == NULL) {
      /* There is no next link. Fall back to default
       * behavior: wake up at the next slot. */
      timeslot_diff = 1;
    }
    /* Update ASN */
    TSCH_ASN_INC(tsch_current_asn, timeslot_diff);
    /* Time to next wake up */
    time_to_next_active_slot = timeslot_diff * tsch_timing[tsch_ts_timeslot_length];
    /* Compensate for the base drift */
    time_to_next_active_slot += tsch_timesync_adaptive_compensate(time_to_next_active_slot);
    /* Update current slot start */
    prev_slot_start = current_slot_start;
    current_slot_start += time_to_next_active_slot;
  } while(!tsch_schedule_slot_operation(&slot_operation_timer, prev_slot_start, time_to_next_active_slot, "assoc"));
}
/*---------------------------------------------------------------------------*/
/* Start actual slot operation */
void
tsch_slot_operation_sync(rtimer_clock_t next_slot_start,
    struct tsch_asn_t *next_slot_asn)
{
  int_master_status_t status;

  current_slot_start = next_slot_start;
  tsch_current_asn = *next_slot_asn;
  status = critical_enter();
  last_sync_asn = tsch_current_asn;
  tsch_last_sync_time = clock_time();
  critical_exit(status);
  current_link = NULL;
}
/*---------------------------------------------------------------------------*/
/** @} */
