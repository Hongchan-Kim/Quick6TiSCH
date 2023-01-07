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

#if WITH_OST /* OST: Include header files */
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

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
/* Indicates whether an extra link is needed to handle the current burst */
static int burst_link_scheduled = 0;
static int is_burst_slot = 0;
#if TSCH_DBT_TEMPORARY_LINK
struct tsch_link temporal_burst_link;
#endif
#if TSCH_DBT_HANDLE_MISSED_DBT_SLOT
static int burst_link_scheduled_count = 0;
#endif
#if TSCH_DBT_HOLD_CURRENT_NBR
static int burst_link_tx = 0;
static int burst_link_rx = 0;
#endif
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

#if HCK_DBG_REGULAR_SLOT_DETAIL
/* Whether to log tx or rx slot details */
static uint8_t regular_slot_is_tx; /* Even when there is no current_packet to send */
static uint8_t regular_slot_is_rx;

static uint8_t regular_slot_tx_packet_len;
static uint8_t regular_slot_tx_do_wait_for_ack;
static uint8_t regular_slot_tx_mac_tx_status;
static uint8_t regular_slot_tx_ack_len;

static uint8_t regular_slot_rx_packet_len;
static uint8_t regular_slot_rx_ack_required;
static uint8_t regular_slot_rx_ack_len;
static rtimer_clock_t regular_slot_rx_estimated_drift;

#if HCK_DBG_REGULAR_SLOT_TIMING /* Variables */
static rtimer_clock_t regular_slot_timestamp_common[2];
static rtimer_clock_t regular_slot_timestamp_tx[16];
static rtimer_clock_t regular_slot_timestamp_rx[14];
#endif /* HCK_DBG_REGULAR_SLOT_TIMING */
#endif /* HCK_DBG_REGULAR_SLOT_DETAIL */

#if HCK_ASAP_EVAL_02_UPA_SINGLE_HOP
#if FIXED_NUM_OF_AGGREGATED_PKTS > 0
uint8_t eval_01_num_of_pkts_aggregated = FIXED_NUM_OF_AGGREGATED_PKTS;
#else
uint8_t eval_01_num_of_pkts_aggregated = 1;
#endif
#endif

#if WITH_ASAP
static rtimer_clock_t asap_curr_slot_start;
static rtimer_clock_t asap_curr_slot_end;
static uint16_t asap_curr_passed_timeslots; /* Includes the first (triggering) slot */
static uint16_t asap_curr_passed_timeslots_except_first_slot;
static uint16_t asap_timeslot_diff_at_the_end;
#endif

#if WITH_UPA
/* In both slots */
static int upa_link_scheduled;
static int upa_link_finished;
static uint8_t upa_schedule_after_upa_link;
static uint8_t upa_link_result_case;

/* In Tx slot */
static uint8_t upa_expected_passed_timeslots_except_first_slot;
static uint8_t upa_link_requested;
static uint8_t upa_tx_slot_pending_tsch_radio_off;
static uint8_t upa_tx_slot_pending_stats_and_log_update;

static uint8_t upa_tx_slot_trig_packet_len;
static uint8_t upa_tx_slot_all_packet_len_same;
static uint8_t upa_tx_slot_trig_ack_len;

static uint8_t upa_tx_slot_dequeued_ringbuf_index_array[TSCH_DEQUEUED_ARRAY_SIZE];
static uint8_t upa_tx_slot_in_queue_array[TSCH_DEQUEUED_ARRAY_SIZE];
struct tsch_packet *upa_tx_slot_packet_array[TSCH_DEQUEUED_ARRAY_SIZE];

static int upa_pkts_to_send;
static uint16_t upa_tx_slot_in_batch_seq;
static uint8_t upa_tx_slot_mac_tx_status;
static struct tsch_packet *upa_tx_slot_curr_packet = NULL;
static void *upa_tx_slot_curr_packet_payload;
static uint8_t upa_tx_slot_curr_packet_len;
static uint8_t upa_tx_slot_b_ack_seen;
static uint16_t upa_tx_slot_batch_tx_ok_count;

static rtimer_clock_t upa_tx_slot_trig_ack_start_time;
static rtimer_clock_t upa_tx_slot_trig_ack_duration;
static rtimer_clock_t upa_tx_slot_batch_tx_start_time;

static rtimer_clock_t upa_tx_slot_curr_start;
static rtimer_clock_t upa_tx_slot_curr_offset;
static rtimer_clock_t upa_tx_slot_curr_duration;

static rtimer_clock_t upa_tx_slot_prev_start;
static rtimer_clock_t upa_tx_slot_prev_offset;
static rtimer_clock_t upa_tx_slot_prev_duration;

/* In Rx slot */
static int upa_pkts_to_receive;
static uint8_t upa_num_of_slots_required[TSCH_MAX_INCOMING_PACKETS];

static uint8_t upa_rx_slot_trig_packet_len;
static uint8_t upa_rx_slot_all_packet_len_same;
static int upa_rx_slot_trig_ack_len;

struct tsch_neighbor *upa_rx_slot_nbr;
static uint16_t upa_rx_slot_batch_rx_ok_count;
static uint8_t upa_rx_slot_last_in_batch_seq;
static uint16_t upa_rx_slot_b_ack_bitmap;
static int16_t upa_rx_slot_curr_input_ringbuf_index;
static struct input_packet *upa_rx_slot_curr_input;
static int upa_rx_slot_header_len;
static int upa_rx_slot_frame_valid;
static frame802154_t upa_rx_slot_frame;
static linkaddr_t upa_rx_slot_src_addr;
static linkaddr_t upa_rx_slot_dest_addr;
static uint8_t upa_rx_slot_b_ack_buf[TSCH_PACKET_MAX_LEN];
static int upa_rx_slot_b_ack_len;

static rtimer_clock_t upa_rx_slot_trig_packet_start_time;
static rtimer_clock_t upa_rx_slot_trig_packet_duration;
static rtimer_clock_t upa_rx_slot_trig_ack_start_time;
static rtimer_clock_t upa_rx_slot_trig_ack_duration;
static rtimer_clock_t upa_rx_slot_batch_rx_start_time;

static rtimer_clock_t upa_rx_slot_curr_start;
static rtimer_clock_t upa_rx_slot_curr_offset;
static rtimer_clock_t upa_rx_slot_curr_reception_start;
static rtimer_clock_t upa_rx_slot_curr_duration;
static rtimer_clock_t upa_rx_slot_curr_timeout;
static rtimer_clock_t upa_rx_slot_curr_deadline;

static rtimer_clock_t upa_rx_slot_last_valid_reception_start;
static rtimer_clock_t upa_rx_slot_last_valid_duration;

static rtimer_clock_t upa_rx_slot_all_reception_end;

#if UPA_DBG_TIMING_TRIPLE_CCA
static uint8_t is_upa_triple_cca;
static rtimer_clock_t upa_timestamp_triple_cca[6];
#endif

#if UPA_DBG_SLOT_TIMING /* Variables */
static uint8_t upa_print_tx_slot_timing;
static rtimer_clock_t upa_tx_slot_timestamp_begin[2];
static rtimer_clock_t upa_tx_slot_timestamp_tx[TSCH_DEQUEUED_ARRAY_SIZE][4];
static rtimer_clock_t upa_tx_slot_timestamp_ack[4];
static rtimer_clock_t upa_tx_slot_timestamp_end[3];

static uint8_t upa_print_rx_slot_timing;
static rtimer_clock_t upa_rx_slot_timestamp_begin[3];
static rtimer_clock_t upa_rx_slot_timestamp_rx[TSCH_MAX_INCOMING_PACKETS][4];
static rtimer_clock_t upa_rx_slot_timestamp_ack[4];
static rtimer_clock_t upa_rx_slot_timestamp_end[2];
#endif

#endif /* WITH_UPA */

#if WITH_SLA /* Variables */
static volatile int sla_in_guard_time;
#endif

static rtimer_clock_t current_slot_unused_offset_start;
static rtimer_clock_t current_slot_unused_offset_end;
static rtimer_clock_t current_slot_unused_offset_time;
static rtimer_clock_t current_slot_idle_start;
static rtimer_clock_t current_slot_busy_time;
static rtimer_clock_t current_slot_idle_time;
static uint8_t current_slot_passed_slots;
static uint16_t asap_tot_packet_len;
static uint16_t asap_ok_packet_len;
static uint8_t asap_tot_ack_len;

/*---------------------------------------------------------------------------*/
struct tsch_asn_t tsch_last_valid_asn;
static rtimer_clock_t volatile last_valid_asn_start;
/*---------------------------------------------------------------------------*/
uint64_t
tsch_calculate_current_asn()
{
  rtimer_clock_t now = RTIMER_NOW();

  uint64_t last_valid_asn = (uint64_t)(tsch_last_valid_asn.ls4b) + ((uint64_t)(tsch_last_valid_asn.ms1b) << 32);

  uint16_t passed_timeslots = ((RTIMER_CLOCK_DIFF(now, last_valid_asn_start) + tsch_timing[tsch_ts_timeslot_length] - 1) 
                        / tsch_timing[tsch_ts_timeslot_length]);

  return last_valid_asn + passed_timeslots - 1;
}
/*---------------------------------------------------------------------------*/

#if WITH_OST /* OST-00-03: struct for t_offset */
typedef struct ost_t_offset_candidate {
  uint8_t installable;
} ost_t_offset_candidate_t;
#endif

#if WITH_OST
uint16_t
ost_hash_ftn(uint16_t value, uint16_t mod) {
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
struct ost_ssq_schedule_t ost_ssq_schedule_list[16];
/*---------------------------------------------------------------------------*/
uint16_t
ost_select_matching_schedule(uint16_t rx_schedule_info)
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
ost_print_ssq_schedule_list(void)
{
/*
  PRINTF("[SSQ_SCHEDULE] index / asn / option\n");
  uint8_t i;
  uint16_t nbr_id;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == 0 && ost_ssq_schedule_list[i].asn.ms1b == 0) {
    } else {
      if(ost_ssq_schedule_list[i].link.link_options == LINK_OPTION_TX) {
        nbr_id = (ost_ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
        //PRINTF("[ID:%u] %u / %x.%lx / Tx %u\n", nbr_id, i, ost_ssq_schedule_list[i].asn.ms1b, ost_ssq_schedule_list[i].asn.ls4b,ost_ssq_schedule_list[i].link.slotframe_handle);
      } else {
        nbr_id = (ost_ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 2) / 2;
        //PRINTF("[ID:%u] %u / %x.%lx / Rx %u\n", nbr_id, i, ost_ssq_schedule_list[i].asn.ms1b, ost_ssq_schedule_list[i].asn.ls4b, ost_ssq_schedule_list[i].link.slotframe_handle);
      }
    }
  }
*/
}
/*---------------------------------------------------------------------------*/
uint8_t
ost_exist_matching_slot(struct tsch_asn_t *target_asn)
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == target_asn->ls4b 
      && ost_ssq_schedule_list[i].asn.ms1b == target_asn->ms1b) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
ost_remove_matching_slot(void)
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == tsch_current_asn.ls4b 
      && ost_ssq_schedule_list[i].asn.ms1b == tsch_current_asn.ms1b) {
      ost_ssq_schedule_list[i].asn.ls4b = 0;
      ost_ssq_schedule_list[i].asn.ms1b = 0;
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
ost_add_matching_slot(uint16_t matching_slot, uint8_t is_tx, uint16_t nbr_id)
{  
  if(1 <= matching_slot && matching_slot <= 16) {
    uint8_t i;
    for(i = 0; i < 16; i++) {
      if(ost_ssq_schedule_list[i].asn.ls4b == 0 && ost_ssq_schedule_list[i].asn.ms1b == 0) {
        ost_ssq_schedule_list[i].asn.ls4b = tsch_current_asn.ls4b;
        ost_ssq_schedule_list[i].asn.ms1b = tsch_current_asn.ms1b;
        TSCH_ASN_INC(ost_ssq_schedule_list[i].asn, matching_slot);

        ost_ssq_schedule_list[i].link.next = NULL;        //not used
        ost_ssq_schedule_list[i].link.handle = 0xffff;    //not used
        ost_ssq_schedule_list[i].link.timeslot = 0xffff;  //not used

        ost_ssq_schedule_list[i].link.channel_offset = 3; //will be updated using hash before TX or RX

        ost_ssq_schedule_list[i].link.link_type = LINK_TYPE_NORMAL;
        ost_ssq_schedule_list[i].link.data = NULL;

        linkaddr_copy(&(ost_ssq_schedule_list[i].link.addr), &tsch_broadcast_address);

        if(is_tx) {
          ost_ssq_schedule_list[i].link.slotframe_handle = SSQ_SCHEDULE_HANDLE_OFFSET + 2 * nbr_id + 1;
          ost_ssq_schedule_list[i].link.link_options = LINK_OPTION_TX;

#if WITH_OST_LOG_DBG
          TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "ost odp: add tx %u", ost_ssq_schedule_list[i].link.slotframe_handle);
          );
#endif

        } else {
          ost_ssq_schedule_list[i].link.slotframe_handle = SSQ_SCHEDULE_HANDLE_OFFSET + 2 * nbr_id + 2;
          ost_ssq_schedule_list[i].link.link_options = LINK_OPTION_RX;

#if WITH_OST_LOG_DBG
          TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "ost odp: add rx %u", ost_ssq_schedule_list[i].link.slotframe_handle);
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
ost_process_rx_schedule_info(frame802154_t* frame) 
{
  uint16_t nbr_id = OST_NODE_ID_FROM_LINKADDR((linkaddr_t *)(&(frame->src_addr)));
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)(&(frame->src_addr)));
  if(nbr != NULL
    && !frame802154_is_broadcast_addr((frame->fcf).dest_addr_mode, frame->dest_addr)
    && ost_is_routing_nbr(nbr) == 1) {
    if((frame->fcf).frame_pending) {
      uint16_t time_to_matching_slot = ost_select_matching_schedule(frame->ost_pigg2);

      if(time_to_matching_slot == 0xffff) { /* 0xffff */
        /* no matching slots */
      } else if(1 <= time_to_matching_slot && (time_to_matching_slot - 1) < 16) {
        /* matching slot at time_to_matching slot */
        ost_add_matching_slot(time_to_matching_slot, 0, nbr_id);
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
ost_process_rx_matching_slot(frame802154_t* frame)
{
  linkaddr_t *eack_src = tsch_queue_get_nbr_address(current_neighbor);
  uint16_t src_id = OST_NODE_ID_FROM_LINKADDR(eack_src);
  uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)eack_src);
  if(nbr != NULL && ost_is_routing_nbr(nbr) == 1) {
    ost_add_matching_slot(frame->ost_pigg2, 1, src_id);
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
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST /* OST-06-02: Select t_offset */
void
ost_eliminate_overlap_toc(ost_t_offset_candidate_t *toc, uint16_t target_N, 
                      uint16_t used_N, uint16_t used_t_offset)
{
  if(target_N < used_N) { /* lower-tier used */
    uint16_t parent_N = used_N - 1;
    uint16_t parent_t_offset = used_t_offset % (1 << (used_N - 1)); 

    ost_eliminate_overlap_toc(toc, target_N, parent_N, parent_t_offset);

  } else if (target_N > used_N) { /* higher-tier used */
    uint16_t child_N = used_N + 1;
    uint16_t left_child_t_offset = used_t_offset;
    uint16_t right_child_t_offset = used_t_offset + (1 << used_N);

    ost_eliminate_overlap_toc(toc, target_N, child_N, left_child_t_offset);
    ost_eliminate_overlap_toc(toc, target_N, child_N, right_child_t_offset);

  } else { /* target_N == used_N */
    if(used_t_offset >= (1 << target_N)) {
      /* ERR: too big used t_offset */
      return;
    }
    toc[used_t_offset].installable = 0;
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST /* OST-06-01: Select t_offset */
uint32_t 
ost_select_t_offset(uint16_t target_id, uint16_t target_N)  /* similar with ost_tx_installable */
{
  /* check tsch_is_locked() in the caller func, ost_process_rx_N */
  ost_t_offset_candidate_t toc[1 << OST_N_MAX];

  /* initialize 2^N toc */
  uint16_t i;
  for(i = 0; i < (1 << target_N); i++) {
    toc[i].installable = 1;
  }

  /* Check resource overlap with schedule of rx pending queue */
  uint16_t new_schedule_nodes_in_input_array[TSCH_MAX_INCOMING_PACKETS];
  uint8_t new_schedule_num_in_input_array = 0;
  for(i = 0; i < TSCH_MAX_INCOMING_PACKETS; i++) {
    new_schedule_nodes_in_input_array[i] = 0;
  }

  int16_t input_index = ringbufindex_peek_get(&input_ringbuf);
  if(input_index != -1) {
    uint8_t num_input_elements = ringbufindex_elements(&input_ringbuf);  

    uint8_t j;
    for(j = input_index; j <input_index + num_input_elements; j++) {
      int16_t actual_input_index;

      if(j >= ringbufindex_size(&input_ringbuf)) {
        actual_input_index = j - ringbufindex_size(&input_ringbuf);
      } else {
        actual_input_index = j;  
      }

      struct input_packet *input_p = &input_array[actual_input_index];
      if(input_p->ost_flag_change_rx_schedule == 1
         && input_p->ost_prN_nbr != NULL) {
        uint8_t pending_id = OST_NODE_ID_FROM_IPADDR(uip_ds6_nbr_get_ipaddr(input_p->ost_prN_nbr));
        if(target_id != pending_id) {
          uint16_t pending_N = (input_p->ost_prN_nbr)->ost_nbr_N;
          uint16_t pending_t_offset = (input_p->ost_prN_nbr)->ost_nbr_t_offset;

          ost_eliminate_overlap_toc(toc, target_N, pending_N, pending_t_offset);

          new_schedule_nodes_in_input_array[new_schedule_num_in_input_array] = pending_id;
          new_schedule_num_in_input_array++;
        }
      }
    }
  }

  /* Check resource overlap with schedule of tx pending queue */
  uint16_t new_schedule_nodes_in_dequeued_array[TSCH_DEQUEUED_ARRAY_SIZE];
  uint8_t new_schedule_num_in_dequeued_array = 0;
  for(i = 0; i < TSCH_DEQUEUED_ARRAY_SIZE; i++) {
    new_schedule_nodes_in_dequeued_array[i] = 0;
  }

  int16_t dequeued_index = ringbufindex_peek_get(&dequeued_ringbuf);
  if(dequeued_index != -1) {
    uint8_t num_dequeued_elements = ringbufindex_elements(&dequeued_ringbuf);  

    uint8_t j;
    for(j = dequeued_index; j < dequeued_index + num_dequeued_elements; j++) {
      int16_t actual_dequeued_index;

      if(j >= ringbufindex_size(&dequeued_ringbuf)) { /* default size: 16 */
        actual_dequeued_index = j - ringbufindex_size(&dequeued_ringbuf);
      } else {
        actual_dequeued_index = j;  
      }

      struct tsch_packet *dequeued_p = dequeued_array[actual_dequeued_index];

      if(dequeued_p->ost_flag_change_tx_schedule == 1
         && dequeued_p->ost_prt_nbr != NULL
         && (dequeued_p->ost_prt_nbr)->ost_my_installable == 1 
         && dequeued_p->ost_flag_rejected_by_nbr == 0) {
        uint8_t pending_id = OST_NODE_ID_FROM_IPADDR(uip_ds6_nbr_get_ipaddr(dequeued_p->ost_prt_nbr));
        uint16_t pending_N = (dequeued_p->ost_prt_nbr)->ost_my_N;
        uint16_t pending_t_offset = (dequeued_p->ost_prt_nbr)->ost_my_t_offset;

        ost_eliminate_overlap_toc(toc, target_N, pending_N, pending_t_offset);

        new_schedule_nodes_in_dequeued_array[new_schedule_num_in_dequeued_array] = pending_id;
        new_schedule_num_in_dequeued_array++;
      }
    }
  }

  /* Check resource overlap with ongoing schedule */
  struct tsch_slotframe *sf = ost_tsch_schedule_get_slotframe_head();
  while(sf != NULL) {
    /* periodic provisioning slotframes, except for rx slotframe for target_id 
       do not need to check rx slotframe for target_id (will be re-installed, if overlapped) */
    if(sf->handle > 2 && sf->handle != ost_get_rx_sf_handle_from_id(target_id)) {

      uint8_t already_eliminated = 0;
      uint8_t k;
      for(k = 0; k < new_schedule_num_in_input_array; k++) {
        if(sf->handle == ost_get_rx_sf_handle_from_id(new_schedule_nodes_in_input_array[k])) {
          already_eliminated = 1;
          break;
        }
      }
      if(already_eliminated == 0) {
        for(k = 0; k < new_schedule_num_in_dequeued_array; k++) {
          if(sf->handle == ost_get_tx_sf_handle_from_id(new_schedule_nodes_in_dequeued_array[k])) {
            already_eliminated = 1;
            break;
          }
        }
      }

      if(already_eliminated == 0) {
        uint16_t n;
        for(n = 1; n <= OST_N_MAX; n++) {
          if((sf->size.val >> n) == 1) {
            uint16_t used_N = n;
            struct tsch_link *l = list_head(sf->links_list);

            if(l != NULL) {
              uint16_t used_t_offset = l->timeslot;
              ost_eliminate_overlap_toc(toc, target_N, used_N, used_t_offset);
            }

            break;
          }
        }
      }
    }
    sf = list_item_next(sf);
  }

  uint32_t rand = random_rand() % (1 << 31); /* OST check later */
  uint32_t j = 0;

  for(i = 0; i < (1 << target_N); i++) {
    j = (i + rand) % (1 << target_N);
    if(toc[j].installable == 1) {
      break;
    }
  }

  if(i == (1 << target_N)) { /* failed to select t_offset */
#if WITH_OST_LOG_INFO
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost sel_T: n %u N %u -> fail", target_id, target_N);
    );
#endif
    return 0xffff;
  } else {
#if WITH_OST_LOG_INFO
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost sel_T: n %u N %u -> %lu", target_id, target_N, j);
    );
#endif
    return j;
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST /* OST-05-02: Process received N */
 /* in short, prN */
void
ost_process_rx_N(frame802154_t *frame, struct input_packet *current_input)
{
  /* initialize OST variables and flags in current_input */
  uip_ds6_nbr_t *ds6_nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)(&(frame->src_addr)));
  if(!frame802154_is_broadcast_addr((frame->fcf).dest_addr_mode, frame->dest_addr)
     && ds6_nbr != NULL
     && ost_is_routing_nbr(ds6_nbr) == 1
     && ds6_nbr->ost_rx_no_path == 0) {
    current_input->ost_prN_nbr = ds6_nbr;
    current_input->ost_prN_new_N = ds6_nbr->ost_nbr_N;
    current_input->ost_prN_new_t_offset = ds6_nbr->ost_nbr_t_offset;
    current_input->ost_flag_change_rx_schedule = 0;
    current_input->ost_flag_failed_to_select_t_offset = 0;
    current_input->ost_flag_respond_to_consec_new_tx_sched_req = 0;
  }

  if(tsch_is_locked()) {
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost_process_rx_N: locked");
    );
    return;
  }

  if(!frame802154_is_broadcast_addr((frame->fcf).dest_addr_mode, frame->dest_addr)
     && ds6_nbr != NULL
     && ost_is_routing_nbr(ds6_nbr) == 1
     && ds6_nbr->ost_rx_no_path == 0) {
    /* No need to allocate rx schedule for ds6_nbr who sent no-path dao */
    uint16_t ds6_nbr_id = OST_NODE_ID_FROM_LINKADDR((linkaddr_t *)(&(frame->src_addr)));

#if WITH_OST_LOG_DBG
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost rcvd N: n %u N %u -> %u", ds6_nbr_id, ds6_nbr->ost_nbr_N, frame->ost_pigg1);
    );
#endif

    if(ds6_nbr->ost_nbr_N == frame->ost_pigg1) { /* received same N */
      ds6_nbr->ost_consecutive_new_tx_schedule_request = 0;
     } else { /* received different N */
      /* Set variables to be used in ost_post_process_rx_N func */
      if(frame->ost_pigg1 >= OST_N_OFFSET_NEW_TX_REQUEST) {
        ds6_nbr->ost_consecutive_new_tx_schedule_request++;
        current_input->ost_prN_new_N = (frame->ost_pigg1) - OST_N_OFFSET_NEW_TX_REQUEST;

#if WITH_OST_LOG_INFO
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prN: non-inst/lowPRR %u", 
                    ds6_nbr->ost_consecutive_new_tx_schedule_request);
        );
#endif

        if(ds6_nbr->ost_consecutive_new_tx_schedule_request >= OST_THRES_CONSECUTIVE_NEW_TX_SCHEDULE_REQUEST) {
          ds6_nbr->ost_consecutive_new_tx_schedule_request = 0;
          current_input->ost_flag_respond_to_consec_new_tx_sched_req = 1;
          return;
        }
      } else {
        ds6_nbr->ost_consecutive_new_tx_schedule_request = 0;
        current_input->ost_prN_new_N = frame->ost_pigg1;
      }

      uint32_t selected_t_offset = ost_select_t_offset(ds6_nbr_id, current_input->ost_prN_new_N);

      if(selected_t_offset < 0xffff) {
        current_input->ost_flag_change_rx_schedule = 1;
        current_input->ost_prN_new_t_offset = (uint16_t)selected_t_offset;
        /* update N and t_off of ds6_nbr only when successfully selected t_off */
        ds6_nbr->ost_nbr_N = current_input->ost_prN_new_N; 
        ds6_nbr->ost_nbr_t_offset = current_input->ost_prN_new_t_offset;        
        return;  
      } else {
        /* do not update ds6_nbr */
        current_input->ost_flag_failed_to_select_t_offset = 1;
        return;
      }
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST /* OST-09: Post process received N */
/* called from tsch_rx_process_pending (after slot_operation finished) */
void
ost_post_process_rx_N(struct input_packet *current_input)
{
  if(current_input->ost_flag_change_rx_schedule == 1) {
    current_input->ost_flag_change_rx_schedule = 0;

    if(current_input->ost_prN_nbr != NULL) {
      uint16_t nbr_id = OST_NODE_ID_FROM_IPADDR(&(current_input->ost_prN_nbr->ipaddr));
      ost_remove_rx(nbr_id);
      ost_add_rx(nbr_id, current_input->ost_prN_new_N, current_input->ost_prN_new_t_offset);

#if WITH_OST_LOG_INFO
      TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ost pprN: n %u N %u T %u", nbr_id, current_input->ost_prN_new_N, current_input->ost_prN_new_t_offset);
      );
#endif

#if WITH_OST_LOG_NBR
      ost_print_nbr();
#endif
#if WITH_OST_LOG_SCH
      tsch_schedule_print_ost();
#endif

    }
  }

  if(current_input->ost_flag_respond_to_consec_new_tx_sched_req == 1) {
    current_input->ost_flag_respond_to_consec_new_tx_sched_req = 0;
    /* No rx schedule change */
  }

  if(current_input->ost_flag_failed_to_select_t_offset == 1) {
    current_input->ost_flag_failed_to_select_t_offset = 0;
    /* No rx schedule change */
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST
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

        ost_set_queuebuf_attr(n->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_SLOTFRAME, handle);
        ost_set_queuebuf_attr(n->tx_array[index]->qb, PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void 
ost_add_tx(linkaddr_t *nbr_lladdr, uint16_t N, uint16_t t_offset)
{
  if(N != 0xffff && t_offset != 0xffff) {
    uint16_t id = OST_NODE_ID_FROM_LINKADDR(nbr_lladdr);
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
      uip_ds6_nbr_t *nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)nbr_lladdr);
      if(nbr != NULL) {
        if(nbr->ost_my_low_prr == 1) {
          ost_update_N_of_packets_in_queue(nbr_lladdr, nbr->ost_my_N);
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
  uint16_t id = OST_NODE_ID_FROM_LINKADDR(nbr_lladdr);
  struct tsch_slotframe *rm_sf;
  uint16_t rm_sf_handle = ost_get_tx_sf_handle_from_id(id);
  rm_sf = tsch_schedule_get_slotframe_by_handle(rm_sf_handle);

  if(rm_sf != NULL) {
    tsch_schedule_remove_slotframe(rm_sf);
    struct tsch_neighbor *n = tsch_queue_get_nbr(nbr_lladdr);
    if(n != NULL) {
      if(!tsch_queue_is_empty(n)) {
        if(neighbor_has_uc_link(tsch_queue_get_nbr_address(n))) {
          ost_change_queue_select_packet(nbr_lladdr, 1, ORCHESTRA_LINKADDR_HASH(nbr_lladdr) % ORCHESTRA_UNICAST_PERIOD); //Use RB

        } else {
          ost_change_queue_select_packet(nbr_lladdr, 2, 0); //Use shared slot
        }
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
ost_has_my_N_changed(uip_ds6_nbr_t * nbr)
{
  /* check whether to match nbr->my_N and installed tx_sf */
  uint16_t nbr_id = OST_NODE_ID_FROM_IPADDR(&(nbr->ipaddr));
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
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST /* Process received t_offset */
int8_t
ost_tx_installable(uint16_t target_id, uint16_t N, uint16_t t_offset)
{
  /* similar with ost_select_t_offset */
  if(tsch_is_locked()) {
    TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "ost_tx_installable: locked");
    );
    return -1;
  }

  if(t_offset == 0xffff) { /* denial from eack_src */
    return -2;
  }

  ost_t_offset_candidate_t toc[1 << OST_N_MAX];

  if(t_offset < (1 << N) ) {
    toc[t_offset].installable = 1;
  } else { /* non-installable (too big t_offset) */
    return -1;
  }

  /* Check resource overlap with schedule of rx pending queue */
  uint8_t i;
  uint16_t new_schedule_nodes_in_input_array[TSCH_MAX_INCOMING_PACKETS];
  uint8_t new_schedule_num_in_input_array = 0;
  for(i = 0; i < TSCH_MAX_INCOMING_PACKETS; i++) {
    new_schedule_nodes_in_input_array[i] = 0;
  }

  int16_t input_index = ringbufindex_peek_get(&input_ringbuf);
  if(input_index != -1) {
    uint8_t num_input_elements = ringbufindex_elements(&input_ringbuf);  

    uint8_t j;
    for(j = input_index; j <input_index + num_input_elements; j++) {
      int16_t actual_input_index;

      if(j >= ringbufindex_size(&input_ringbuf)) {
        actual_input_index = j - ringbufindex_size(&input_ringbuf);
      } else {
        actual_input_index = j;  
      }

      struct input_packet *input_p = &input_array[actual_input_index];
      if(input_p->ost_flag_change_rx_schedule == 1
         && input_p->ost_prN_nbr != NULL) {
        uint8_t pending_id = OST_NODE_ID_FROM_IPADDR(uip_ds6_nbr_get_ipaddr(input_p->ost_prN_nbr));
        uint16_t pending_N = (input_p->ost_prN_nbr)->ost_nbr_N;
        uint16_t pending_t_offset = (input_p->ost_prN_nbr)->ost_nbr_t_offset;

        ost_eliminate_overlap_toc(toc, N, pending_N, pending_t_offset);

        new_schedule_nodes_in_input_array[new_schedule_num_in_input_array] = pending_id;
        new_schedule_num_in_input_array++;
      }
    }
  }

  /* Check resource overlap with schedule of tx pending queue */
  uint16_t new_schedule_nodes_in_dequeued_array[TSCH_DEQUEUED_ARRAY_SIZE];
  uint8_t new_schedule_num_in_dequeued_array = 0;
  for(i = 0; i < TSCH_DEQUEUED_ARRAY_SIZE; i++) {
    new_schedule_nodes_in_dequeued_array[i] = 0;
  }

  int16_t dequeued_index = ringbufindex_peek_get(&dequeued_ringbuf);
  if(dequeued_index != -1) {
    uint8_t num_dequeued_elements = ringbufindex_elements(&dequeued_ringbuf);  

    uint8_t j;
    for(j = dequeued_index; j < dequeued_index + num_dequeued_elements; j++) {
      int16_t actual_dequeued_index;

      if(j >= ringbufindex_size(&dequeued_ringbuf)) { /* default size: 16 */
        actual_dequeued_index = j - ringbufindex_size(&dequeued_ringbuf);
      } else {
        actual_dequeued_index = j;  
      }

      struct tsch_packet *dequeued_p = dequeued_array[actual_dequeued_index];

      if(dequeued_p->ost_flag_change_tx_schedule == 1
         && dequeued_p->ost_prt_nbr != NULL
         && (dequeued_p->ost_prt_nbr)->ost_my_installable == 1 
         && dequeued_p->ost_flag_rejected_by_nbr == 0) {
        uint8_t pending_id = OST_NODE_ID_FROM_IPADDR(uip_ds6_nbr_get_ipaddr(dequeued_p->ost_prt_nbr));
        if(target_id != pending_id) {
          uint16_t pending_N = (dequeued_p->ost_prt_nbr)->ost_my_N;
          uint16_t pending_t_offset = (dequeued_p->ost_prt_nbr)->ost_my_t_offset;

          ost_eliminate_overlap_toc(toc, N, pending_N, pending_t_offset);

          new_schedule_nodes_in_dequeued_array[new_schedule_num_in_dequeued_array] = pending_id;
          new_schedule_num_in_dequeued_array++;
        }
      }
    }
  }

  /* check resource overlap with ongoing schedule */
  struct tsch_slotframe *sf = ost_tsch_schedule_get_slotframe_head();
  while(sf != NULL) {
    /* periodic provisioning slotframes, except for tx slotframe for target_id 
       do not need to check tx slotframe for target_id (will be re-installed, if overlapped) */
    if(sf->handle > 2 && sf->handle != ost_get_tx_sf_handle_from_id(target_id)) {

      uint8_t already_eliminated = 0;
      uint8_t k;
      for(k = 0; k < new_schedule_num_in_input_array; k++) {
        if(sf->handle == ost_get_rx_sf_handle_from_id(new_schedule_nodes_in_input_array[k])) {
          already_eliminated = 1;
          break;
        }
      }
      if(already_eliminated == 0) {
        for(k = 0; k < new_schedule_num_in_dequeued_array; k++) {
          if(sf->handle == ost_get_tx_sf_handle_from_id(new_schedule_nodes_in_dequeued_array[k])) {
            already_eliminated = 1;
            break;
          }
        }
      }

      if(already_eliminated == 0) {
        uint16_t n;
        for(n = 1; n <= OST_N_MAX; n++) {
          if((sf->size.val >> n) == 1) {
            uint16_t used_N = n;
            struct tsch_link *l = list_head(sf->links_list);

            if(l != NULL) {
              uint16_t used_t_offset = l->timeslot;
              ost_eliminate_overlap_toc(toc, N, used_N, used_t_offset);
            }

            break;
          }
        }
      }
    }
    sf = list_item_next(sf);
  }

  if(toc[t_offset].installable == 1) {
    return 1; /* installable t_offset */
  } else {
    return -1; /* non-installable (overlapped) */
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST /* Process received t_offset */
/* In short, prt */
void
ost_process_rx_t_offset(frame802154_t* frame)
{
  linkaddr_t *eack_src = tsch_queue_get_nbr_address(current_neighbor);
  uint16_t eack_src_id = OST_NODE_ID_FROM_LINKADDR(eack_src);
  
  uip_ds6_nbr_t *ds6_nbr = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)eack_src);

  current_packet->ost_prt_nbr = ds6_nbr;
  current_packet->ost_prt_new_t_offset = ds6_nbr->ost_my_t_offset;
  current_packet->ost_flag_increase_N = 0;
  current_packet->ost_flag_change_tx_schedule = 0;
  current_packet->ost_flag_update_N_of_pkts_in_queue = 0;
  current_packet->ost_flag_rejected_by_nbr = 0;

  if(ds6_nbr != NULL && ost_is_routing_nbr(ds6_nbr) == 1) {
    if(ds6_nbr->ost_my_t_offset != frame->ost_pigg1 
       || ost_has_my_N_changed(ds6_nbr)) {
      /* To be used in ost_post_process_rx_t_offset */
      current_packet->ost_prt_new_t_offset = frame->ost_pigg1;

      if(frame->ost_pigg1 == OST_T_OFFSET_ALLOCATION_FAILURE) {
        current_packet->ost_flag_increase_N = 1; /* increase N due to allocation failure */
#if WITH_OST_LOG_INFO
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: T alloc fail");
        );
#endif
        return;
      }
      if(frame->ost_pigg1 == OST_T_OFFSET_CONSECUTIVE_NEW_TX_REQUEST) {
        current_packet->ost_flag_increase_N = 1; /* increase N due to low prr or consec req */
#if WITH_OST_LOG_INFO
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: consec req");
        );
#endif
        return;          
      }

      ds6_nbr->ost_my_t_offset = current_packet->ost_prt_new_t_offset;
      current_packet->ost_flag_change_tx_schedule = 1;

      /* 1: installable, -1: non-installable, -2: reject */
      int8_t result = ost_tx_installable(eack_src_id, ds6_nbr->ost_my_N, ds6_nbr->ost_my_t_offset);

      if(result == 1) { /* installable */
        if(ds6_nbr->ost_my_installable == 0) {
          ds6_nbr->ost_my_installable = 1;
          current_packet->ost_flag_update_N_of_pkts_in_queue = 1; /* WHY??? */
        }
#if WITH_OST_LOG_INFO
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: inst");
        );
#endif
        return;
      } else if(result == -1) { /* cannot install, request different t_offset */
        if(ds6_nbr->ost_my_installable == 1) {
          ds6_nbr->ost_my_installable = 0;
          current_packet->ost_flag_update_N_of_pkts_in_queue = 1; /* WHY??? */
        }
#if WITH_OST_LOG_INFO
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: non-inst");
        );
#endif
        return;
      } else if(result == -2) { /* rejected by nbr (when t_offset == 65535) */
        if(ds6_nbr->ost_my_installable == 0) {
          ds6_nbr->ost_my_installable = 1;
          current_packet->ost_flag_update_N_of_pkts_in_queue = 1;
        }
        current_packet->ost_flag_rejected_by_nbr = 1;
#if WITH_OST_LOG_INFO
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost prT: deny");
        );
#endif
        return;
      }
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST
/* called from tsch_rx_process_pending (after slot_operation) */
void
ost_post_process_rx_t_offset(struct tsch_packet *p)
{
  linkaddr_t *nbr_lladdr = (linkaddr_t *)uip_ds6_nbr_get_ll(p->ost_prt_nbr);

  /* OST_T_OFFSET_ALLOCATION_FAILURE or OST_T_OFFSET_CONSECUTIVE_NEW_TX_REQUEST */
  if(p->ost_flag_increase_N == 1) {
    p->ost_flag_increase_N = 0;

    if((p->ost_prt_nbr)->ost_my_N < OST_N_MAX) {
#if WITH_OST_LOG_INFO
      TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ost pprT: inc N %u -> %u", (p->ost_prt_nbr)->ost_my_N, ((p->ost_prt_nbr)->ost_my_N) + 1);
      );
#endif
      ((p->ost_prt_nbr)->ost_my_N)++;
      ost_update_N_of_packets_in_queue(nbr_lladdr, (p->ost_prt_nbr)->ost_my_N);
    } else {
#if WITH_OST_LOG_INFO
      TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
                  "ost pprT: inc N MAX");
      );
#endif
    }

  } else if(p->ost_flag_change_tx_schedule == 1) {
    p->ost_flag_change_tx_schedule = 0;

    if((p->ost_prt_nbr) != NULL) {
      ost_remove_tx(nbr_lladdr);

      if((p->ost_prt_nbr)->ost_my_installable == 1) {
        if(p->ost_flag_rejected_by_nbr == 0) { /* installable, not rejected (0xffff) */
          ost_add_tx(nbr_lladdr, (p->ost_prt_nbr)->ost_my_N, p->ost_prt_new_t_offset);
#if WITH_OST_LOG_INFO
          uint16_t nbr_id = OST_NODE_ID_FROM_IPADDR(&((p->ost_prt_nbr)->ipaddr));
          TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "ost pprT: inst n %u N %u T %u", nbr_id, (p->ost_prt_nbr)->ost_my_N, p->ost_prt_new_t_offset);
          );
#endif
        }
      } else { /* cannot install */
#if WITH_OST_LOG_INFO
        uint16_t nbr_id = OST_NODE_ID_FROM_IPADDR(&((p->ost_prt_nbr)->ipaddr));
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "ost pprT: non-inst n %u", nbr_id);
        );
#endif
      }

      if(p->ost_flag_update_N_of_pkts_in_queue == 1) {
        p->ost_flag_update_N_of_pkts_in_queue = 0;

        if((p->ost_prt_nbr)->ost_my_installable == 1) {
          ost_update_N_of_packets_in_queue(nbr_lladdr, (p->ost_prt_nbr)->ost_my_N);
        } else {
          ost_update_N_of_packets_in_queue(nbr_lladdr, (p->ost_prt_nbr)->ost_my_N + OST_N_OFFSET_NEW_TX_REQUEST);
        }
      }

#if WITH_OST_LOG_NBR
      ost_print_nbr();
#endif
#if WITH_OST_LOG_SCH
      tsch_schedule_print_ost();
#endif
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
#if HCK_APPLY_LATEST_CONTIKI
static
void update_link_backoff(struct tsch_link *link) {
  if(link != NULL
      && (link->link_options & LINK_OPTION_TX)
      && (link->link_options & LINK_OPTION_SHARED)) {
    /* Decrement the backoff window for all neighbors able to transmit over
     * this Tx, Shared link. */
    tsch_queue_update_all_backoff_windows(&link->addr);
  }
}
#endif
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
ost_reserved_ssq(uint16_t nbr_id)
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == 0 && ost_ssq_schedule_list[i].asn.ms1b == 0) {
      /* do nothing */
    } else if(!(ost_ssq_schedule_list[i].asn.ls4b == tsch_current_asn.ls4b 
              && ost_ssq_schedule_list[i].asn.ms1b == tsch_current_asn.ms1b)) {
      if(ost_ssq_schedule_list[i].link.link_options == LINK_OPTION_TX) {
        uint16_t id = (ost_ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
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
ost_remove_reserved_ssq(uint16_t nbr_id) //hckim
{
  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == 0 && ost_ssq_schedule_list[i].asn.ms1b == 0) {

    } else if(!(ost_ssq_schedule_list[i].asn.ls4b == tsch_current_asn.ls4b && ost_ssq_schedule_list[i].asn.ms1b == tsch_current_asn.ms1b)) {
      if(ost_ssq_schedule_list[i].link.link_options == LINK_OPTION_TX) {
        uint16_t id = (ost_ssq_schedule_list[i].link.slotframe_handle - SSQ_SCHEDULE_HANDLE_OFFSET - 1) / 2;
        if(nbr_id == id) {
          ost_ssq_schedule_list[i].asn.ls4b = 0;
          ost_ssq_schedule_list[i].asn.ms1b = 0;
        }
      }
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_UPA
uint8_t
upa_calculate_slot_utility(struct tsch_neighbor *curr_nbr,
                        struct tsch_packet *triggering_pkt,
                        uint8_t num_of_pkts_to_request)
{
  uint8_t upa_bitmap_of_slot_utility = 0;
  int upa_all_operation_timeslots = 0; /* include triggering slot */

  upa_expected_passed_timeslots_except_first_slot = 0;

  int triggering_pkt_len = queuebuf_datalen(triggering_pkt->qb);
  int triggering_ack_len = 21;

  rtimer_clock_t triggering_slot_operation_duration = 0;
  triggering_slot_operation_duration = tsch_timing[tsch_ts_tx_offset] 
                                     + TSCH_PACKET_DURATION(triggering_pkt_len)
                                     + tsch_timing[tsch_ts_tx_ack_delay]
                                     + TSCH_PACKET_DURATION(triggering_ack_len);

  upa_all_operation_timeslots = (triggering_slot_operation_duration 
                                  + tsch_timing[tsch_ts_timeslot_length] - 1) 
                                / tsch_timing[tsch_ts_timeslot_length];
  if(upa_all_operation_timeslots > 1) {
    upa_bitmap_of_slot_utility = upa_bitmap_of_slot_utility | (1 << 0);
  }

  int upa_expected_packet_len = 0;
  int upa_expected_timeslots = 0;

  rtimer_clock_t upa_expected_packet_duration = 0;
  rtimer_clock_t upa_expected_tx_duration = 0;
  rtimer_clock_t upa_expected_all_tx_duration = 0;

  uint8_t i = 1;
  for(i = 1; i <= num_of_pkts_to_request; i++) {
    struct tsch_packet *upa_packet = tsch_queue_upa_get_next_packet_for_nbr(curr_nbr, i);

    upa_expected_packet_len = queuebuf_datalen(upa_packet->qb);
    upa_expected_packet_duration = TSCH_PACKET_DURATION(upa_expected_packet_len);

    if(i == 1) {
      upa_expected_tx_duration += (upa_timing[upa_ts_tx_offset_1] 
                                    + upa_expected_packet_duration);
    } else {
      upa_expected_tx_duration += (upa_timing[upa_ts_tx_offset_2] 
                                    + upa_expected_packet_duration);
    }
    upa_expected_all_tx_duration = upa_expected_tx_duration
                                + upa_timing[upa_ts_tx_ack_delay] 
                                + upa_timing[upa_ts_max_ack]
                                + upa_timing[upa_ts_tx_process_b_ack];

    upa_expected_timeslots = (triggering_slot_operation_duration 
                                + upa_expected_all_tx_duration 
                                + tsch_timing[tsch_ts_timeslot_length] - 1) 
                              / tsch_timing[tsch_ts_timeslot_length];

    if(upa_expected_timeslots > upa_all_operation_timeslots) {
      upa_all_operation_timeslots = upa_expected_timeslots;
      upa_bitmap_of_slot_utility = upa_bitmap_of_slot_utility | (1 << i);
    }
  }

  upa_expected_passed_timeslots_except_first_slot = upa_all_operation_timeslots - 1;

  return upa_bitmap_of_slot_utility;
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

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx0: start of tsch_tx_slot */
  regular_slot_timestamp_tx[0] = RTIMER_NOW();
#endif

  /* First check if we have space to store a newly dequeued packet (in case of
   * successful Tx or Drop) */
  dequeued_index = ringbufindex_peek_put(&dequeued_ringbuf);
  if(dequeued_index != -1) {
    ++tsch_dequeued_ringbuf_available_count;

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
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
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

#if WITH_SLA
      if(do_wait_for_ack) {
        current_packet->sla_is_broadcast = 0;
      } else {
        current_packet->sla_is_broadcast = 1;
      }
#endif

#if HCK_DBG_REGULAR_SLOT_DETAIL /* Store packet len and do_wait_for_ack */
      regular_slot_tx_packet_len = packet_len;
      regular_slot_tx_do_wait_for_ack = do_wait_for_ack;
#endif

#if WITH_UPA
      upa_link_requested = 0;
      
      /* Unicast. More packets in queue for the neighbor? */
      if(do_wait_for_ack && tsch_queue_nbr_packet_count(current_neighbor) > 1) {
        /* except for current packet: tsch_queue_nbr_packet_count(current_neighbor) - 1 */
        int upa_pkts_pending = tsch_queue_nbr_packet_count(current_neighbor) - 1;
        /* consider current packet: ringbufindex_elements(&dequeued_ringbuf) + 1 */
        int upa_empty_space_of_dequeued_ringbuf
            = ((int)TSCH_DEQUEUED_ARRAY_SIZE - 1) - (ringbufindex_elements(&dequeued_ringbuf) + 1) > 0 ?
              ((int)TSCH_DEQUEUED_ARRAY_SIZE - 1) - (ringbufindex_elements(&dequeued_ringbuf) + 1) : 0;

        int upa_pkts_to_request = MIN(upa_pkts_pending, upa_empty_space_of_dequeued_ringbuf);
        upa_pkts_to_request = MIN(upa_pkts_to_request, (((int)TSCH_MAX_INCOMING_PACKETS - 1) - 1));

        if(upa_pkts_to_request > 0
#if HCK_ASAP_EVAL_02_UPA_SINGLE_HOP
            && upa_pkts_to_request >= eval_01_num_of_pkts_aggregated
#endif
          ) {
#if HCK_ASAP_EVAL_02_UPA_SINGLE_HOP
          upa_pkts_to_request = eval_01_num_of_pkts_aggregated;
          upa_calculate_slot_utility(current_neighbor, current_packet, upa_pkts_to_request);
          uint16_t upa_info_to_request = (upa_pkts_to_request << 8) + 0;
#else /* HCK_ASAP_EVAL_02_UPA_SINGLE_HOP */
          uint8_t upa_bitmap_of_slot_utility 
                  = upa_calculate_slot_utility(current_neighbor, current_packet, upa_pkts_to_request);
          uint16_t upa_info_to_request = (upa_pkts_to_request << 8) + upa_bitmap_of_slot_utility;
#endif /* HCK_ASAP_EVAL_02_UPA_SINGLE_HOP */
          upa_link_requested = 1;
          tsch_packet_set_frame_pending(packet, packet_len);

          frame802154_t upa_frame;
          int upa_hdr_len;
          upa_hdr_len = frame802154_parse((uint8_t *)packet, packet_len, &upa_frame);
          ((uint8_t *)(packet))[upa_hdr_len + 2] = (uint8_t)(upa_info_to_request & 0xFF);
          ((uint8_t *)(packet))[upa_hdr_len + 3] = (uint8_t)((upa_info_to_request >> 8) & 0xFF);

#if UPA_DBG_OPERATION
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "upa pol req %u %x %x (%d %d)", upa_pkts_to_request,
                                           upa_bitmap_of_slot_utility, 
                                           upa_info_to_request,
                                           upa_pkts_pending,
                                           upa_empty_space_of_dequeued_ringbuf));
#endif
        }
      }
#elif WITH_TSCH_DEFAULT_BURST_TRANSMISSION
      /* Unicast. More packets in queue for the neighbor? */
      burst_link_requested = 0;
      if(do_wait_for_ack
             && tsch_current_burst_count + 1 < TSCH_BURST_MAX_LEN
             && tsch_queue_nbr_packet_count(current_neighbor) > 1) {
#if MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
        uint16_t time_to_earliest_schedule = 0;
        if(tsch_schedule_get_next_timeslot_available_or_not(&tsch_current_asn, &time_to_earliest_schedule)) {
#if ENABLE_MODIFIED_DBT_LOG
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "sched dbt tx req %d %d", burst_link_requested, time_to_earliest_schedule));
#endif
#endif
          burst_link_requested = 1;
          tsch_packet_set_frame_pending(packet, packet_len);
#if MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
        }
#if ENABLE_MODIFIED_DBT_LOG
        else {
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "sched dbt tx coll %d %d", burst_link_requested, time_to_earliest_schedule));
        }
#endif
#endif
      }
#endif

      /* read seqno from payload */
      seqno = ((uint8_t *)(packet))[2];

#if WITH_OST /* OST-03-07: Piggyback N */
#if !OST_ON_DEMAND_PROVISION

      seqno = ((uint8_t *)(packet))[4];

#else /* OST_ON_DEMAND_PROVISION */
      frame802154_fcf_t fcf;
      frame802154_parse_fcf((uint8_t *)(packet), &fcf);

      int queued_pkts = ringbufindex_elements(&current_neighbor->tx_ringbuf);
      uint16_t nbr_id = OST_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(current_neighbor));

      if(fcf.ack_required) {
        if(queued_pkts > 1) {
          if(ost_reserved_ssq(nbr_id) == 0) { /* if 1, there exists at least one reserved ssq for nbr_id */
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

#if WITH_OST_LOG_INFO
            TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                "ost odp: ssq_s %u", ssq_schedule)
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

#endif /* OST_ON_DEMAND_PROVISION */
#endif /* WITH_OST */

      /* if this is an EB, then update its Sync-IE */
      if(current_neighbor == n_eb) {
        packet_ready = tsch_packet_update_eb(packet, packet_len, current_packet->tsch_sync_ie_offset);
#if WITH_SLA /* Coordinator/non-coordinator: update information in EB before transmission */
        if(packet_ready && sla_packet_update_eb(packet, packet_len, current_packet->tsch_sync_ie_offset)) {
          packet_ready = 1;
        } else {
          packet_ready = 0;
        }
#endif
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

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx1: before RADIO.prepare() */
      regular_slot_timestamp_tx[1] = RTIMER_NOW();
#endif

      /* prepare packet to send: copy to radio buffer */
      if(packet_ready && NETSTACK_RADIO.prepare(packet, packet_len) == 0) { /* 0 means success */
        static rtimer_clock_t tx_duration;

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx2: after RADIO.prepare() */
        regular_slot_timestamp_tx[2] = RTIMER_NOW();
#endif

#if TSCH_CCA_ENABLED
        cca_status = 1;

        current_slot_unused_offset_start = RTIMER_NOW();

        /* delay before CCA */
        TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_cca_offset], "cca");
        TSCH_DEBUG_TX_EVENT();

        current_slot_unused_offset_end = RTIMER_NOW();
        current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx3: after cca_offset */
        regular_slot_timestamp_tx[3] = RTIMER_NOW();
#endif

        tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx4: after tsch_radio_on() */
        regular_slot_timestamp_tx[4] = RTIMER_NOW();
#endif

#if WITH_UPA && UPA_TRIPLE_CCA
#if UPA_DBG_TIMING_TRIPLE_CCA
        is_upa_triple_cca = 1;
        upa_timestamp_triple_cca[0] = RTIMER_NOW();
#endif

        /* 1st CCA */
        RTIMER_BUSYWAIT_UNTIL_ABS(!(cca_status &= NETSTACK_RADIO.channel_clear()),
                           current_slot_start, tsch_timing[tsch_ts_cca_offset] + tsch_timing[tsch_ts_cca]);
        TSCH_DEBUG_TX_EVENT();

#if UPA_DBG_TIMING_TRIPLE_CCA
        upa_timestamp_triple_cca[1] = RTIMER_NOW();
#endif

        if(cca_status == 1) {
          tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

          current_slot_unused_offset_start = RTIMER_NOW();
  
          /* delay before 2nd CCA */
          RTIMER_BUSYWAIT_UNTIL_ABS(0, current_slot_start, tsch_timing[tsch_ts_cca_offset] + tsch_timing[upa_ts_inter_cca_offset]);
          current_slot_unused_offset_end = RTIMER_NOW();
          current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

          TSCH_DEBUG_TX_EVENT();

#if UPA_DBG_TIMING_TRIPLE_CCA
          upa_timestamp_triple_cca[2] = RTIMER_NOW();
#endif

          /* 2nd CCA */
          RTIMER_BUSYWAIT_UNTIL_ABS(!(cca_status &= NETSTACK_RADIO.channel_clear()),
              current_slot_start, 
              tsch_timing[tsch_ts_cca_offset] + tsch_timing[upa_ts_inter_cca_offset] + tsch_timing[tsch_ts_cca]);
          TSCH_DEBUG_TX_EVENT();

#if UPA_DBG_TIMING_TRIPLE_CCA
          upa_timestamp_triple_cca[3] = RTIMER_NOW();
#endif
        }

        if(cca_status == 1) {
          tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

          current_slot_unused_offset_start = RTIMER_NOW();

          /* delay before 3rd CCA */
          RTIMER_BUSYWAIT_UNTIL_ABS(0, current_slot_start, tsch_timing[tsch_ts_cca_offset] + tsch_timing[upa_ts_inter_cca_offset] * 2);
          current_slot_unused_offset_end = RTIMER_NOW();
          current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

          TSCH_DEBUG_TX_EVENT();

#if UPA_DBG_TIMING_TRIPLE_CCA
          upa_timestamp_triple_cca[4] = RTIMER_NOW();
#endif

          /* 3rd CCA */
          RTIMER_BUSYWAIT_UNTIL_ABS(!(cca_status &= NETSTACK_RADIO.channel_clear()),
              current_slot_start, 
              tsch_timing[tsch_ts_cca_offset] + tsch_timing[upa_ts_inter_cca_offset] * 2 + tsch_timing[tsch_ts_cca]);
          TSCH_DEBUG_TX_EVENT();

#if UPA_DBG_TIMING_TRIPLE_CCA
          upa_timestamp_triple_cca[5] = RTIMER_NOW();
#endif
        }
#else /* WITH_UPA && UPA_TRIPLE_CCA */

        /* CCA */
        RTIMER_BUSYWAIT_UNTIL_ABS(!(cca_status &= NETSTACK_RADIO.channel_clear()),
                           current_slot_start, tsch_timing[tsch_ts_cca_offset] + tsch_timing[tsch_ts_cca]);
        TSCH_DEBUG_TX_EVENT();

#endif /* WITH_UPA && UPA_TRIPLE_CCA */

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx5: after CCA */
        regular_slot_timestamp_tx[5] = RTIMER_NOW();
#endif

        /* there is not enough time to turn radio off */
        /*  NETSTACK_RADIO.off(); */
        if(cca_status == 0) {
          mac_tx_status = MAC_TX_COLLISION;
        } else
#endif /* TSCH_CCA_ENABLED */
        {
          current_slot_unused_offset_start = RTIMER_NOW();

          /* delay before TX */
          TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_tx_offset] - RADIO_DELAY_BEFORE_TX, "TxBeforeTx");

          current_slot_unused_offset_end = RTIMER_NOW();
          current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

          TSCH_DEBUG_TX_EVENT();

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx6: before RADIO.transmit() */
          regular_slot_timestamp_tx[6] = RTIMER_NOW();
#endif

          /* send packet already in radio tx buffer */
          mac_tx_status = NETSTACK_RADIO.transmit(packet_len);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx7: after RADIO.transmit() */
          regular_slot_timestamp_tx[7] = RTIMER_NOW();
#endif

          tx_count++;
          /* Save tx timestamp */
          tx_start_time = current_slot_start + tsch_timing[tsch_ts_tx_offset];
          /* calculate TX duration based on sent packet len */
          tx_duration = TSCH_PACKET_DURATION(packet_len);
          /* limit tx_time to its max value */
          tx_duration = MIN(tx_duration, tsch_timing[tsch_ts_max_tx]);

#if WITH_OST
          tx_duration = tsch_timing[tsch_ts_max_tx];
#endif

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx8: before turn_radio_off() */
          regular_slot_timestamp_tx[8] = RTIMER_NOW();
#endif

          /* turn tadio off -- will turn on again to wait for ACK if needed */
          tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx9: after turn_radio_off() */
          regular_slot_timestamp_tx[9] = RTIMER_NOW();
#endif

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

              current_slot_unused_offset_start = RTIMER_NOW();

              /* Unicast: wait for ack after tx: sleep until ack time */
              TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start,
                  tsch_timing[tsch_ts_tx_offset] + tx_duration + tsch_timing[tsch_ts_rx_ack_delay] - RADIO_DELAY_BEFORE_RX, "TxBeforeAck");

              current_slot_unused_offset_end = RTIMER_NOW();
              current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx10: before turn_radio_on() for ACK */
              regular_slot_timestamp_tx[10] = RTIMER_NOW();
#endif

              TSCH_DEBUG_TX_EVENT();
              tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx11: after turn_radio_on() for ACK */
              regular_slot_timestamp_tx[11] = RTIMER_NOW();
#endif

              /* Wait for ACK to come */
              RTIMER_BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),
                  tx_start_time, tx_duration + tsch_timing[tsch_ts_rx_ack_delay] + tsch_timing[tsch_ts_ack_wait] + RADIO_DELAY_BEFORE_DETECT);
              TSCH_DEBUG_TX_EVENT();

              ack_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;
#if WITH_UPA
              upa_tx_slot_trig_ack_start_time = ack_start_time;
#endif

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx12: ACK start time */
              regular_slot_timestamp_tx[12] = ack_start_time;
#endif

              /* Wait for ACK to finish */
              RTIMER_BUSYWAIT_UNTIL_ABS(!NETSTACK_RADIO.receiving_packet(),
                                 ack_start_time, tsch_timing[tsch_ts_max_ack]);
              TSCH_DEBUG_TX_EVENT();

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx13: ACK end time, before turn_radio_off() */
              regular_slot_timestamp_tx[13] = RTIMER_NOW();
#endif

#if WITH_UPA
              if(upa_link_requested) {
                upa_tx_slot_pending_tsch_radio_off = 1;
              } else {
                upa_tx_slot_pending_tsch_radio_off = 0;
#endif
                tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);
#if WITH_UPA
              }
#endif


#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx14: after turn_radio_off() for ACK */
              regular_slot_timestamp_tx[14] = RTIMER_NOW();
#endif

#if TSCH_HW_FRAME_FILTERING
              /* Leaving promiscuous mode */
              NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
              NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode | RADIO_RX_MODE_ADDRESS_FILTER);
#endif /* TSCH_HW_FRAME_FILTERING */

              /* Read ack frame */
              ack_len = NETSTACK_RADIO.read((void *)ackbuf, sizeof(ackbuf));

#if HCK_DBG_REGULAR_SLOT_DETAIL /* Store ack_len */
              regular_slot_tx_ack_len = ack_len;
#endif

              asap_tot_ack_len += ack_len;

#if WITH_UPA
              upa_tx_slot_trig_ack_len = ack_len;
#endif

              is_time_source = 0;
              /* The radio driver should return 0 if no valid packets are in the rx buffer */
              if(ack_len > 0) {
                is_time_source = current_neighbor != NULL && current_neighbor->is_time_source;

                if(tsch_packet_parse_eack(ackbuf, ack_len, seqno,
                    &frame, &ack_ies, &ack_hdrlen) == 0) {
                  ack_len = 0;
                }

#if WITH_OST /* Process received t_offset */
#if WITH_OST_LOG_DBG
                uint16_t nbr_id = OST_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(current_neighbor));
                TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                    "ost rcvd T: n %u T %u", nbr_id, frame.ost_pigg1)
                );
#endif

                if(ack_len != 0) {
                  ost_process_rx_t_offset(&frame);
#if OST_ON_DEMAND_PROVISION
                  ost_process_rx_matching_slot(&frame);
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

#if WITH_SLA /* Coordinator: record received ACK length */
              if(tsch_is_coordinator && ack_len != 0) {
                sla_record_ack_len(ack_len);
              }
#endif

#if WITH_UPA
              if(ack_len != 0) {
                uint16_t upa_pkts_allowed = ack_ies.ie_upa_info;
#if UPA_DBG_OPERATION
                if(upa_link_requested) {
                  TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                      "upa pol allowed %u", upa_pkts_allowed));
                }
#endif
                if(upa_link_requested && upa_pkts_allowed > 0) {
                  upa_link_scheduled = 1;
                  upa_pkts_to_send = upa_pkts_allowed;
                } else {
                  upa_link_scheduled = 0;
                  upa_pkts_to_send = 0;
                }
                upa_link_requested = 0;
              }

              if(upa_tx_slot_pending_tsch_radio_off) {
                upa_tx_slot_pending_tsch_radio_off = 0;

                if(upa_link_scheduled == 0) {
                  tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);                
                }
              }
#endif

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

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
                /* We requested an extra slot and got an ack. This means
                the extra slot will be scheduled at the received */
                if(burst_link_requested) {
#if MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
                  if(tsch_packet_get_frame_pending(ackbuf, ack_len)) {
#if ENABLE_MODIFIED_DBT_LOG
                    TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                        "sched dbt tx succ %d", burst_link_scheduled));
#endif
#endif
                    burst_link_scheduled = 1;
#if TSCH_DBT_HOLD_CURRENT_NBR
                    burst_link_tx = 1;
                    burst_link_rx = 0;
#endif
#if MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
                  } else {
#if ENABLE_MODIFIED_DBT_LOG
                    TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                        "sched dbt tx fail %d", burst_link_scheduled));
#endif
                    burst_link_scheduled = 0;
                  }
#endif
                }

#endif /* WITH_TSCH_DEFAULT_BURST_TRANSMISSION */

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

#if WITH_UPA
    current_packet->upa_sent_in_batch = 0;
#endif

    /* Post TX: Update neighbor queue state */
    in_queue = tsch_queue_packet_sent(current_neighbor, current_packet, current_link, mac_tx_status);

    /* The packet was dequeued, add it to dequeued_ringbuf for later processing */
    if(in_queue == 0) {
      dequeued_array[dequeued_index] = current_packet;
      ringbufindex_put(&dequeued_ringbuf);
    }

#if HCK_DBG_REGULAR_SLOT_DETAIL
    regular_slot_tx_mac_tx_status = mac_tx_status;
#endif

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegTx15: after processing ACK */
    regular_slot_timestamp_tx[15] = RTIMER_NOW();
#endif

#if WITH_UPA
    if(upa_link_scheduled) {
      upa_tx_slot_pending_stats_and_log_update = 1;
    } else {
      upa_tx_slot_pending_stats_and_log_update = 0;
#endif
      /* If this is an unicast packet to timesource, update stats */
      if(current_neighbor != NULL && current_neighbor->is_time_source) {
        tsch_stats_tx_packet(current_neighbor, mac_tx_status, tsch_current_channel);
      }

      current_slot_idle_start = RTIMER_NOW();
      current_slot_busy_time = RTIMER_CLOCK_DIFF(current_slot_idle_start, current_slot_start);
      current_slot_passed_slots = (current_slot_busy_time + tsch_timing[tsch_ts_timeslot_length] - 1) 
                                  / tsch_timing[tsch_ts_timeslot_length];
      current_slot_idle_time = current_slot_passed_slots * tsch_timing[tsch_ts_timeslot_length] - current_slot_busy_time;

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
#if ENABLE_LOG_TSCH_WITH_APP_FOOTER
          memcpy(&log->tx.app_magic, (uint8_t *)queuebuf_dataptr(current_packet->qb) + queuebuf_datalen(current_packet->qb) - 2, 2);
          memcpy(&log->tx.app_seqno, (uint8_t *)queuebuf_dataptr(current_packet->qb) + queuebuf_datalen(current_packet->qb) - 2 - 4, 4);
#endif
          log->tx.asap_unused_offset_time = RTIMERTICKS_TO_US(current_slot_unused_offset_time);
          log->tx.asap_idle_time = RTIMERTICKS_TO_US(current_slot_idle_time);
          log->tx.asap_curr_slot_len = tsch_timing_us[tsch_ts_timeslot_length];
          log->tx.asap_num_of_slots_until_idle_time = current_slot_passed_slots;
          log->tx.asap_ack_len = asap_tot_ack_len;
      );
#if WITH_UPA
    }
#endif

#if !WITH_TSCH_DEFAULT_BURST_TRANSMISSION
    /* hckim measure tx operation counts */
    if(current_link->slotframe_handle == TSCH_SCHED_EB_SF_HANDLE) {
      ++tsch_eb_sf_tx_operation_count;
    } else if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
      ++tsch_common_sf_tx_operation_count;
    } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
      ++tsch_unicast_sf_tx_operation_count;
    }
#if WITH_OST
    else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
          && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
      ++tsch_ost_pp_sf_tx_operation_count;
    } else if(current_link->slotframe_handle > OST_ONDEMAND_SF_ID_OFFSET) {
      ++tsch_ost_odp_sf_tx_operation_count;
    }
#endif
#else /* !WITH_TSCH_DEFAULT_BURST_TRANSMISSION */
    /* hckim measure tx operation counts */
    if(!is_burst_slot) {
      if(current_link->slotframe_handle == TSCH_SCHED_EB_SF_HANDLE) {
        ++tsch_eb_sf_tx_operation_count;
      } else if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
        ++tsch_common_sf_tx_operation_count;
      } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
        ++tsch_unicast_sf_tx_operation_count;
      }
#if WITH_OST
      else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
            && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
        ++tsch_ost_pp_sf_tx_operation_count;
      }
#endif
    } else {
      if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
        ++tsch_common_sf_bst_tx_operation_count;
      } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
        ++tsch_unicast_sf_bst_tx_operation_count;
      }
#if WITH_OST
      else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
            && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
        ++tsch_ost_pp_sf_bst_tx_operation_count;
      }
#endif
    }
#endif

    /* Poll process for later processing of packet sent events and logs */
    process_poll(&tsch_pending_events_process);
  }
  /* hckim */
  else {
    ++tsch_dequeued_ringbuf_full_count;
  }

#if WITH_UPA
  if(upa_link_scheduled) {
#if UPA_DBG_SLOT_TIMING /* upaTxB0: start of tsch_upa_tx_slot, prepare burst */
    upa_print_tx_slot_timing = 1;
    upa_tx_slot_timestamp_begin[0] = RTIMER_NOW();
#endif

    upa_tx_slot_trig_packet_len = queuebuf_datalen(current_packet->qb);
    upa_tx_slot_all_packet_len_same = 1;

    asap_tot_packet_len += upa_tx_slot_trig_packet_len;
    asap_ok_packet_len += upa_tx_slot_trig_packet_len;

    uint8_t i = 0;
    for(i = 0; i < TSCH_DEQUEUED_ARRAY_SIZE; i++) {
      upa_tx_slot_dequeued_ringbuf_index_array[i] = 0;
      upa_tx_slot_in_queue_array[i] = 0;
      upa_tx_slot_packet_array[i] = NULL;
    }

    int upa_tx_slot_ringbuf_head_index = ringbufindex_peek_get(&current_neighbor->tx_ringbuf);
    for(i = 0; i < upa_pkts_to_send; i++) {
      upa_tx_slot_dequeued_ringbuf_index_array[i] = (upa_tx_slot_ringbuf_head_index + i) < TSCH_QUEUE_NUM_PER_NEIGHBOR ?
                                    (upa_tx_slot_ringbuf_head_index + i) : 
                                    (upa_tx_slot_ringbuf_head_index + i) - TSCH_QUEUE_NUM_PER_NEIGHBOR;
      upa_tx_slot_packet_array[i] = current_neighbor->tx_array[upa_tx_slot_dequeued_ringbuf_index_array[i]];
    }

    upa_tx_slot_in_batch_seq = 1;
    upa_tx_slot_curr_packet = upa_tx_slot_packet_array[0];

#if UPA_DBG_SLOT_TIMING /* upaTxB1: end of preparing burst */
    upa_tx_slot_timestamp_begin[1] = RTIMER_NOW();
#endif

    upa_tx_slot_trig_ack_duration = TSCH_PACKET_DURATION(upa_tx_slot_trig_ack_len);
    upa_tx_slot_batch_tx_start_time = upa_tx_slot_trig_ack_start_time 
                                    + upa_tx_slot_trig_ack_duration;

    while(1) {
#if UPA_DBG_OPERATION
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa tx seq %u begin", upa_tx_slot_in_batch_seq));
#endif

      if(ringbufindex_elements(&dequeued_ringbuf) + upa_tx_slot_in_batch_seq < TSCH_DEQUEUED_ARRAY_SIZE) {
        ++tsch_dequeued_ringbuf_available_count;

        if(upa_tx_slot_curr_packet == NULL || upa_tx_slot_curr_packet->qb == NULL) {
          upa_tx_slot_mac_tx_status = MAC_TX_ERR_FATAL;
        } else {
          upa_tx_slot_curr_packet_payload = queuebuf_dataptr(upa_tx_slot_curr_packet->qb);
          upa_tx_slot_curr_packet_len = queuebuf_datalen(upa_tx_slot_curr_packet->qb);

          if(upa_tx_slot_trig_packet_len != upa_tx_slot_curr_packet_len) {
            upa_tx_slot_all_packet_len_same = 0;
          }

          asap_tot_packet_len += upa_tx_slot_curr_packet_len;

          frame802154_t upa_frame;
          int upa_hdr_len;
          upa_hdr_len = frame802154_parse((uint8_t *)upa_tx_slot_curr_packet_payload, upa_tx_slot_curr_packet_len, &upa_frame);
          ((uint8_t *)(upa_tx_slot_curr_packet_payload))[upa_hdr_len + 2] = (uint8_t)(upa_tx_slot_in_batch_seq & 0xFF);
          ((uint8_t *)(upa_tx_slot_curr_packet_payload))[upa_hdr_len + 3] = (uint8_t)((upa_tx_slot_in_batch_seq >> 8) & 0xFF);

#if UPA_DBG_OPERATION
          uint8_t upa_mac_seqno = ((uint8_t *)(upa_tx_slot_curr_packet_payload))[2];
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "upa tx mac seq %u", upa_mac_seqno));
#endif

#if UPA_DBG_SLOT_TIMING /* upaTxT0: before RADIO.prepare() */
          upa_tx_slot_timestamp_tx[upa_tx_slot_in_batch_seq - 1][0] = RTIMER_NOW();
#endif

          if(NETSTACK_RADIO.prepare(upa_tx_slot_curr_packet_payload, upa_tx_slot_curr_packet_len) == 0) { /* 0 means success */

#if UPA_DBG_SLOT_TIMING /* upaTxT1: after RADIO.prepare() */
            upa_tx_slot_timestamp_tx[upa_tx_slot_in_batch_seq - 1][1] = RTIMER_NOW();
#endif

            if(upa_tx_slot_in_batch_seq == 1) { /* the first transmission */
              upa_tx_slot_curr_start = upa_tx_slot_batch_tx_start_time;
              upa_tx_slot_curr_offset = upa_timing[upa_ts_tx_offset_1];

              current_slot_unused_offset_start = RTIMER_NOW();

              TSCH_SCHEDULE_AND_YIELD(pt, t, upa_tx_slot_curr_start, 
                                      upa_tx_slot_curr_offset - RADIO_DELAY_BEFORE_TX, "upaTx1");

              current_slot_unused_offset_end = RTIMER_NOW();
              current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);
            } else {
              upa_tx_slot_prev_start = upa_tx_slot_curr_start;
              upa_tx_slot_prev_offset = upa_tx_slot_curr_offset;
              upa_tx_slot_prev_duration = upa_tx_slot_curr_duration;

              upa_tx_slot_curr_start = upa_tx_slot_prev_start
                                      + upa_tx_slot_prev_offset
                                      + upa_tx_slot_prev_duration;

              upa_tx_slot_curr_offset = upa_timing[upa_ts_tx_offset_2];

              current_slot_unused_offset_start = RTIMER_NOW();

              TSCH_SCHEDULE_AND_YIELD(pt, t, upa_tx_slot_curr_start, 
                                      upa_tx_slot_curr_offset - RADIO_DELAY_BEFORE_TX, "upaTx2");

              current_slot_unused_offset_end = RTIMER_NOW();
              current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);
            }

#if UPA_DBG_SLOT_TIMING /* upaTxT2: before RADIO.transmit() */
            upa_tx_slot_timestamp_tx[upa_tx_slot_in_batch_seq - 1][2] = RTIMER_NOW();
#endif

            upa_tx_slot_mac_tx_status = NETSTACK_RADIO.transmit(upa_tx_slot_curr_packet_len);

#if UPA_DBG_SLOT_TIMING /* upaTxT3: after RADIO.transmit() */
            upa_tx_slot_timestamp_tx[upa_tx_slot_in_batch_seq - 1][3] = RTIMER_NOW();
#endif

            tx_count++;

            upa_tx_slot_curr_duration = TSCH_PACKET_DURATION(upa_tx_slot_curr_packet_len);
            upa_tx_slot_curr_duration = MIN(upa_tx_slot_curr_duration, upa_timing[upa_ts_max_tx]);

            if(upa_tx_slot_mac_tx_status == RADIO_TX_OK) {
              upa_tx_slot_mac_tx_status = MAC_TX_NOACK; /* Not yet ACK received */
            } else {
              upa_tx_slot_mac_tx_status = MAC_TX_ERR;
            }
          } else {
            upa_tx_slot_mac_tx_status = MAC_TX_ERR;
          }
        }

#if UPA_DBG_OPERATION
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "upa tx seq %u end", upa_tx_slot_in_batch_seq));
#endif
      } else {
        ++tsch_dequeued_ringbuf_full_count;

#if UPA_DBG_ESSENTIAL
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "!upa tx %u fail (full buf)", upa_tx_slot_in_batch_seq));
#endif

        break; /* No space in dequeued_array */
      }

      if(upa_tx_slot_in_batch_seq < upa_pkts_to_send) {
        ++upa_tx_slot_in_batch_seq;
        upa_tx_slot_curr_packet = upa_tx_slot_packet_array[upa_tx_slot_in_batch_seq - 1];
      } else {
        upa_tx_slot_curr_packet = NULL;
        break; /* Transmitted all the allowed packets */
      }
    }

#if UPA_DBG_SLOT_TIMING /* upaTxA0: after while loop */
    upa_tx_slot_timestamp_ack[0] = RTIMER_NOW();
#endif

    upa_tx_slot_prev_start = upa_tx_slot_curr_start;
    upa_tx_slot_prev_offset = upa_tx_slot_curr_offset;
    upa_tx_slot_prev_duration = upa_tx_slot_curr_duration;

    upa_tx_slot_curr_start = upa_tx_slot_prev_start
                            + upa_tx_slot_prev_offset
                            + upa_tx_slot_prev_duration;
    upa_tx_slot_curr_offset = upa_timing[upa_ts_rx_ack_delay];

    current_slot_unused_offset_start = RTIMER_NOW();

    TSCH_SCHEDULE_AND_YIELD(pt, t, upa_tx_slot_curr_start, upa_tx_slot_curr_offset, "upaTx3");

    current_slot_unused_offset_end = RTIMER_NOW();
    current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

#if UPA_DBG_SLOT_TIMING /* upaTxA1: after ts_rx_ack_delay */
    upa_tx_slot_timestamp_ack[1] = RTIMER_NOW();
#endif

    upa_tx_slot_curr_offset = (upa_timing[upa_ts_tx_offset_1] + upa_timing[upa_ts_max_tx])
                            + (upa_timing[upa_ts_tx_offset_2] + upa_timing[upa_ts_max_tx]) * (upa_pkts_to_send - 1)
                            + upa_timing[upa_ts_rx_ack_delay] + upa_timing[upa_ts_ack_wait];

    uint8_t upa_b_ackbuf[TSCH_PACKET_MAX_LEN];
    int upa_b_ack_len;
    rtimer_clock_t upa_b_ack_start_time;
    struct ieee802154_ies upa_b_ack_ies;
    uint8_t upa_b_ack_hdrlen;
    frame802154_t upa_b_ack_frame;

    RTIMER_BUSYWAIT_UNTIL_ABS((upa_tx_slot_b_ack_seen = NETSTACK_RADIO.receiving_packet()),
                              upa_tx_slot_batch_tx_start_time, 
                              upa_tx_slot_curr_offset + RADIO_DELAY_BEFORE_DETECT);

    upa_b_ack_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

#if UPA_DBG_SLOT_TIMING /* upaTxA2: ACK start time */
    upa_tx_slot_timestamp_ack[2] = upa_b_ack_start_time;
#endif

    RTIMER_BUSYWAIT_UNTIL_ABS(!NETSTACK_RADIO.receiving_packet(),
                              upa_b_ack_start_time, upa_timing[upa_ts_max_ack]);

#if UPA_DBG_SLOT_TIMING /* upaTxA3: ACK end time */
    upa_tx_slot_timestamp_ack[3] = RTIMER_NOW();
#endif

    upa_b_ack_len = NETSTACK_RADIO.read((void *)upa_b_ackbuf, sizeof(upa_b_ackbuf));

    if(upa_b_ack_len > 0) {
      if(tsch_packet_parse_eack(upa_b_ackbuf, upa_b_ack_len, 0, &upa_b_ack_frame, &upa_b_ack_ies, &upa_b_ack_hdrlen) == 0) {
        upa_b_ack_len = 0;
      }
    }

    uint16_t upa_b_ack_received_info = 0;
    if(upa_b_ack_len > 0) {
      upa_b_ack_received_info = upa_b_ack_ies.ie_upa_info;

#if UPA_DBG_OPERATION
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa b-ack %u", upa_b_ack_received_info));
#endif
    } else {
      upa_b_ack_received_info = 0;

#if UPA_DBG_ESSENTIAL
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa b-ack fail"));
#endif
    }

    asap_tot_ack_len += upa_b_ack_len;

#if UPA_DBG_SLOT_TIMING /* upaTxE0: after RADIO.read() for ACK, before tsch_radio_off() */
    upa_tx_slot_timestamp_end[0] = RTIMER_NOW();
#endif

    tsch_radio_off(TSCH_RADIO_CMD_ON_FORCE);

#if UPA_DBG_SLOT_TIMING /* upaTxE1: after tsch_radio_off() */
    upa_tx_slot_timestamp_end[1] = RTIMER_NOW();
#endif

    if(upa_tx_slot_pending_stats_and_log_update) {
      upa_tx_slot_pending_stats_and_log_update = 0;

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
#if ENABLE_LOG_TSCH_WITH_APP_FOOTER
          memcpy(&log->tx.app_magic, (uint8_t *)queuebuf_dataptr(current_packet->qb) + queuebuf_datalen(current_packet->qb) - 2, 2);
          memcpy(&log->tx.app_seqno, (uint8_t *)queuebuf_dataptr(current_packet->qb) + queuebuf_datalen(current_packet->qb) - 2 - 4, 4);
#endif
          log->tx.asap_unused_offset_time = 0;
          log->tx.asap_idle_time = 0;
          log->tx.asap_curr_slot_len = 0;
          log->tx.asap_num_of_slots_until_idle_time = 0;
          log->tx.asap_ack_len = 0;
      );
    }

    upa_tx_slot_batch_tx_ok_count = 0;
    int upa_num_of_non_zero_in_queue_pkts = 0;

    uint8_t upa_seq = 0;
    for(upa_seq = 0; upa_seq < upa_pkts_to_send; upa_seq++) {
      upa_tx_slot_packet_array[upa_seq]->transmissions++;
      upa_tx_slot_packet_array[upa_seq]->upa_sent_in_batch = 1;

      uint8_t upa_result = (upa_b_ack_received_info & (1 << upa_seq)) >> upa_seq;
      if(upa_result == 1) {
        upa_tx_slot_packet_array[upa_seq]->ret = MAC_TX_OK;
        ++upa_tx_slot_batch_tx_ok_count;
        asap_ok_packet_len += queuebuf_datalen(upa_tx_slot_packet_array[upa_seq]->qb);
      } else {
        upa_tx_slot_packet_array[upa_seq]->ret = MAC_TX_NOACK;
      }

      TSCH_LOG_ADD(tsch_log_tx,
          log->tx.mac_tx_status = upa_tx_slot_packet_array[upa_seq]->ret;
          log->tx.num_tx = upa_tx_slot_packet_array[upa_seq]->transmissions;
          log->tx.datalen = queuebuf_datalen(upa_tx_slot_packet_array[upa_seq]->qb);
          log->tx.drift = 0;
          log->tx.drift_used = 0;
          log->tx.is_data = ((((uint8_t *)(queuebuf_dataptr(upa_tx_slot_packet_array[upa_seq]->qb)))[0]) & 7) == FRAME802154_DATAFRAME;
#if LLSEC802154_ENABLED
          log->tx.sec_level = queuebuf_attr(upa_tx_slot_packet_array[upa_seq]->qb, PACKETBUF_ATTR_SECURITY_LEVEL);
#else /* LLSEC802154_ENABLED */
          log->tx.sec_level = 0;
#endif /* LLSEC802154_ENABLED */
          linkaddr_copy(&log->tx.dest, queuebuf_addr(upa_tx_slot_packet_array[upa_seq]->qb, PACKETBUF_ADDR_RECEIVER));
          log->tx.seqno = queuebuf_attr(upa_tx_slot_packet_array[upa_seq]->qb, PACKETBUF_ATTR_MAC_SEQNO);
#if ENABLE_LOG_TSCH_WITH_APP_FOOTER
          memcpy(&log->tx.app_magic, (uint8_t *)queuebuf_dataptr(upa_tx_slot_packet_array[upa_seq]->qb) + queuebuf_datalen(upa_tx_slot_packet_array[upa_seq]->qb) - 2, 2);
          memcpy(&log->tx.app_seqno, (uint8_t *)queuebuf_dataptr(upa_tx_slot_packet_array[upa_seq]->qb) + queuebuf_datalen(upa_tx_slot_packet_array[upa_seq]->qb) - 2 - 4, 4);
#endif
          log->tx.asap_unused_offset_time = 0;
          log->tx.asap_idle_time = 0;
          log->tx.asap_curr_slot_len = 0;
          log->tx.asap_num_of_slots_until_idle_time = 0;
          log->tx.asap_ack_len = 0;
      );

      upa_tx_slot_in_queue_array[upa_seq] 
          = tsch_queue_upa_packet_sent(current_neighbor, 
                                        upa_tx_slot_packet_array[upa_seq], 
                                        current_link, 
                                        upa_tx_slot_packet_array[upa_seq]->ret);

      if(upa_tx_slot_in_queue_array[upa_seq] == 0) {
        int upa_tx_slot_dequeued_index = ringbufindex_peek_put(&dequeued_ringbuf);
        if(upa_tx_slot_dequeued_index != -1) {
          dequeued_array[upa_tx_slot_dequeued_index] = upa_tx_slot_packet_array[upa_seq];
          ringbufindex_put(&dequeued_ringbuf);
        }      
      } else {
        ++upa_num_of_non_zero_in_queue_pkts;
      }
    }

    uint8_t upa_cursor = 0;
    if(upa_num_of_non_zero_in_queue_pkts > 0 ) {
      for(i = 0; i < upa_pkts_to_send; i++) {
        if(upa_tx_slot_in_queue_array[(upa_pkts_to_send - 1) - i] == 1) {
          uint8_t upa_dest_ringbuf_index = upa_tx_slot_dequeued_ringbuf_index_array[(upa_pkts_to_send - 1) - upa_cursor];
          current_neighbor->tx_array[upa_dest_ringbuf_index] = upa_tx_slot_packet_array[(upa_pkts_to_send - 1) - i];
          ++upa_cursor;
          if(upa_num_of_non_zero_in_queue_pkts == upa_cursor) {
            break;
          }
        }
      }
    }
    int upa_get_ptr_shift = upa_pkts_to_send - upa_num_of_non_zero_in_queue_pkts;
    ringbufindex_shift_get_ptr(&current_neighbor->tx_ringbuf, upa_get_ptr_shift);

#if UPA_DBG_SLOT_TIMING /* upaTxE2: after processing ACK */
    upa_tx_slot_timestamp_end[2] = RTIMER_NOW();
#endif

    current_slot_idle_start = RTIMER_NOW();
    current_slot_busy_time = RTIMER_CLOCK_DIFF(current_slot_idle_start, current_slot_start);
    current_slot_passed_slots = (current_slot_busy_time + tsch_timing[tsch_ts_timeslot_length] - 1) 
                                / tsch_timing[tsch_ts_timeslot_length];
    current_slot_idle_time = current_slot_passed_slots * tsch_timing[tsch_ts_timeslot_length] - current_slot_busy_time;

#if HCK_ASAP_EVAL_02_UPA_SINGLE_HOP && (FIXED_NUM_OF_AGGREGATED_PKTS == 0)
    if(upa_tx_slot_all_packet_len_same == 1 
      && upa_tx_slot_batch_tx_ok_count == upa_pkts_to_send) {
      eval_01_num_of_pkts_aggregated++;
      if(eval_01_num_of_pkts_aggregated > NUM_OF_MAX_AGGREGATED_PKTS) {
        eval_01_num_of_pkts_aggregated = 1;
      }
    }
#endif

    if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
      tsch_common_sf_upa_tx_ok_count += upa_tx_slot_batch_tx_ok_count;
    } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
      tsch_unicast_sf_upa_tx_ok_count += upa_tx_slot_batch_tx_ok_count;
    }
#if WITH_OST
    else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
          && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
      tsch_ost_pp_sf_upa_tx_ok_count += upa_tx_slot_batch_tx_ok_count;
    }
#endif

    process_poll(&tsch_pending_events_process);
  }
#endif /* WITH_UPA */

#if WITH_OST && OST_ON_DEMAND_PROVISION
  if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET) {
    ost_remove_matching_slot();
  }
  if(current_link->slotframe_handle == 1) { /* RB */
    int queued_pkts = ringbufindex_elements(&current_neighbor->tx_ringbuf);
    uint16_t nbr_id = OST_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(current_neighbor));

    if(ost_reserved_ssq(nbr_id) && queued_pkts == 0) { /* Tx occurs by RB before reserved ssq Tx */
      /* Remove Tx ssq by RB */
      ost_remove_reserved_ssq(nbr_id);
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

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx0: start of tsch_rx_slot */
  regular_slot_timestamp_rx[0] = RTIMER_NOW();
#endif

  TSCH_DEBUG_RX_EVENT();

  input_index = ringbufindex_peek_put(&input_ringbuf);
  if(input_index == -1) {
    /* hckim log */
    ++tsch_input_ringbuf_full_count;

    TSCH_LOG_ADD(tsch_log_message,
        snprintf(log->message, sizeof(log->message),
            "full input queue");
    );

    input_queue_drop++;
  } else {
    ++tsch_input_ringbuf_available_count;

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

    current_slot_unused_offset_start = RTIMER_NOW();

    /* Wait before starting to listen */
    TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");

    current_slot_unused_offset_end = RTIMER_NOW();
    current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

    TSCH_DEBUG_RX_EVENT();

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx1: rx_offset expired, before tsch_radio_on() */
    regular_slot_timestamp_rx[1] = RTIMER_NOW();
#endif

    /* Start radio for at least guard time */
    tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx2: after tsch_radio_on(), start to listen */
    regular_slot_timestamp_rx[2] = RTIMER_NOW();
#endif

    packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();

    if(!packet_seen) {
#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx3: rx start time */
      regular_slot_timestamp_rx[3] = RTIMER_NOW();
#endif

      /* Check if receiving within guard time */
      RTIMER_BUSYWAIT_UNTIL_ABS((packet_seen = (NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet())),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + RADIO_DELAY_BEFORE_DETECT);
    }
    if(!packet_seen) {
#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx4: no packet seen */
      regular_slot_timestamp_rx[4] = RTIMER_NOW();
#endif

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

#if WITH_UPA
      upa_rx_slot_trig_packet_start_time = rx_start_time;
#endif

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx3: rx start time */
      regular_slot_timestamp_rx[3] = rx_start_time;
#endif

      /* Wait until packet is received, turn radio off */
      RTIMER_BUSYWAIT_UNTIL_ABS(!NETSTACK_RADIO.receiving_packet(),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + tsch_timing[tsch_ts_max_tx]);
      TSCH_DEBUG_RX_EVENT();

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx4: rx end time, before tsch_radio_off() */
      regular_slot_timestamp_rx[4] = RTIMER_NOW();
#endif

      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx5: after tsch_radio_off() */
      regular_slot_timestamp_rx[5] = RTIMER_NOW();
#endif

      if(NETSTACK_RADIO.pending_packet()) {

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx6: after RADIO.pending_packet() */
        regular_slot_timestamp_rx[6] = RTIMER_NOW();
#endif

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

#if WITH_UPA
        current_input->upa_received_in_batch = 0;
        upa_rx_slot_trig_packet_len = current_input->len;
#endif

        /* OST-04-01: Parse received N */
        header_len = frame802154_parse((uint8_t *)current_input->payload, current_input->len, &frame);
        frame_valid = header_len > 0 &&
          frame802154_check_dest_panid(&frame) &&
          frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

#if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
        /* At the end of the reception, get an more accurate estimate of SFD arrival time */
        NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
#endif

        /* calculate RX duration based on input packet len */
        packet_duration = TSCH_PACKET_DURATION(current_input->len);
        /* limit packet_duration to its max value */
        packet_duration = MIN(packet_duration, tsch_timing[tsch_ts_max_tx]);

#if WITH_OST
        packet_duration = tsch_timing[tsch_ts_max_tx];
#endif

#if WITH_UPA
        upa_rx_slot_trig_packet_duration = packet_duration;
#endif

        if(!frame_valid) {
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "!failed to parse frame %u %u", header_len, current_input->len));
        }

#if HCK_DBG_REGULAR_SLOT_DETAIL
        regular_slot_rx_packet_len = current_input->len;
        regular_slot_rx_ack_required = frame.fcf.ack_required;
#endif

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

#if HCK_DBG_REGULAR_SLOT_DETAIL /* Store ack_len */
            regular_slot_rx_estimated_drift = estimated_drift;
#endif

#ifdef TSCH_CALLBACK_DO_NACK
            if(frame.fcf.ack_required) {
              do_nack = TSCH_CALLBACK_DO_NACK(current_link,
                  &source_address, &destination_address);
            }
#endif

#if WITH_SLA
            if(frame.fcf.ack_required) {
              current_input->sla_is_broadcast = 0;
            } else {
              current_input->sla_is_broadcast = 1;
            }
#endif

            if(frame.fcf.ack_required) {
              static uint8_t ack_buf[TSCH_PACKET_MAX_LEN];
              static int ack_len;

#if WITH_OST /* OST-05-01: Process received N */
              /* process received N before generate EACK */
              ost_process_rx_N(&frame, current_input);
#if OST_ON_DEMAND_PROVISION              
              uint16_t matching_slot = ost_process_rx_schedule_info(&frame);
#if WITH_OST_LOG_INFO
              TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                  "ost odp: m_slot %u", matching_slot)
              );
#endif
#endif /* OST_ON_DEMAND_PROVISION */
#endif /* WITH_OST */

#if WITH_UPA
              upa_pkts_to_receive = 0;

              if(tsch_packet_get_frame_pending(current_input->payload, current_input->len)) {
                int upa_info_requested = 0;

                if(frame.fcf.ie_list_present) {
                  struct ieee802154_ies upa_ies;
                  frame802154e_parse_information_elements(frame.payload, frame.payload_len, &upa_ies);
                  upa_info_requested = (int)upa_ies.ie_upa_info;
                } else {
                  upa_info_requested = 0;
                }

                uint8_t upa_pkts_requested = (uint8_t)((upa_info_requested >> 8) & 0xFF);
                uint8_t upa_bitmap_requested = (uint8_t)(upa_info_requested & 0xFF);

                /* consider current packet: ringbufindex_elements(&input_ringbuf) + 1 */
                int upa_empty_space_of_input_ringbuf
                        = ((int)TSCH_MAX_INCOMING_PACKETS - 1) - (ringbufindex_elements(&input_ringbuf) + 1) > 0 ?
                          ((int)TSCH_MAX_INCOMING_PACKETS - 1) - (ringbufindex_elements(&input_ringbuf) + 1) : 0;

                /* consider current packet: tsch_queue_global_packet_count() + 1 */
                int upa_empty_space_of_global_queue
                        = (int)QUEUEBUF_NUM - (tsch_queue_global_packet_count() + 1) > 0 ?
                          (int)QUEUEBUF_NUM - (tsch_queue_global_packet_count() + 1) : 0;

                int upa_min_empty_space_of_ringbuf_or_queue = 0;

                int upa_is_root = tsch_rpl_callback_is_root();
                int upa_has_no_children = tsch_rpl_callback_has_no_children();

                int upa_is_root_or_has_no_children = (upa_is_root == 1) || (upa_has_no_children == 1);
                if(upa_is_root_or_has_no_children == 1) { /* Do not consider queue length for root/end nodes */
                  upa_min_empty_space_of_ringbuf_or_queue = upa_empty_space_of_input_ringbuf;
                } else {
                  upa_min_empty_space_of_ringbuf_or_queue = (int)MIN(upa_empty_space_of_input_ringbuf, upa_empty_space_of_global_queue);
                }

                uint8_t upa_max_acceptable_pkts = MIN(upa_pkts_requested, upa_min_empty_space_of_ringbuf_or_queue);

                uint8_t i = 0;
                for(i = 0; i < TSCH_MAX_INCOMING_PACKETS; i++) {
                  upa_num_of_slots_required[i] = 0;
                }

                int upa_num_of_timeslots_for_triggering = 1;
                uint8_t upa_bit_value = (upa_bitmap_requested & (1 << 0)) >> 0;
                if(upa_bit_value == 1) {
                  upa_num_of_timeslots_for_triggering += 1;
                }
                upa_num_of_slots_required[0] = upa_num_of_timeslots_for_triggering;

                for(i = 1; i <= upa_max_acceptable_pkts; i++) {
                  upa_bit_value = (upa_bitmap_requested & (1 << i)) >> i;
                  if(upa_bit_value == 1) {
                    upa_num_of_slots_required[i] = upa_num_of_slots_required[i - 1] + 1;
                  } else {
                    upa_num_of_slots_required[i] = upa_num_of_slots_required[i - 1];
                  }
                }

#if UPA_RX_SLOT_POLICY == 0
                upa_pkts_to_receive = upa_max_acceptable_pkts;
#elif UPA_RX_SLOT_POLICY == 1
                uint16_t upa_num_of_pkts_with_max_slot_utility = 0;
                int upa_max_gain = 100;
                int upa_curr_gain;

                for(i = 1; i <= upa_max_acceptable_pkts; i++) {
                  upa_curr_gain = (1 + i) * 100 / upa_num_of_slots_required[i];
                  if((upa_curr_gain > 100) && (upa_curr_gain >= upa_max_gain)) {
                    upa_max_gain = upa_curr_gain;
                    upa_num_of_pkts_with_max_slot_utility = i;
                  }
                }

                upa_pkts_to_receive = upa_num_of_pkts_with_max_slot_utility;
#elif UPA_RX_SLOT_POLICY == 2
                uint16_t upa_num_of_max_pkts_with_gain_in_slot_utility = 0;
                int upa_curr_gain;

                for(i = 1; i <= upa_max_acceptable_pkts; i++) {
                  upa_curr_gain = (1 + i) * 100 /  upa_num_of_slots_required[i];
                  if(upa_curr_gain > 100) {
                    upa_num_of_max_pkts_with_gain_in_slot_utility = i;
                  }
                }

                upa_pkts_to_receive = upa_num_of_max_pkts_with_gain_in_slot_utility;
#endif

#if UPA_DBG_OPERATION
                TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                    "upa pol acc %d (%u %x) (%d %d %d)", 
                    upa_pkts_to_receive, 
                    upa_pkts_requested, 
                    upa_bitmap_requested,
                    upa_is_root_or_has_no_children,
                    upa_empty_space_of_input_ringbuf,
                    upa_empty_space_of_global_queue));
#endif
              }

#elif WITH_TSCH_DEFAULT_BURST_TRANSMISSION
#if MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
              /* Schedule a burst link iff the frame pending bit was set */
              int frame_pending_bit_set_or_not = tsch_packet_get_frame_pending(current_input->payload, current_input->len);
              if(frame_pending_bit_set_or_not) {
                uint16_t time_to_earliest_schedule = 0;
                if(tsch_schedule_get_next_timeslot_available_or_not(&tsch_current_asn, &time_to_earliest_schedule)) {
                  burst_link_scheduled = 1;
#if ENABLE_MODIFIED_DBT_LOG
                  TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                      "sched dbt rx succ %d %d", burst_link_scheduled, time_to_earliest_schedule));
#endif
                } else {
                  burst_link_scheduled = 0;
#if ENABLE_MODIFIED_DBT_LOG
                  TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                      "sched dbt rx coll %d %d", burst_link_scheduled, time_to_earliest_schedule));
#endif
                }
              } else {
                burst_link_scheduled = 0;
              }
#else
              /* Schedule a burst link iff the frame pending bit was set */
              burst_link_scheduled = tsch_packet_get_frame_pending(current_input->payload, current_input->len);
#endif
#if TSCH_DBT_HOLD_CURRENT_NBR
              if(burst_link_scheduled == 1) {
                burst_link_tx = 0;
                burst_link_rx = 1;
              }
#endif
#endif

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx7: before create ACK */
              regular_slot_timestamp_rx[7] = RTIMER_NOW();
#endif

#if !WITH_OST
#if WITH_UPA /* HEADER_IE_IN_DATA_AND_ACK */
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack, 
                  upa_pkts_to_receive);
#else /* Default burst transmission or no burst transmission */
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack);

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
              if(burst_link_scheduled == 1) {
                tsch_packet_set_frame_pending(ack_buf, ack_len);
              }
#endif

#endif
#else /* !WITH_OST */
#if OST_ON_DEMAND_PROVISION
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack,
                  current_input, matching_slot);
#elif WITH_UPA /* HEADER_IE_IN_DATA_AND_ACK */
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack, 
                  current_input, upa_pkts_to_receive);
#else /* Default burst transmission or no burst transmission */
              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack, 
                  current_input);

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
              if(burst_link_scheduled == 1) {
                tsch_packet_set_frame_pending(ack_buf, ack_len);
              }
#endif

#endif
#endif /* !WITH_OST */

              asap_tot_ack_len += ack_len;

#if WITH_UPA
              upa_rx_slot_trig_ack_len = ack_len;
#endif


#if HCK_DBG_REGULAR_SLOT_DETAIL /* Store ack_len */
              regular_slot_rx_ack_len = ack_len;
#endif

              if(ack_len > 0) {
#if LLSEC802154_ENABLED
                if(tsch_is_pan_secured) {
                  /* Secure ACK frame. There is only header and header IEs, therefore data len == 0. */
                  ack_len += tsch_security_secure_frame(ack_buf, ack_buf, ack_len, 0, &tsch_current_asn);
                }
#endif /* LLSEC802154_ENABLED */

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx8: before RADIO.prepare() */
                regular_slot_timestamp_rx[8] = RTIMER_NOW();
#endif

                /* Copy to radio buffer */
                NETSTACK_RADIO.prepare((const void *)ack_buf, ack_len);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx9: after RADIO.prepare() */
                regular_slot_timestamp_rx[9] = RTIMER_NOW();
#endif

                current_slot_unused_offset_start = RTIMER_NOW();

                /* Wait for time to ACK and transmit ACK */
                TSCH_SCHEDULE_AND_YIELD(pt, t, rx_start_time,
                                        packet_duration + tsch_timing[tsch_ts_tx_ack_delay] - RADIO_DELAY_BEFORE_TX, "RxBeforeAck");

                current_slot_unused_offset_end = RTIMER_NOW();
                current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

                TSCH_DEBUG_RX_EVENT();

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx10: before RADIO.transmit() */
                regular_slot_timestamp_rx[10] = RTIMER_NOW();
#endif

                NETSTACK_RADIO.transmit(ack_len);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx11: after RADIO.transmit(), before tsch_radio_off() */
                regular_slot_timestamp_rx[11] = RTIMER_NOW();
#endif

                tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx12: after tsch_radio_off() */
                regular_slot_timestamp_rx[12] = RTIMER_NOW();
#endif

#if WITH_SLA /* Coordinator: record transmitted ACK length */
                if(tsch_is_coordinator && ack_len != 0) {
                  sla_record_ack_len(ack_len);
                }
#endif
#if WITH_UPA
                if(tsch_packet_get_frame_pending(current_input->payload, current_input->len)
                  && upa_pkts_to_receive > 0) {
                  upa_link_scheduled = 1;
                } else {
                  upa_link_scheduled = 0;
                  upa_pkts_to_receive = 0;
                }
#elif WITH_TSCH_DEFAULT_BURST_TRANSMISSION
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

#if WITH_UPA
            if(upa_link_scheduled) {
              upa_rx_slot_nbr = n;
            }
#endif
            /* Add current input to ringbuf */
            ringbufindex_put(&input_ringbuf);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegRx13: process rx result */
            regular_slot_timestamp_rx[13] = RTIMER_NOW();
#endif

            current_slot_idle_start = RTIMER_NOW();
            current_slot_busy_time = RTIMER_CLOCK_DIFF(current_slot_idle_start, current_slot_start);
            current_slot_passed_slots = (current_slot_busy_time + tsch_timing[tsch_ts_timeslot_length] - 1) 
                                        / tsch_timing[tsch_ts_timeslot_length];
            current_slot_idle_time = current_slot_passed_slots * tsch_timing[tsch_ts_timeslot_length] - current_slot_busy_time;

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
              log->rx.rssi = current_input->rssi;
#if ENABLE_LOG_TSCH_WITH_APP_FOOTER
              memcpy(&log->rx.app_magic, (uint8_t *)current_input->payload + current_input->len - 2, 2);
              memcpy(&log->rx.app_seqno, (uint8_t *)current_input->payload + current_input->len - 2 - 4, 4);
#endif
#if WITH_UPA
              log->rx.asap_unused_offset_time = upa_link_scheduled == 0 ? RTIMERTICKS_TO_US(current_slot_unused_offset_time) : 0;
              log->rx.asap_idle_time = upa_link_scheduled == 0 ? RTIMERTICKS_TO_US(current_slot_idle_time) : 0;
              log->rx.asap_curr_slot_len = upa_link_scheduled == 0 ? tsch_timing_us[tsch_ts_timeslot_length] : 0;
              log->rx.asap_num_of_slots_until_idle_time = upa_link_scheduled == 0 ? current_slot_passed_slots : 0;
              log->rx.asap_ack_len = upa_link_scheduled == 0 ? asap_tot_ack_len : 0;
#else
              log->rx.asap_unused_offset_time = RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->rx.asap_idle_time = RTIMERTICKS_TO_US(current_slot_idle_time);
              log->rx.asap_curr_slot_len = tsch_timing_us[tsch_ts_timeslot_length];
              log->rx.asap_num_of_slots_until_idle_time = current_slot_passed_slots;
              log->rx.asap_ack_len = asap_tot_ack_len;
#endif
            );

#if !WITH_TSCH_DEFAULT_BURST_TRANSMISSION
            /* hckim measure rx operation counts */
            if(current_link->slotframe_handle == TSCH_SCHED_EB_SF_HANDLE) {
              ++tsch_eb_sf_rx_operation_count;
            } else if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
              ++tsch_common_sf_rx_operation_count;
            } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
              ++tsch_unicast_sf_rx_operation_count;
            }
#if WITH_OST
            else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
              && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
              ++tsch_ost_pp_sf_rx_operation_count;
            } else if(current_link->slotframe_handle > OST_ONDEMAND_SF_ID_OFFSET) {
              ++tsch_ost_odp_sf_rx_operation_count;
            }
#endif
#else /*  !WITH_TSCH_DEFAULT_BURST_TRANSMISSION */
            /* hckim measure rx operation counts */
            if(!is_burst_slot) {
              if(current_link->slotframe_handle == TSCH_SCHED_EB_SF_HANDLE) {
                ++tsch_eb_sf_rx_operation_count;
              } else if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
                ++tsch_common_sf_rx_operation_count;
              } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
                ++tsch_unicast_sf_rx_operation_count;
              } 
#if WITH_OST
              else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
                && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
                ++tsch_ost_pp_sf_rx_operation_count;
              }
#endif
            } else {
              if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
                ++tsch_common_sf_bst_rx_operation_count;
              } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
                ++tsch_unicast_sf_bst_rx_operation_count;
              } 
#if WITH_OST
              else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
                && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
                ++tsch_ost_pp_sf_bst_rx_operation_count;
              }
#endif
            }
#endif
          }

          /* Poll process for processing of pending input and logs */
          process_poll(&tsch_pending_events_process);
        }
      }
#if ENABLE_LOG_TSCH_SLOT_LEVEL_RX_LOG
      else {
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "!no pending packet"));
      }
#endif

      tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

    }

    if(input_queue_drop != 0) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
              "!queue full skipped %u", input_queue_drop);
      );
      input_queue_drop = 0;
    }
  }

#if WITH_UPA
  if(upa_link_scheduled) {
#if UPA_DBG_SLOT_TIMING /* UPARxB0: start of tsch_upa_rx_slot, before set offset */
    upa_print_rx_slot_timing = 1;
    upa_rx_slot_timestamp_begin[0] = RTIMER_NOW();
#endif

    upa_rx_slot_all_packet_len_same = 1;

    asap_ok_packet_len += upa_rx_slot_trig_packet_len;

    upa_rx_slot_trig_ack_start_time = upa_rx_slot_trig_packet_start_time 
                                    + upa_rx_slot_trig_packet_duration 
                                    + tsch_timing[tsch_ts_tx_ack_delay];
    upa_rx_slot_trig_ack_duration = TSCH_PACKET_DURATION(upa_rx_slot_trig_ack_len);
    upa_rx_slot_batch_rx_start_time = upa_rx_slot_trig_ack_start_time 
                                    + upa_rx_slot_trig_ack_duration;

    upa_rx_slot_batch_rx_ok_count = 0;

#if UPA_DBG_SLOT_TIMING /* UPARxB1: start of tsch_upa_rx_slot, before tsch_radio_on() */
    upa_rx_slot_timestamp_begin[1] = RTIMER_NOW();
#endif

    tsch_radio_on(TSCH_RADIO_CMD_ON_FORCE);

#if UPA_DBG_SLOT_TIMING /* UPARxB2: after tsch_radio_on() */
    upa_rx_slot_timestamp_begin[2] = RTIMER_NOW();
#endif

    upa_rx_slot_last_in_batch_seq = 0;
    upa_rx_slot_b_ack_bitmap = 0;

    while(1) {
#if UPA_DBG_OPERATION
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa rx begin (last seq %u)", upa_rx_slot_last_in_batch_seq));
#endif

      upa_rx_slot_curr_input_ringbuf_index = ringbufindex_peek_put(&input_ringbuf);
      if(upa_rx_slot_curr_input_ringbuf_index != -1) {
        ++tsch_input_ringbuf_available_count;

        upa_rx_slot_curr_input = &input_array[upa_rx_slot_curr_input_ringbuf_index];

#if UPA_DBG_SLOT_TIMING /* UPARxR0: set ts_rx_offset */
        upa_rx_slot_timestamp_rx[upa_rx_slot_batch_rx_ok_count][0] = RTIMER_NOW();
#endif

        if(upa_rx_slot_last_in_batch_seq == 0) {
          upa_rx_slot_curr_start = upa_rx_slot_batch_rx_start_time;
          upa_rx_slot_curr_offset = upa_timing[upa_ts_rx_offset_1];
          upa_rx_slot_curr_timeout = (upa_timing[upa_ts_tx_offset_1] + upa_timing[upa_ts_max_tx]) 
                                    + (upa_timing[upa_ts_tx_offset_2] + upa_timing[upa_ts_max_tx]) * (upa_pkts_to_receive - 1);

          current_slot_unused_offset_start = RTIMER_NOW();

          TSCH_SCHEDULE_AND_YIELD(pt, t, upa_rx_slot_curr_start, upa_rx_slot_curr_offset, "upaRx1");

          current_slot_unused_offset_end = RTIMER_NOW();
          current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);
        } else {
          upa_rx_slot_curr_start = upa_rx_slot_last_valid_reception_start + upa_rx_slot_last_valid_duration;
          upa_rx_slot_curr_offset = upa_timing[upa_ts_rx_offset_2];
          upa_rx_slot_curr_timeout = (upa_timing[upa_ts_tx_offset_2] + upa_timing[upa_ts_max_tx]) 
                                      * (upa_pkts_to_receive - upa_rx_slot_last_in_batch_seq);

          current_slot_unused_offset_start = RTIMER_NOW();

          TSCH_SCHEDULE_AND_YIELD(pt, t, upa_rx_slot_curr_start, upa_rx_slot_curr_offset, "upaRx2");

          current_slot_unused_offset_end = RTIMER_NOW();
          current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);
        }

#if UPA_DBG_SLOT_TIMING /* UPARxR1: start to listen */
        upa_rx_slot_timestamp_rx[upa_rx_slot_batch_rx_ok_count][1] = RTIMER_NOW();
#endif

        uint8_t upa_packet_seen;
        upa_packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();
        if(!upa_packet_seen) {
          RTIMER_BUSYWAIT_UNTIL_ABS((upa_packet_seen = (NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet())),
              upa_rx_slot_curr_start, upa_rx_slot_curr_timeout + RADIO_DELAY_BEFORE_DETECT);
        }

        if(!upa_packet_seen) {
          upa_rx_slot_all_reception_end = upa_rx_slot_curr_start + upa_rx_slot_curr_timeout;

#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "upa rx completed (timeout)"));
#endif
          break; /* finish while loop of this upa_rx_slot */

        } else {
          upa_rx_slot_curr_reception_start = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

#if UPA_DBG_SLOT_TIMING /* UPARxR2: rx start time */
          upa_rx_slot_timestamp_rx[upa_rx_slot_batch_rx_ok_count][2] = upa_rx_slot_curr_reception_start;
#endif

          RTIMER_BUSYWAIT_UNTIL_ABS(!NETSTACK_RADIO.receiving_packet(),
              upa_rx_slot_curr_reception_start, upa_timing[upa_ts_max_tx]);

          if(NETSTACK_RADIO.pending_packet()) {
#if UPA_DBG_SLOT_TIMING /* UPARxR3: rx end time */
            upa_rx_slot_timestamp_rx[upa_rx_slot_batch_rx_ok_count][3] = RTIMER_NOW();
#endif
            radio_value_t upa_radio_last_rssi;
            radio_value_t upa_radio_last_lqi;

            upa_rx_slot_curr_input->len = NETSTACK_RADIO.read((void *)upa_rx_slot_curr_input->payload, TSCH_PACKET_MAX_LEN);
            NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_RSSI, &upa_radio_last_rssi);
            upa_rx_slot_curr_input->rx_asn = tsch_current_asn;
            upa_rx_slot_curr_input->rssi = (signed)upa_radio_last_rssi;
            upa_rx_slot_curr_input->channel = tsch_current_channel;

            upa_rx_slot_curr_input->upa_received_in_batch = 1;

            upa_rx_slot_header_len = frame802154_parse((uint8_t *)upa_rx_slot_curr_input->payload, 
                                                        upa_rx_slot_curr_input->len, 
                                                        &upa_rx_slot_frame);
            upa_rx_slot_frame_valid = upa_rx_slot_header_len > 0 &&
              frame802154_check_dest_panid(&upa_rx_slot_frame) &&
              frame802154_extract_linkaddr(&upa_rx_slot_frame, &upa_rx_slot_src_addr, &upa_rx_slot_dest_addr);

            upa_rx_slot_curr_duration = TSCH_PACKET_DURATION(upa_rx_slot_curr_input->len);
            upa_rx_slot_curr_duration = MIN(upa_rx_slot_curr_duration, upa_timing[upa_ts_max_tx]);

            if(!upa_rx_slot_frame_valid) {
              TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                  "!upa failed to parse frame %u %u", upa_rx_slot_header_len, upa_rx_slot_curr_input->len));
            }

            if(upa_rx_slot_frame_valid) {
              if(upa_rx_slot_frame.fcf.frame_type != FRAME802154_DATAFRAME) {
                  TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                      "!upa discarding frame with type %u, len %u", upa_rx_slot_frame.fcf.frame_type, upa_rx_slot_curr_input->len));
                  upa_rx_slot_frame_valid = 0;
              }
            }

            if(upa_rx_slot_frame_valid) {
              if(linkaddr_cmp(&upa_rx_slot_dest_addr, &linkaddr_node_addr)
                && linkaddr_cmp(&upa_rx_slot_src_addr, &source_address)
                && !linkaddr_cmp(&upa_rx_slot_src_addr, &linkaddr_node_addr)) {
                rx_count++;

                struct ieee802154_ies upa_ies;
                frame802154e_parse_information_elements(upa_rx_slot_frame.payload, upa_rx_slot_frame.payload_len, &upa_ies);
                uint16_t upa_received_in_batch_seq = upa_ies.ie_upa_info;
                upa_rx_slot_b_ack_bitmap = upa_rx_slot_b_ack_bitmap | (1 << (upa_received_in_batch_seq - 1));

#if UPA_DBG_OPERATION
                TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                    "upa rx seq %u -> %u", upa_received_in_batch_seq, upa_rx_slot_b_ack_bitmap));
#endif

                ++upa_rx_slot_batch_rx_ok_count;

                if(upa_rx_slot_trig_packet_len != upa_rx_slot_curr_input->len) {
                  upa_rx_slot_all_packet_len_same = 0;
                }

                asap_ok_packet_len += upa_rx_slot_curr_input->len;

                /* update upa seq and timing only if upa_rx_slot_frame_valid == 1 */
                upa_rx_slot_last_in_batch_seq = upa_received_in_batch_seq;
                upa_rx_slot_last_valid_reception_start = upa_rx_slot_curr_reception_start;
                upa_rx_slot_last_valid_duration = upa_rx_slot_curr_duration;

                ringbufindex_put(&input_ringbuf);

                if(upa_rx_slot_nbr != NULL) {
                  NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_LINK_QUALITY, &upa_radio_last_lqi);
                  tsch_stats_rx_packet(upa_rx_slot_nbr, upa_rx_slot_curr_input->rssi, upa_radio_last_lqi, tsch_current_channel);
                }

                TSCH_LOG_ADD(tsch_log_rx,
                  linkaddr_copy(&log->rx.src, (linkaddr_t *)&upa_rx_slot_frame.src_addr);
                  log->rx.is_unicast = upa_rx_slot_frame.fcf.ack_required;
                  log->rx.datalen = upa_rx_slot_curr_input->len;
                  log->rx.drift = 0;
                  log->rx.drift_used = 0;
                  log->rx.is_data = upa_rx_slot_frame.fcf.frame_type == FRAME802154_DATAFRAME;
                  log->rx.sec_level = upa_rx_slot_frame.aux_hdr.security_control.security_level;
                  log->rx.estimated_drift = 0;
                  log->rx.seqno = upa_rx_slot_frame.seq;
                  log->rx.rssi = upa_rx_slot_curr_input->rssi;
#if ENABLE_LOG_TSCH_WITH_APP_FOOTER
                  memcpy(&log->rx.app_magic, (uint8_t *)upa_rx_slot_curr_input->payload + upa_rx_slot_curr_input->len - 2, 2);
                  memcpy(&log->rx.app_seqno, (uint8_t *)upa_rx_slot_curr_input->payload + upa_rx_slot_curr_input->len - 2 - 4, 4);
#endif
                  log->rx.asap_unused_offset_time = 0;
                  log->rx.asap_idle_time = 0;
                  log->rx.asap_curr_slot_len = 0;
                  log->rx.asap_num_of_slots_until_idle_time = 0;
                  log->rx.asap_ack_len = 0;
                );
              }
            }
          }

#if UPA_DBG_ESSENTIAL
          else {
            TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                "!upa no pending packet"));
          }
#endif
        }
      } else { /* upa_rx_slot_curr_input_ringbuf_index == -1 */
        /* hckim log */
        ++tsch_input_ringbuf_full_count;

#if UPA_DBG_ESSENTIAL
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "!upa rx fail (last seq %u, full buf)", upa_rx_slot_last_in_batch_seq));
#endif
        break;
      }

#if UPA_DBG_OPERATION
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa rx end (last seq %u)", upa_rx_slot_last_in_batch_seq));
#endif

      upa_rx_slot_curr_deadline = upa_rx_slot_curr_start + upa_rx_slot_curr_timeout;

      if(upa_rx_slot_last_in_batch_seq >= upa_pkts_to_receive
        || !RTIMER_CLOCK_LT(RTIMER_NOW(), upa_rx_slot_curr_deadline + RADIO_DELAY_BEFORE_DETECT)) {
        if(upa_rx_slot_last_in_batch_seq >= upa_pkts_to_receive) {
          upa_rx_slot_all_reception_end = upa_rx_slot_curr_reception_start + upa_rx_slot_curr_duration;

#if UPA_DBG_OPERATION
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "upa rx completed (final seq)"));
#endif
        } else {
          upa_rx_slot_all_reception_end = upa_rx_slot_curr_deadline;

#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "upa rx completed (deadline)"));
#endif
        }
        break;
      }
    }

    upa_rx_slot_curr_offset = upa_timing[upa_ts_tx_ack_delay];

#if UPA_DBG_SLOT_TIMING /* UPARxA0: before create ACK */
    upa_rx_slot_timestamp_ack[0] = RTIMER_NOW();
#endif

#if WITH_OST
    upa_rx_slot_b_ack_len = tsch_packet_create_eack(upa_rx_slot_b_ack_buf, sizeof(upa_rx_slot_b_ack_buf),
        &upa_rx_slot_src_addr, /* frame.seq */0, /* estimated_drift */0, /* do_nack */0, NULL, upa_rx_slot_b_ack_bitmap);
#else
    upa_rx_slot_b_ack_len = tsch_packet_create_eack(upa_rx_slot_b_ack_buf, sizeof(upa_rx_slot_b_ack_buf),
        &upa_rx_slot_src_addr, /* frame.seq */0, /* estimated_drift */0, /* do_nack */0, upa_rx_slot_b_ack_bitmap);
#endif

    if(upa_rx_slot_b_ack_len > 0) {

#if UPA_DBG_SLOT_TIMING /* UPARxA1: after create ACK, before RADIO.prepare() */
      upa_rx_slot_timestamp_ack[1] = RTIMER_NOW();
#endif

      NETSTACK_RADIO.prepare((const void *)upa_rx_slot_b_ack_buf, upa_rx_slot_b_ack_len);

#if UPA_DBG_SLOT_TIMING /* UPARxA2: after RADIO.prepare() */
      upa_rx_slot_timestamp_ack[2] = RTIMER_NOW();
#endif

      current_slot_unused_offset_start = RTIMER_NOW();

      TSCH_SCHEDULE_AND_YIELD(pt, t, upa_rx_slot_all_reception_end,
                              upa_rx_slot_curr_offset - RADIO_DELAY_BEFORE_TX, "upaRx3");

      current_slot_unused_offset_end = RTIMER_NOW();
      current_slot_unused_offset_time += RTIMER_CLOCK_DIFF(current_slot_unused_offset_end, current_slot_unused_offset_start);

#if UPA_DBG_SLOT_TIMING /* UPARxA3: before RADIO.transmit() for ACK */
      upa_rx_slot_timestamp_ack[3] = RTIMER_NOW();
#endif

      NETSTACK_RADIO.transmit(upa_rx_slot_b_ack_len);

    }

    asap_tot_ack_len += upa_rx_slot_b_ack_len;

    upa_rx_slot_nbr = NULL;

#if UPA_DBG_SLOT_TIMING /* UPARxE0: after RADIO.transmit() for ACK, before tsch_radio_off() */
    upa_rx_slot_timestamp_end[0] = RTIMER_NOW();
#endif

    tsch_radio_off(TSCH_RADIO_CMD_ON_FORCE);

#if UPA_DBG_SLOT_TIMING /* UPARxE1: after tsch_radio_off() */
    upa_rx_slot_timestamp_end[1] = RTIMER_NOW();
#endif

    current_slot_idle_start = RTIMER_NOW();
    current_slot_busy_time = RTIMER_CLOCK_DIFF(current_slot_idle_start, current_slot_start);
    current_slot_passed_slots = (current_slot_busy_time + tsch_timing[tsch_ts_timeslot_length] - 1) 
                                / tsch_timing[tsch_ts_timeslot_length];
    current_slot_idle_time = current_slot_passed_slots * tsch_timing[tsch_ts_timeslot_length] - current_slot_busy_time;

    if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
      tsch_common_sf_upa_rx_ok_count += upa_rx_slot_batch_rx_ok_count;
    } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
      tsch_unicast_sf_upa_rx_ok_count += upa_rx_slot_batch_rx_ok_count;
    }
#if WITH_OST
    else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
          && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
      tsch_ost_pp_sf_upa_rx_ok_count += upa_rx_slot_batch_rx_ok_count;
    }
#endif

    process_poll(&tsch_pending_events_process);
  }
#endif

#if WITH_OST && OST_ON_DEMAND_PROVISION
  if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET) {
    ost_remove_matching_slot();
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

#if WITH_SLA /* Coordinator/non-coordinator: at triggering asn, apply next timeslot length */
    if((tsch_current_asn.ls4b == sla_triggering_asn.ls4b) 
        && (tsch_current_asn.ms1b == sla_triggering_asn.ms1b)) {
      uint16_t sla_prev_timeslot_length = tsch_timing_us[tsch_ts_timeslot_length];
      if(tsch_timing_us[tsch_ts_timeslot_length] != sla_next_timeslot_length) {
        sla_apply_next_timeslot_length();

#if SLA_DBG_ESSENTIAL
        TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "APL apply_ts c_ts %u p_ts %u HK-S",
                            tsch_timing_us[tsch_ts_timeslot_length], 
                            sla_prev_timeslot_length);
        );
#endif
      } else {
#if SLA_DBG_ESSENTIAL
        TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "APL not_apply_ts c_ts %u p_ts %u HK-S",
                            tsch_timing_us[tsch_ts_timeslot_length], 
                            sla_prev_timeslot_length);
        );
#endif
      }

      sla_finish_rapid_eb_broadcasting();
      sla_in_guard_time = 0;

    } else if((int32_t)(TSCH_ASN_DIFF(sla_triggering_asn, tsch_current_asn)) > 0) {
      // SLA-TODO: needs to consider ASN overflow
      if((int32_t)(TSCH_ASN_DIFF(sla_triggering_asn, tsch_current_asn) <= SLA_GUARD_TIME_TIMESLOTS)) {
        if(tsch_timing_us[tsch_ts_timeslot_length] != sla_next_timeslot_length) {
          sla_in_guard_time = 1;
        } else {
          sla_in_guard_time = 0;
        }
      } else {
        sla_in_guard_time = 0;
      }
    } else {
      sla_in_guard_time = 0;
    }
#endif

#if WITH_UPA
    if(upa_schedule_after_upa_link) { /* Successful scheduling after UPA */
      upa_schedule_after_upa_link = 0;
      if(upa_pkts_to_send > 0) {
        if(upa_link_result_case == 1) {
          tsch_common_sf_upa_tx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(upa_log_result,
              log->upa_result.upa_is_overflowed = 0;
              log->upa_result.upa_link_type = upa_link_result_case;
              log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_send;
              log->upa_result.upa_num_of_successful_pkts = upa_tx_slot_batch_tx_ok_count;
              log->upa_result.upa_trig_pkt_len = upa_tx_slot_trig_packet_len;
              log->upa_result.upa_all_pkt_len_same = upa_tx_slot_all_packet_len_same;
              log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
              log->upa_result.upa_tot_pkt_len = asap_tot_packet_len;
              log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
              log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
              log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
              log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
              log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
              log->upa_result.upa_num_of_expected_slots = upa_expected_passed_timeslots_except_first_slot + 1;
          );
#endif
        } else if(upa_link_result_case == 3) {
          tsch_unicast_sf_upa_tx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(upa_log_result,
              log->upa_result.upa_is_overflowed = 0;
              log->upa_result.upa_link_type = upa_link_result_case;
              log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_send;
              log->upa_result.upa_num_of_successful_pkts = upa_tx_slot_batch_tx_ok_count;
              log->upa_result.upa_trig_pkt_len = upa_tx_slot_trig_packet_len;
              log->upa_result.upa_all_pkt_len_same = upa_tx_slot_all_packet_len_same;
              log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
              log->upa_result.upa_tot_pkt_len = asap_tot_packet_len;
              log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
              log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
              log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
              log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
              log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
              log->upa_result.upa_num_of_expected_slots = upa_expected_passed_timeslots_except_first_slot + 1;
          );
#endif
        }
#if WITH_OST
        else if(upa_link_result_case == 5) {
          tsch_ost_pp_sf_upa_tx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(upa_log_result,
              log->upa_result.upa_is_overflowed = 0;
              log->upa_result.upa_link_type = upa_link_result_case;
              log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_send;
              log->upa_result.upa_num_of_successful_pkts = upa_tx_slot_batch_tx_ok_count;
              log->upa_result.upa_trig_pkt_len = upa_tx_slot_trig_packet_len;
              log->upa_result.upa_all_pkt_len_same = upa_tx_slot_all_packet_len_same;
              log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
              log->upa_result.upa_tot_pkt_len = asap_tot_packet_len;
              log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
              log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
              log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
              log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
              log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
              log->upa_result.upa_num_of_expected_slots = upa_expected_passed_timeslots_except_first_slot + 1;
          );
#endif
        }
        upa_tx_slot_trig_packet_len = 0;
        upa_tx_slot_all_packet_len_same = 0;
#endif
      } else {
        if(upa_link_result_case == 2) {
          tsch_common_sf_upa_rx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(upa_log_result,
              log->upa_result.upa_is_overflowed = 0;
              log->upa_result.upa_link_type = upa_link_result_case;
              log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_receive;
              log->upa_result.upa_num_of_successful_pkts = upa_rx_slot_batch_rx_ok_count;
              log->upa_result.upa_trig_pkt_len = upa_rx_slot_trig_packet_len;
              log->upa_result.upa_all_pkt_len_same = upa_rx_slot_all_packet_len_same;
              log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
              log->upa_result.upa_tot_pkt_len = 0;
              log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
              log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
              log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
              log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
              log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
              log->upa_result.upa_num_of_expected_slots = 0;
          );
#endif
        }
        else if(upa_link_result_case == 4) {
          tsch_unicast_sf_upa_rx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(upa_log_result,
              log->upa_result.upa_is_overflowed = 0;
              log->upa_result.upa_link_type = upa_link_result_case;
              log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_receive;
              log->upa_result.upa_num_of_successful_pkts = upa_rx_slot_batch_rx_ok_count;
              log->upa_result.upa_trig_pkt_len = upa_rx_slot_trig_packet_len;
              log->upa_result.upa_all_pkt_len_same = upa_rx_slot_all_packet_len_same;
              log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
              log->upa_result.upa_tot_pkt_len = 0;
              log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
              log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
              log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
              log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
              log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
              log->upa_result.upa_num_of_expected_slots = 0;
          );
#endif
        }
#if WITH_OST
        else if(upa_link_result_case == 6) {
          tsch_ost_pp_sf_upa_rx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
          TSCH_LOG_ADD(upa_log_result,
              log->upa_result.upa_is_overflowed = 0;
              log->upa_result.upa_link_type = upa_link_result_case;
              log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_receive;
              log->upa_result.upa_num_of_successful_pkts = upa_rx_slot_batch_rx_ok_count;
              log->upa_result.upa_trig_pkt_len = upa_rx_slot_trig_packet_len;
              log->upa_result.upa_all_pkt_len_same = upa_rx_slot_all_packet_len_same;
              log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
              log->upa_result.upa_tot_pkt_len = 0;
              log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
              log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
              log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
              log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
              log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
              log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
              log->upa_result.upa_num_of_expected_slots = 0;
          );
#endif
        }
        upa_rx_slot_trig_packet_len = 0;
        upa_rx_slot_all_packet_len_same = 0;
#endif
      }

      upa_pkts_to_send = 0;
      upa_pkts_to_receive = 0;
      asap_curr_passed_timeslots = 0;
      asap_curr_passed_timeslots_except_first_slot = 0;
    }
#endif

    TSCH_ASN_COPY(tsch_last_valid_asn, tsch_current_asn);
    last_valid_asn_start = current_slot_start;

    if(current_link == NULL || tsch_lock_requested
#if WITH_SLA /* Coordinator/non-coordinator: set sla guard time */
      || sla_in_guard_time
#endif
      ) { /* Skip slot operation if there is no link
                                                          or if there is a pending request for getting the lock */
      /* Issue a log whenever skipping a slot */

#if WITH_SLA && SLA_DBG_ESSENTIAL
      TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "!skipped slot %u %u %u SKIP %u HK-S",
                            tsch_locked,
                            tsch_lock_requested,
                            current_link == NULL,
                            sla_in_guard_time);
      );
#else
      TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "!skipped slot %u %u %u",
                            tsch_locked,
                            tsch_lock_requested,
                            current_link == NULL);
      );
#endif

#if WITH_OST && OST_ON_DEMAND_PROVISION
      if(ost_exist_matching_slot(&tsch_current_asn)) {
        ost_remove_matching_slot();

        TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "!skipped ost odp");
        );
      }
#endif 

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && TSCH_DBT_HANDLE_SKIPPED_DBT_SLOT
      if(burst_link_scheduled) {
        burst_link_scheduled = 0;
        tsch_current_burst_count = 0;
#if TSCH_DBT_HOLD_CURRENT_NBR
        burst_link_tx = 0;
        burst_link_rx = 0;
#endif
        TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "!skipped dbt");
        );
      }
#endif

    } else {
      int is_active_slot;
      TSCH_DEBUG_SLOT_START();
      tsch_in_slot_operation = 1;
      /* Measure on-air noise level while TSCH is idle */
      tsch_stats_sample_rssi();
      /* Reset drift correction */
      drift_correction = 0;
      is_drift_correction_used = 0;

      current_slot_unused_offset_start = 0;
      current_slot_unused_offset_end = 0;
      current_slot_unused_offset_time = 0;
      current_slot_idle_start = 0;
      current_slot_busy_time = 0;
      current_slot_idle_time = 0;
      current_slot_passed_slots = 0;
      asap_tot_packet_len = 0;
      asap_ok_packet_len = 0;
      asap_tot_ack_len = 0;

#if WITH_OST && OST_ON_DEMAND_PROVISION
      if(current_link->slotframe_handle > SSQ_SCHEDULE_HANDLE_OFFSET 
        && current_link->link_options == LINK_OPTION_TX) {
        if(current_packet == NULL) {
          if(tsch_is_locked()) { /* tsch-schedule is being changing, so locked - skip this slot */
            ost_remove_matching_slot();
          } else {
            /* ERROR: ssq Tx schedule, but no packets to Tx */
          }
        }
      }
#endif

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && TSCH_DBT_HOLD_CURRENT_NBR
      if(burst_link_scheduled && burst_link_tx) {
        current_packet = tsch_queue_burst_get_next_packet_for_nbr(current_neighbor);
      } else if(burst_link_scheduled && burst_link_rx) {
        current_packet = NULL;
      } else {
        /* Get a packet ready to be sent */
        current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);

      }
#else
      /* Get a packet ready to be sent */
      current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
#endif

#if HCK_APPLY_LATEST_CONTIKI
      uint8_t do_skip_best_link = 0;
      if(current_packet == NULL && backup_link != NULL) {
        /* There is no packet to send, and this link does not have Rx flag. Instead of doing
         * nothing, switch to the backup link (has Rx flag) if any
         * and if the current link cannot Rx or both links can Rx, but the backup link has priority. */
        if(!(current_link->link_options & LINK_OPTION_RX)
            || backup_link->slotframe_handle < current_link->slotframe_handle) {
          do_skip_best_link = 1;
        }
      }

      if(do_skip_best_link) {
        /* skipped a Tx link, refresh its backoff */
        update_link_backoff(current_link);

        current_link = backup_link;
        current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
      }
#else /* HCK_APPLY_LATEST_CONTIKI */
      /* There is no packet to send, and this link does not have Rx flag. Instead of doing
        * nothing, switch to the backup link (has Rx flag) if any. */
      if(current_packet == NULL && !(current_link->link_options & LINK_OPTION_RX) && backup_link != NULL) {
        current_link = backup_link;
        current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
      }

#if WITH_OST
      /* Seungbeom Jeong added this else if block */
      /* In the case of there is no packet to send in the current best link,
          even if there is Rx option in the current best link,
          the backup link without Tx option can have slotframe handle smaller than the current best link
          -> then, the backup link must be executed */
      else if(current_packet == NULL && (current_link->link_options & LINK_OPTION_RX) && backup_link != NULL) {
        if(current_link->slotframe_handle > backup_link->slotframe_handle) {
          /* There could be Tx option in backup link */
          current_link = backup_link;
          current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
        }
      }
#endif
#endif /* HCK_APPLY_LATEST_CONTIKI */


#if !WITH_TSCH_DEFAULT_BURST_TRANSMISSION
      /* hckim measure cell utilization from current_link */
      if(current_link->slotframe_handle == TSCH_SCHED_EB_SF_HANDLE) {
        ++tsch_scheduled_eb_sf_cell_count;
      } else if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
        ++tsch_scheduled_common_sf_cell_count;
      } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
        ++tsch_scheduled_unicast_sf_cell_count;
      }
#if WITH_OST
      else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
            && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
        if(current_link->link_options & LINK_OPTION_TX) {
          ++tsch_scheduled_ost_pp_sf_tx_cell_count;
        } else if(current_link->link_options & LINK_OPTION_RX) {
          ++tsch_scheduled_ost_pp_sf_rx_cell_count;
        }
      } else if(current_link->slotframe_handle > OST_ONDEMAND_SF_ID_OFFSET) {
        if(current_link->link_options & LINK_OPTION_TX) {
          ++tsch_scheduled_ost_odp_sf_tx_cell_count;
        } else if(current_link->link_options & LINK_OPTION_RX) {
          ++tsch_scheduled_ost_odp_sf_rx_cell_count;
        }
      }
#endif
#elif WITH_TSCH_DEFAULT_BURST_TRANSMISSION
      if(!burst_link_scheduled) {
        if(current_link->slotframe_handle == TSCH_SCHED_EB_SF_HANDLE) {
          ++tsch_scheduled_eb_sf_cell_count;
        } else if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
          ++tsch_scheduled_common_sf_cell_count;
        } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
          ++tsch_scheduled_unicast_sf_cell_count;
        }
#if WITH_OST
        else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
              && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
          if(current_link->link_options & LINK_OPTION_TX) {
            ++tsch_scheduled_ost_pp_sf_tx_cell_count;
          } else if(current_link->link_options & LINK_OPTION_RX) {
            ++tsch_scheduled_ost_pp_sf_rx_cell_count;
          }
        }
#endif
      } else {
        if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
          if(burst_link_tx) {
            ++tsch_scheduled_common_sf_bst_tx_cell_count;
          } else if(burst_link_rx) {
            ++tsch_scheduled_common_sf_bst_rx_cell_count;
          }
        } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
          if(burst_link_tx) {
            ++tsch_scheduled_unicast_sf_bst_tx_cell_count;
          } else if(burst_link_rx) {
            ++tsch_scheduled_unicast_sf_bst_rx_cell_count;
          }
        }
#if WITH_OST
        else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
              && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
          if(current_link->link_options & LINK_OPTION_TX) {
            ++tsch_scheduled_ost_pp_sf_bst_tx_cell_count;
          } else if(current_link->link_options & LINK_OPTION_RX) {
            ++tsch_scheduled_ost_pp_sf_bst_rx_cell_count;
          }
        }
#endif
      }
#endif

#if HCK_DBG_REGULAR_SLOT_DETAIL
      if(current_packet != NULL ||
        (current_link->link_options & LINK_OPTION_TX && !(current_link->link_options & LINK_OPTION_RX))) {
        regular_slot_is_tx = 1;
        regular_slot_is_rx = 0;
      } else if(current_packet == NULL && current_link->link_options & LINK_OPTION_RX) {
        regular_slot_is_rx = 1;
        regular_slot_is_tx = 0;
      }
#endif

      is_active_slot = current_packet != NULL || (current_link->link_options & LINK_OPTION_RX);
      if(is_active_slot) {

#if WITH_OST
        uint16_t rx_id = 0;

        if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
          && current_link->slotframe_handle <= SSQ_SCHEDULE_HANDLE_OFFSET) {
          if(current_link->link_options & LINK_OPTION_TX) {
            rx_id = ost_get_id_from_tx_sf_handle(current_link->slotframe_handle);
          } else if(current_link->link_options & LINK_OPTION_RX) {
            rx_id = OST_NODE_ID_FROM_LINKADDR(&linkaddr_node_addr);
          }

          struct tsch_slotframe *sf = tsch_schedule_get_slotframe_by_handle(current_link->slotframe_handle);
          if(sf != NULL) {
            uint64_t ASN = (uint64_t)(tsch_current_asn.ls4b) + ((uint64_t)(tsch_current_asn.ms1b) << 32);
            uint64_t ASFN = ASN / sf->size.val;
            uint16_t hash_input = (uint16_t)(rx_id + ASFN);
            uint16_t minus_c_offset = ost_hash_ftn(hash_input, 2) ; /* 0 or 1 */
            current_link->channel_offset = 3; /* default: 3 */
            current_link->channel_offset = current_link->channel_offset - minus_c_offset; /* 3 - 0 or 3 - 1 */
          } else {
//              goto ost_donothing;
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
            rx_id = OST_NODE_ID_FROM_LINKADDR(&linkaddr_node_addr);
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
          uint16_t minus_c_offset = ost_hash_ftn(hash_input, 2) ; // 0 or 1
          current_link->channel_offset = 3; // default
          current_link->channel_offset = current_link->channel_offset - minus_c_offset; // 3-0 or 3-1

#if WITH_OST_LOG_DBG
          TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "ost odp: op");
          );
#endif

        }
#endif
#endif

        /* If we are in a burst, we stick to current channel instead of
        * doing channel hopping, as per IEEE 802.15.4-2015 */
        tsch_current_timeslot = current_link->timeslot; // hckim
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
        if(burst_link_scheduled) {
          /* Reset burst_link_scheduled flag. Will be set again if burst continue. */
          burst_link_scheduled = 0;
#if TSCH_DBT_HOLD_CURRENT_NBR
          burst_link_tx = 0;
          burst_link_rx = 0;
#endif
          is_burst_slot = 1;
        } else 
#endif
        {
          /* Hop channel */
          tsch_current_channel_offset = tsch_get_channel_offset(current_link, current_packet);
          tsch_current_channel = tsch_calculate_channel(&tsch_current_asn, tsch_current_channel_offset);
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
          is_burst_slot = 0;
#endif
        }

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegC0: before RADIO.set_channel() */
        regular_slot_timestamp_common[0] = RTIMER_NOW();
#endif

        NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, tsch_current_channel);

#if HCK_DBG_REGULAR_SLOT_TIMING /* RegC1: after RADIO.set_channel() */
        regular_slot_timestamp_common[1] = RTIMER_NOW();
#endif

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
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
        if(is_burst_slot) {
          is_burst_slot = 0;
        }
#endif
      } else {
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
        /* Make sure to end the burst in cast, for some reason, we were
        * in a burst but now without any more packet to send. */
        burst_link_scheduled = 0;
#if TSCH_DBT_HOLD_CURRENT_NBR
        burst_link_tx = 0;
        burst_link_rx = 0;
#endif
#endif
      }

#if 0//WITH_OST
ost_donothing:
#endif

      TSCH_DEBUG_SLOT_END();
    }



#if WITH_UPA
    if(upa_link_scheduled) {
      if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
        if(upa_pkts_to_send > 0) { /* Tx upa cell */
          ++tsch_scheduled_common_sf_upa_tx_cell_count;
          tsch_common_sf_upa_tx_reserved_count += upa_pkts_to_send;
        } else { /* Rx upa cell */
          ++tsch_scheduled_common_sf_upa_rx_cell_count;
          tsch_common_sf_upa_rx_reserved_count += upa_pkts_to_receive;
        }
      } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
        if(upa_pkts_to_send > 0) { /* Tx upa cell */
          ++tsch_scheduled_unicast_sf_upa_tx_cell_count;
          tsch_unicast_sf_upa_tx_reserved_count += upa_pkts_to_send;
        } else { /* Rx upa cell */
          ++tsch_scheduled_unicast_sf_upa_rx_cell_count;
          tsch_unicast_sf_upa_rx_reserved_count += upa_pkts_to_receive;
        }
      }
#if WITH_OST
      else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
            && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
        if(current_link->link_options & LINK_OPTION_TX) {
          ++tsch_scheduled_ost_pp_sf_upa_tx_cell_count;
          tsch_ost_pp_sf_upa_tx_reserved_count += upa_pkts_to_send;
        } else if(current_link->link_options & LINK_OPTION_RX) {
          ++tsch_scheduled_ost_pp_sf_upa_rx_cell_count;
          tsch_ost_pp_sf_upa_rx_reserved_count += upa_pkts_to_receive;
        }
      }
#endif
    }

    if(upa_link_scheduled) {
      upa_link_scheduled = 0;
      upa_link_finished = 1;

      if(upa_pkts_to_send > 0) {
        if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
          upa_link_result_case = 1;
        } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
          upa_link_result_case = 3;
        }
#if WITH_OST
        else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
              && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
          upa_link_result_case = 5;
        }
#endif
      } else {
        if(current_link->slotframe_handle == TSCH_SCHED_COMMON_SF_HANDLE) {
          upa_link_result_case = 2;
        } else if(current_link->slotframe_handle == TSCH_SCHED_UNICAST_SF_HANDLE) {
          upa_link_result_case = 4;
        }
#if WITH_OST
        else if(current_link->slotframe_handle > OST_PERIODIC_SF_ID_OFFSET 
              && current_link->slotframe_handle <= OST_ONDEMAND_SF_ID_OFFSET) {
          upa_link_result_case = 6;
        }
#endif
      }
    }
#endif

#if WITH_ASAP
    asap_curr_slot_start = current_slot_start;
    asap_curr_slot_end = RTIMER_NOW();

    if(RTIMER_CLOCK_DIFF(asap_curr_slot_end, asap_curr_slot_start) == 0) {
      asap_curr_passed_timeslots = 1;
    } else {
      asap_curr_passed_timeslots = ((RTIMER_CLOCK_DIFF(asap_curr_slot_end, asap_curr_slot_start) + tsch_timing[tsch_ts_timeslot_length] - 1) 
                              / tsch_timing[tsch_ts_timeslot_length]);
    }
    asap_curr_passed_timeslots_except_first_slot = asap_curr_passed_timeslots - 1;

    if(asap_curr_passed_timeslots_except_first_slot > 0
#if WITH_UPA
      && !upa_link_finished
#endif
      ) {

      TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "!overflowed ts %u %u %d %d", 
                          RTIMER_CLOCK_DIFF(asap_curr_slot_end, asap_curr_slot_start), 
                          tsch_timing[tsch_ts_timeslot_length], 
                          asap_curr_passed_timeslots,
                          asap_curr_passed_timeslots_except_first_slot);
      );

    }

    TSCH_ASN_INC(tsch_current_asn, asap_curr_passed_timeslots_except_first_slot);
#endif

#if HCK_DBG_REGULAR_SLOT_DETAIL
    if(regular_slot_is_tx == 1) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg t_r %u %u %u %u %u", 
          regular_slot_tx_packet_len, 
          regular_slot_tx_do_wait_for_ack, 
          regular_slot_tx_mac_tx_status, 
          regular_slot_tx_ack_len,
          current_slot_start));

      regular_slot_tx_packet_len = 0;
      regular_slot_tx_do_wait_for_ack = 0;
      regular_slot_tx_mac_tx_status = 0;
      regular_slot_tx_ack_len = 0;

#if HCK_DBG_REGULAR_SLOT_TIMING /* Print regular tx slot timing */
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg t_c %u %u",
          regular_slot_timestamp_common[0] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_common[0], current_slot_start) : 0,
          regular_slot_timestamp_common[1] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_common[1], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg t_1 %u %u %u %u",
          regular_slot_timestamp_tx[0] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[0], current_slot_start) : 0,
          regular_slot_timestamp_tx[1] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[1], current_slot_start) : 0,
          regular_slot_timestamp_tx[2] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[2], current_slot_start) : 0,
          regular_slot_timestamp_tx[3] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[3], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg t_2 %u %u %u %u",
          regular_slot_timestamp_tx[4] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[4], current_slot_start) : 0,
          regular_slot_timestamp_tx[5] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[5], current_slot_start) : 0,
          regular_slot_timestamp_tx[6] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[6], current_slot_start) : 0,
          regular_slot_timestamp_tx[7] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[7], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg t_3 %u %u %u %u",
          regular_slot_timestamp_tx[8] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[8], current_slot_start) : 0,
          regular_slot_timestamp_tx[9] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[9], current_slot_start) : 0,
          regular_slot_timestamp_tx[10] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[10], current_slot_start) : 0,
          regular_slot_timestamp_tx[11] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[11], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg t_4 %u %u %u %u",
          regular_slot_timestamp_tx[12] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[12], current_slot_start) : 0,
          regular_slot_timestamp_tx[13] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[13], current_slot_start) : 0,
          regular_slot_timestamp_tx[14] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[14], current_slot_start) : 0,
          regular_slot_timestamp_tx[15] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_tx[15], current_slot_start) : 0));

      uint8_t j = 0;
      for(j = 0; j < 2; j++) {
        regular_slot_timestamp_common[j] = 0;
      }
      for(j = 0; j < 16; j++) {
        regular_slot_timestamp_tx[j] = 0;
      }
#endif
    } else if(regular_slot_is_rx == 1) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg r_r %u %u %u %u %d", 
          regular_slot_rx_packet_len, 
          regular_slot_rx_ack_required, 
          regular_slot_rx_ack_len,
          current_slot_start,
          regular_slot_rx_estimated_drift));

      regular_slot_rx_packet_len = 0;
      regular_slot_rx_ack_required = 0;
      regular_slot_rx_ack_len = 0;
      regular_slot_rx_estimated_drift = 0;

#if HCK_DBG_REGULAR_SLOT_TIMING /* Print regular rx slot timing */
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg r_c %u %u",
          regular_slot_timestamp_common[0] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_common[0], current_slot_start) : 0,
          regular_slot_timestamp_common[1] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_common[1], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg r_1 %u %u %u %u",
          regular_slot_timestamp_rx[0] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[0], current_slot_start) : 0,
          regular_slot_timestamp_rx[1] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[1], current_slot_start) : 0,
          regular_slot_timestamp_rx[2] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[2], current_slot_start) : 0,
          regular_slot_timestamp_rx[3] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[3], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg r_2 %u %u %u %u",
          regular_slot_timestamp_rx[5] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[4], current_slot_start) : 0,
          regular_slot_timestamp_rx[6] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[5], current_slot_start) : 0,
          regular_slot_timestamp_rx[7] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[6], current_slot_start) : 0,
          regular_slot_timestamp_rx[8] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[7], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg r_3 %u %u %u %u",
          regular_slot_timestamp_rx[10] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[8], current_slot_start) : 0,
          regular_slot_timestamp_rx[11] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[9], current_slot_start) : 0,
          regular_slot_timestamp_rx[12] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[10], current_slot_start) : 0,
          regular_slot_timestamp_rx[13] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[11], current_slot_start) : 0));
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "reg r_4 %u %u",
          regular_slot_timestamp_rx[12] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[12], current_slot_start) : 0,
          regular_slot_timestamp_rx[13] != 0 ?
            (unsigned)RTIMER_CLOCK_DIFF(regular_slot_timestamp_rx[13], current_slot_start) : 0));
      uint8_t j = 0;
      for(j = 0; j < 2; j++) {
        regular_slot_timestamp_common[j] = 0;
      }
      for(j = 0; j < 14; j++) {
        regular_slot_timestamp_rx[j] = 0;
      }
#endif
    }
    regular_slot_is_tx = 0;
    regular_slot_is_rx = 0;
#endif /* HCK_DBG_REGULAR_SLOT_DETAIL */

#if UPA_TRIPLE_CCA && UPA_DBG_TIMING_TRIPLE_CCA
    if(is_upa_triple_cca) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "t_c %u %u %u %u %u %u ",
          upa_timestamp_triple_cca[0] != 0 ? 
            (unsigned)RTIMER_CLOCK_DIFF(upa_timestamp_triple_cca[0], current_slot_start) : 0,
          upa_timestamp_triple_cca[1] != 0 ? 
            (unsigned)RTIMER_CLOCK_DIFF(upa_timestamp_triple_cca[1], current_slot_start) : 0,
          upa_timestamp_triple_cca[2] != 0 ? 
            (unsigned)RTIMER_CLOCK_DIFF(upa_timestamp_triple_cca[2], current_slot_start) : 0,
          upa_timestamp_triple_cca[3] != 0 ? 
            (unsigned)RTIMER_CLOCK_DIFF(upa_timestamp_triple_cca[3], current_slot_start) : 0,
          upa_timestamp_triple_cca[4] != 0 ? 
            (unsigned)RTIMER_CLOCK_DIFF(upa_timestamp_triple_cca[4], current_slot_start) : 0,
          upa_timestamp_triple_cca[5] != 0 ? 
            (unsigned)RTIMER_CLOCK_DIFF(upa_timestamp_triple_cca[5], current_slot_start) : 0));

      uint8_t k = 0;
      for(k = 0; k < 6; k++) {
        upa_timestamp_triple_cca[k] = 0;
      }
    }
    is_upa_triple_cca = 0;
#endif

#if WITH_UPA && UPA_DBG_SLOT_TIMING /* Print UPA tx slot timing */
    if(upa_print_tx_slot_timing == 1) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa t_r %u %u %u %u a_s %u", 
          upa_tx_slot_trig_packet_len,
          upa_tx_slot_all_packet_len_same,
          upa_pkts_to_send,
          upa_tx_slot_batch_tx_ok_count,
          upa_tx_slot_b_ack_seen));

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa t_h %u %u %u", 
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_trig_ack_start_time, current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_trig_ack_start_time + upa_tx_slot_trig_ack_duration, current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_batch_tx_start_time, current_slot_start)));

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa t_b %u %u", 
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_begin[0], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_begin[1], current_slot_start)));

      uint8_t l = 0;
      for(l = 0; l < upa_pkts_to_send; l++) {
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "upa t_%u %u %u %u %u", l + 1,
            (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_tx[l][0], current_slot_start),
            (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_tx[l][1], current_slot_start),
            (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_tx[l][2], current_slot_start),
            (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_tx[l][3], current_slot_start)));
      }

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa t_a %u %u %u %u",
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_ack[0], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_ack[1], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_ack[2], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_ack[3], current_slot_start)));    

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa t_e %u %u %u", 
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_end[0], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_end[1], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_tx_slot_timestamp_end[2], current_slot_start)));

      for(l = 0; l < 2; l++) {
        upa_tx_slot_timestamp_begin[l] = 0;
      }
      for(l = 0; l < TSCH_DEQUEUED_ARRAY_SIZE; l++) {
        upa_tx_slot_timestamp_tx[l][0] = 0;
        upa_tx_slot_timestamp_tx[l][1] = 0;
        upa_tx_slot_timestamp_tx[l][2] = 0;
        upa_tx_slot_timestamp_tx[l][3] = 0;
      }
      for(l = 0; l < 4; l++) {
        upa_tx_slot_timestamp_ack[l] = 0;
      }
      for(l = 0; l < 3; l++) {
        upa_tx_slot_timestamp_end[l] = 0;
      }
    } else if(upa_print_rx_slot_timing == 1) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa r_r %u %u %u %u a_l %u", 
          upa_rx_slot_trig_packet_len, upa_rx_slot_all_packet_len_same, upa_pkts_to_receive, upa_rx_slot_batch_rx_ok_count, upa_rx_slot_b_ack_len));

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa r_h %u %u %u", 
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_trig_ack_start_time, current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_trig_ack_start_time + upa_rx_slot_trig_ack_duration, current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_batch_rx_start_time, current_slot_start)));

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa r_b %u %u %u", 
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_begin[0], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_begin[1], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_begin[2], current_slot_start)));

      uint8_t l = 0;
      for(l = 0; l < upa_rx_slot_batch_rx_ok_count; l++) {
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
            "upa r_%u %u %u %u %u", l + 1,
            (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_rx[l][0], current_slot_start),
            (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_rx[l][1], current_slot_start),
            (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_rx[l][2], current_slot_start),
            (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_rx[l][3], current_slot_start)));
      }

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa r_a %u %u %u %u",
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_ack[0], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_ack[1], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_ack[2], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_ack[3], current_slot_start)));

      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "upa r_e %u %u", 
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_end[0], current_slot_start),
          (unsigned)RTIMER_CLOCK_DIFF(upa_rx_slot_timestamp_end[1], current_slot_start)));

      for(l = 0; l < 3; l++) {
        upa_rx_slot_timestamp_begin[l] = 0;
      }
      for(l = 0; l < TSCH_MAX_INCOMING_PACKETS; l++) {
        upa_rx_slot_timestamp_rx[l][0] = 0;
        upa_rx_slot_timestamp_rx[l][1] = 0;
        upa_rx_slot_timestamp_rx[l][2] = 0;
        upa_rx_slot_timestamp_rx[l][3] = 0;
      }
      for(l = 0; l < 4; l++) {
        upa_rx_slot_timestamp_ack[l] = 0;
      }
      for(l = 0; l < 2; l++) {
        upa_rx_slot_timestamp_end[l] = 0;
      }
    }
    upa_print_tx_slot_timing = 0;
    upa_print_rx_slot_timing = 0;
#endif

#if WITH_ASAP && ASAP_DBG_SLOT_END
    if(upa_link_finished) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "asap s_e %u %u %u %d", 
          asap_curr_slot_start,
          (unsigned)RTIMER_CLOCK_DIFF(asap_curr_slot_end, asap_curr_slot_start),
          asap_curr_passed_timeslots,
          asap_curr_passed_timeslots * tsch_timing[tsch_ts_timeslot_length] 
            - RTIMER_CLOCK_DIFF(asap_curr_slot_end, asap_curr_slot_start)));
    }
#endif

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

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && TSCH_DBT_HANDLE_MISSED_DBT_SLOT
      burst_link_scheduled_count = 0;
#endif
      /* Schedule next wakeup skipping slots if missed deadline */
      do {
        if(current_link != NULL
            && current_link->link_options & LINK_OPTION_TX
            && current_link->link_options & LINK_OPTION_SHARED) {
          /* Decrement the backoff window for all neighbors able to transmit over
          * this Tx, Shared link. */
          tsch_queue_update_all_backoff_windows(&current_link->addr);
        }

#if WITH_UPA
        if(upa_schedule_after_upa_link) { /* Failed scheduling after UPA */
          upa_schedule_after_upa_link = 0;
          ++asap_curr_passed_timeslots;
          ++asap_curr_passed_timeslots_except_first_slot;

          if(upa_pkts_to_send > 0) {
            if(upa_link_result_case == 1) {
              tsch_common_sf_upa_tx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
              TSCH_LOG_ADD(upa_log_result,
                  log->upa_result.upa_is_overflowed = 1;
                  log->upa_result.upa_link_type = upa_link_result_case;
                  log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_send;
                  log->upa_result.upa_num_of_successful_pkts = upa_tx_slot_batch_tx_ok_count;
                  log->upa_result.upa_trig_pkt_len = upa_tx_slot_trig_packet_len;
                  log->upa_result.upa_all_pkt_len_same = upa_tx_slot_all_packet_len_same;
                  log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
                  log->upa_result.upa_tot_pkt_len = asap_tot_packet_len;
                  log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
                  log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
                  log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
                  log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
                  log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
                  log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
                  log->upa_result.upa_num_of_expected_slots = upa_expected_passed_timeslots_except_first_slot + 1;
              );
#endif
            } else if(upa_link_result_case == 3) {
              tsch_unicast_sf_upa_tx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
              TSCH_LOG_ADD(upa_log_result,
                  log->upa_result.upa_is_overflowed = 1;
                  log->upa_result.upa_link_type = upa_link_result_case;
                  log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_send;
                  log->upa_result.upa_num_of_successful_pkts = upa_tx_slot_batch_tx_ok_count;
                  log->upa_result.upa_trig_pkt_len = upa_tx_slot_trig_packet_len;
                  log->upa_result.upa_all_pkt_len_same = upa_tx_slot_all_packet_len_same;
                  log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
                  log->upa_result.upa_tot_pkt_len = asap_tot_packet_len;
                  log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
                  log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
                  log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
                  log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
                  log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
                  log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
                  log->upa_result.upa_num_of_expected_slots = upa_expected_passed_timeslots_except_first_slot + 1;
              );
#endif
            }
#if WITH_OST
            else if(upa_link_result_case == 5) {
              tsch_ost_pp_sf_upa_tx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
              TSCH_LOG_ADD(upa_log_result,
                  log->upa_result.upa_is_overflowed = 1;
                  log->upa_result.upa_link_type = upa_link_result_case;
                  log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_send;
                  log->upa_result.upa_num_of_successful_pkts = upa_tx_slot_batch_tx_ok_count;
                  log->upa_result.upa_trig_pkt_len = upa_tx_slot_trig_packet_len;
                  log->upa_result.upa_all_pkt_len_same = upa_tx_slot_all_packet_len_same;
                  log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
                  log->upa_result.upa_tot_pkt_len = asap_tot_packet_len;
                  log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
                  log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
                  log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
                  log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
                  log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
                  log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
                  log->upa_result.upa_num_of_expected_slots = upa_expected_passed_timeslots_except_first_slot + 1;
              );
#endif
            }
#endif
            upa_tx_slot_trig_packet_len = 0;
            upa_tx_slot_all_packet_len_same = 0;
          } else {
            if(upa_link_result_case == 2) {
              tsch_common_sf_upa_rx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
              TSCH_LOG_ADD(upa_log_result,
                  log->upa_result.upa_is_overflowed = 1;
                  log->upa_result.upa_link_type = upa_link_result_case;
                  log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_receive;
                  log->upa_result.upa_num_of_successful_pkts = upa_rx_slot_batch_rx_ok_count;
                  log->upa_result.upa_trig_pkt_len = upa_rx_slot_trig_packet_len;
                  log->upa_result.upa_all_pkt_len_same = upa_rx_slot_all_packet_len_same;
                  log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
                  log->upa_result.upa_tot_pkt_len = 0;
                  log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
                  log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
                  log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
                  log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
                  log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
                  log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
                  log->upa_result.upa_num_of_expected_slots = 0;
              );
#endif
            }
            else if(upa_link_result_case == 4) {
              tsch_unicast_sf_upa_rx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
              TSCH_LOG_ADD(upa_log_result,
                  log->upa_result.upa_is_overflowed = 1;
                  log->upa_result.upa_link_type = upa_link_result_case;
                  log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_receive;
                  log->upa_result.upa_num_of_successful_pkts = upa_rx_slot_batch_rx_ok_count;
                  log->upa_result.upa_trig_pkt_len = upa_rx_slot_trig_packet_len;
                  log->upa_result.upa_all_pkt_len_same = upa_rx_slot_all_packet_len_same;
                  log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
                  log->upa_result.upa_tot_pkt_len = 0;
                  log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
                  log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
                  log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
                  log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
                  log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
                  log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
                  log->upa_result.upa_num_of_expected_slots = 0;
              );
#endif
            }
#if WITH_OST
            else if(upa_link_result_case == 6) {
              tsch_ost_pp_sf_upa_rx_timeslots += asap_curr_passed_timeslots_except_first_slot;
#if UPA_DBG_ESSENTIAL
              TSCH_LOG_ADD(upa_log_result,
                  log->upa_result.upa_is_overflowed = 1;
                  log->upa_result.upa_link_type = upa_link_result_case;
                  log->upa_result.upa_num_of_reserved_pkts = upa_pkts_to_receive;
                  log->upa_result.upa_num_of_successful_pkts = upa_rx_slot_batch_rx_ok_count;
                  log->upa_result.upa_trig_pkt_len = upa_rx_slot_trig_packet_len;
                  log->upa_result.upa_all_pkt_len_same = upa_rx_slot_all_packet_len_same;
                  log->upa_result.upa_tot_ack_len = asap_tot_ack_len;
                  log->upa_result.upa_tot_pkt_len = 0;
                  log->upa_result.upa_successful_pkt_len = asap_ok_packet_len;
                  log->upa_result.upa_unused_offset_time = (unsigned)RTIMERTICKS_TO_US(current_slot_unused_offset_time);
                  log->upa_result.upa_idle_time = (unsigned)RTIMERTICKS_TO_US(current_slot_idle_time);
                  log->upa_result.upa_curr_slot_length = tsch_timing_us[tsch_ts_timeslot_length];
                  log->upa_result.upa_num_of_slots_until_ilde_time = current_slot_passed_slots;
                  log->upa_result.upa_num_of_slots_until_scheduling = asap_curr_passed_timeslots;
                  log->upa_result.upa_num_of_expected_slots = 0;
              );
#endif
            }
#endif
            upa_rx_slot_trig_packet_len = 0;
            upa_rx_slot_all_packet_len_same = 0;
          }

          upa_pkts_to_send = 0;
          upa_pkts_to_receive = 0;
          asap_curr_passed_timeslots = 0;
          asap_curr_passed_timeslots_except_first_slot = 0;
        }
#endif /* WITH_UPA */

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && TSCH_DBT_HANDLE_MISSED_DBT_SLOT
        if(burst_link_scheduled_count > 0) { /* This means deadline of DBT link has been missed */
          burst_link_scheduled = 0;
          burst_link_scheduled_count = 0;
#if TSCH_DBT_HOLD_CURRENT_NBR
          burst_link_tx = 0;
          burst_link_rx = 0;
#endif

          TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "!missed dbt");
          );
        }
#endif


#if !WITH_TSCH_DEFAULT_BURST_TRANSMISSION
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
#else
          /* A burst link was scheduled. Replay the current link at the
          next time offset */
          if(burst_link_scheduled && current_link != NULL) {
#if TSCH_DBT_HANDLE_MISSED_DBT_SLOT
            ++burst_link_scheduled_count;
#endif
            timeslot_diff = 1;
            backup_link = NULL;

#if TSCH_DBT_TEMPORARY_LINK
            temporal_burst_link.next = NULL;
            temporal_burst_link.handle = current_link->handle;
            linkaddr_copy(&(temporal_burst_link.addr), &(current_link->addr));
            temporal_burst_link.slotframe_handle = current_link->slotframe_handle;
            temporal_burst_link.timeslot = (current_link->timeslot + 1) 
                                  % tsch_schedule_get_slotframe_by_handle(temporal_burst_link.slotframe_handle)->size.val;
            temporal_burst_link.channel_offset = current_link->channel_offset;
            temporal_burst_link.link_options = current_link->link_options;
            temporal_burst_link.link_type = current_link->link_type;
            temporal_burst_link.data = current_link->data;

            current_link = &temporal_burst_link;
#endif

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
#endif

#if UPA_DBG_OPERATION
        if(asap_curr_passed_timeslots_except_first_slot > 0) {
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "after upa: ts to next %u", timeslot_diff));
        }
#endif


        /* Update ASN */
        TSCH_ASN_INC(tsch_current_asn, timeslot_diff);

#if WITH_ALICE
        TSCH_ASN_COPY(alice_current_asn, tsch_current_asn);
#endif

#if WITH_OST && OST_ON_DEMAND_PROVISION
        if(current_link == NULL && tsch_is_locked() 
          && ost_exist_matching_slot(&tsch_current_asn)) { /* tsch-schedule is being changing, so locked */
          ost_remove_matching_slot();
        }
#endif        

#if WITH_ASAP
        asap_timeslot_diff_at_the_end = timeslot_diff;
        if(asap_curr_passed_timeslots_except_first_slot > 0) {
          timeslot_diff += asap_curr_passed_timeslots_except_first_slot;
        }
#endif

        /* Time to next wake up */
        time_to_next_active_slot = timeslot_diff * tsch_timing[tsch_ts_timeslot_length] + drift_correction;

#if WITH_ASAP && ASAP_DBG_SLOT_END
        if(upa_link_finished) {
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "asap s_n %u %u %u %d", 
              asap_timeslot_diff_at_the_end,
              timeslot_diff,
              time_to_next_active_slot,
              time_to_next_active_slot
                - RTIMER_CLOCK_DIFF(asap_curr_slot_end, asap_curr_slot_start)));
        }
#endif

#if WITH_UPA
        if(upa_link_finished) {
          upa_link_finished = 0;
          upa_schedule_after_upa_link = 1;
        } else {
          time_to_next_active_slot += tsch_timesync_adaptive_compensate(time_to_next_active_slot);
        }
#else
        time_to_next_active_slot += tsch_timesync_adaptive_compensate(time_to_next_active_slot);
#endif
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

#if WITH_ALICE
    TSCH_ASN_COPY(alice_current_asn, tsch_current_asn);
#endif

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

#if WITH_ALICE
  alice_current_asn = *next_slot_asn;
#endif

  status = critical_enter();
  last_sync_asn = tsch_current_asn;
  tsch_last_sync_time = clock_time();
  critical_exit(status);
  current_link = NULL;
}
/*---------------------------------------------------------------------------*/
/** @} */
