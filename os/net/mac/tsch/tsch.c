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
 *         IEEE 802.15.4 TSCH MAC implementation.
 *         Does not use any RDC layer. Should be used with nordc.
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 *         Beshr Al Nahas <beshr@sics.se>
 *
 */

/**
 * \addtogroup tsch
 * @{
*/

#include "contiki.h"
#include "dev/radio.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/nbr-table.h"
#include "net/link-stats.h"
#include "net/mac/framer/framer-802154.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/mac-sequence.h"
#include "lib/random.h"
#include "net/routing/routing.h"

#if TSCH_WITH_SIXTOP
#include "net/mac/tsch/sixtop/sixtop.h"
#endif

#if WITH_DRA
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/routing/rpl-classic/rpl-private.h"
#endif

#if FRAME802154_VERSION < FRAME802154_IEEE802154_2015
#error TSCH: FRAME802154_VERSION must be at least FRAME802154_IEEE802154_2015
#endif

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "TSCH"
#define LOG_LEVEL LOG_LEVEL_MAC

#if HCK_FORMATION_BOOTSTRAP_STATE_INFO
uint8_t hck_fitst_transition_to_tsch_joined = 0;
uint8_t hck_fitst_transition_to_rpl_joined = 0;
uint8_t hck_fitst_transition_to_joined_node = 0;

uint64_t hck_first_asn_tsch_joined = 0;
uint64_t hck_first_asn_rpl_joined = 0;
uint64_t hck_first_asn_joined_node = 0;

uint8_t hck_formation_bootstrap_state = HCK_BOOTSTRAP_STATE_0_NEW_NODE;
uint64_t hck_formation_state_transition_asn = 0;
#endif

/*---------------------------------------------------------------------------*/
#if WITH_DRA
uint8_t DRA_MAX_M = 0; /* Set as the maximum number of slots that can be 
                          allocated in the slotframe at intervals of 2 slots 
                          in powers of 2 */
uint8_t DRA_T_SLOTFRAMES = 0; /* Set as the minimum number of slotframes 
                                 that ensures a period of at least one second. */

/* All sequence numbers begins from 1 */
uint16_t dra_my_eb_seq = 1;
uint16_t dra_my_bc_seq = 1;
uint16_t dra_my_uc_seq = 1;

static uint16_t dra_my_num_of_pkts = 0;
static uint16_t dra_my_adapt_count = 1; /* Begins from 1 */

uint8_t dra_my_m = 0;
uint8_t dra_total_max_m = 0;

struct tsch_asn_t dra_current_asn;
uint64_t dra_lastly_scheduled_asfn = 0;

struct dra_nbr_info {
  uint8_t dra_nbr_id;
  uint8_t dra_nbr_last_m;
  uint16_t dra_nbr_last_eb_seq;
  uint16_t dra_nbr_last_bc_seq;
  uint16_t dra_nbr_last_uc_seq;
  uint16_t dra_nbr_last_eb_adapt_count;
  uint16_t dra_nbr_last_bc_adapt_count;
  uint16_t dra_nbr_last_uc_adapt_count;
  uint16_t dra_nbr_num_of_pkts;
};
static struct dra_nbr_info dra_nbr_info_table[DRA_NBR_NUM];
/*---------------------------------------------------------------------------*/
int dra_receive_control_message(int rx_dra_nbr_id, uint8_t rx_dra_m, 
                                uint16_t rx_dra_seq, uint8_t rx_dra_eb_1_bc_2_uc_3)
{
  /* First, get matched/available index */
  int nbr_index = -1;
  int i = 0;
  for(i = 0; i < DRA_NBR_NUM; i++) {
    if(dra_nbr_info_table[i].dra_nbr_id == 0 
      || dra_nbr_info_table[i].dra_nbr_id == rx_dra_nbr_id) {
      nbr_index = i;
      dra_nbr_info_table[nbr_index].dra_nbr_id = rx_dra_nbr_id;
      break;
    }
  }

  /* Second, update corresponding info */
  if(nbr_index != -1) {
    uint16_t dra_nbr_stored_seq = 0;
    uint16_t dra_nbr_stored_adapt_count = 0;
    uint16_t dra_nbr_adapt_count_diff = 0;
    uint16_t dra_nbr_accum_pkts_tot = 0;
    uint16_t dra_nbr_accum_pkts = 0;

    if(rx_dra_eb_1_bc_2_uc_3 == 1) {
      dra_nbr_stored_seq = dra_nbr_info_table[nbr_index].dra_nbr_last_eb_seq;
      dra_nbr_stored_adapt_count = dra_nbr_info_table[nbr_index].dra_nbr_last_eb_adapt_count;
      dra_nbr_adapt_count_diff = 0;

      if(dra_nbr_stored_seq == 0 || dra_nbr_stored_adapt_count == 0) { /* First time received */
        dra_nbr_accum_pkts = 1;
      } else if(rx_dra_seq < dra_nbr_stored_seq) {
        dra_nbr_accum_pkts = 1;
      } else {
        dra_nbr_accum_pkts_tot = rx_dra_seq - dra_nbr_stored_seq;
        if(dra_my_adapt_count > dra_nbr_stored_adapt_count) {
          dra_nbr_adapt_count_diff = dra_my_adapt_count - dra_nbr_stored_adapt_count;
          dra_nbr_accum_pkts = dra_nbr_accum_pkts_tot / dra_nbr_adapt_count_diff;
        } else if(dra_my_adapt_count == dra_nbr_stored_adapt_count) {
          dra_nbr_accum_pkts = dra_nbr_accum_pkts_tot;
        } else {
          dra_nbr_accum_pkts = 1;
        }
      }
      dra_nbr_info_table[nbr_index].dra_nbr_last_eb_seq = rx_dra_seq;
      if(dra_nbr_stored_seq < rx_dra_seq) {
        dra_nbr_info_table[nbr_index].dra_nbr_last_eb_adapt_count = dra_my_adapt_count;
      }

    } else if(rx_dra_eb_1_bc_2_uc_3 == 2) {
      dra_nbr_stored_seq = dra_nbr_info_table[nbr_index].dra_nbr_last_bc_seq;
      dra_nbr_stored_adapt_count = dra_nbr_info_table[nbr_index].dra_nbr_last_bc_adapt_count;
      dra_nbr_adapt_count_diff = 0;

      if(dra_nbr_stored_seq == 0 || dra_nbr_stored_adapt_count == 0) { /* First time received */
        dra_nbr_accum_pkts = 1;
      } else if(rx_dra_seq < dra_nbr_stored_seq) {
        dra_nbr_accum_pkts = 1;
      } else {
        dra_nbr_accum_pkts_tot = rx_dra_seq - dra_nbr_stored_seq;
        if(dra_my_adapt_count > dra_nbr_stored_adapt_count) {
          dra_nbr_adapt_count_diff = dra_my_adapt_count - dra_nbr_stored_adapt_count;
          dra_nbr_accum_pkts = dra_nbr_accum_pkts_tot / dra_nbr_adapt_count_diff;
        } else if(dra_my_adapt_count == dra_nbr_stored_adapt_count) {
          dra_nbr_accum_pkts = dra_nbr_accum_pkts_tot;
        } else {
          dra_nbr_accum_pkts = 1;
        }
      }
      dra_nbr_info_table[nbr_index].dra_nbr_last_bc_seq = rx_dra_seq;
      if(dra_nbr_stored_seq < rx_dra_seq) {
        dra_nbr_info_table[nbr_index].dra_nbr_last_bc_adapt_count = dra_my_adapt_count;
      }

    } else { /* UC packets other than EB and BC packets */
      dra_nbr_stored_seq = dra_nbr_info_table[nbr_index].dra_nbr_last_uc_seq;
      dra_nbr_stored_adapt_count = dra_nbr_info_table[nbr_index].dra_nbr_last_uc_adapt_count;
      dra_nbr_adapt_count_diff = 0;

      if(dra_nbr_stored_seq == 0 || dra_nbr_stored_adapt_count == 0) { /* First time received */
        dra_nbr_accum_pkts = 1;
      } else if(rx_dra_seq < dra_nbr_stored_seq) {
        dra_nbr_accum_pkts = 1;
      } else {
        dra_nbr_accum_pkts_tot = rx_dra_seq - dra_nbr_stored_seq;
        if(dra_my_adapt_count > dra_nbr_stored_adapt_count) {
          dra_nbr_adapt_count_diff = dra_my_adapt_count - dra_nbr_stored_adapt_count;
          dra_nbr_accum_pkts = dra_nbr_accum_pkts_tot / dra_nbr_adapt_count_diff;
        } else if(dra_my_adapt_count == dra_nbr_stored_adapt_count) {
          dra_nbr_accum_pkts = dra_nbr_accum_pkts_tot;
        } else {
          dra_nbr_accum_pkts = 1;
        }
      }
      dra_nbr_info_table[nbr_index].dra_nbr_last_uc_seq = rx_dra_seq;
      if(dra_nbr_stored_seq < rx_dra_seq) {
        dra_nbr_info_table[nbr_index].dra_nbr_last_uc_adapt_count = dra_my_adapt_count;
      }
    }
    dra_nbr_info_table[nbr_index].dra_nbr_num_of_pkts += dra_nbr_accum_pkts;
    dra_nbr_info_table[nbr_index].dra_nbr_last_m = rx_dra_m;

#if DRA_DBG
    if(rx_dra_eb_1_bc_2_uc_3 == 1) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "dra nbr eb %u %u %u %u %u %u %u %u", dra_nbr_info_table[nbr_index].dra_nbr_id,
                                                dra_nbr_stored_seq,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_eb_seq,
                                                dra_nbr_stored_adapt_count,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_eb_adapt_count,
                                                dra_nbr_accum_pkts,
                                                dra_nbr_info_table[nbr_index].dra_nbr_num_of_pkts,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_m));
    } else if(rx_dra_eb_1_bc_2_uc_3 == 2) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "dra nbr bc %u %u %u %u %u %u %u %u", dra_nbr_info_table[nbr_index].dra_nbr_id,
                                                dra_nbr_stored_seq,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_bc_seq,
                                                dra_nbr_stored_adapt_count,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_bc_adapt_count,
                                                dra_nbr_accum_pkts,
                                                dra_nbr_info_table[nbr_index].dra_nbr_num_of_pkts,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_m));
    } else {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
          "dra nbr uc %u %u %u %u %u %u %u %u", dra_nbr_info_table[nbr_index].dra_nbr_id,
                                                dra_nbr_stored_seq,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_uc_seq,
                                                dra_nbr_stored_adapt_count,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_uc_adapt_count,
                                                dra_nbr_accum_pkts,
                                                dra_nbr_info_table[nbr_index].dra_nbr_num_of_pkts,
                                                dra_nbr_info_table[nbr_index].dra_nbr_last_m));
    }
#endif

    return 1;

  } else {
#if DRA_DBG
    TSCH_LOG_ADD(tsch_log_message,
        snprintf(log->message, sizeof(log->message),
        "dra nbr N/A"));
#endif

    return -1;
  }
}
/*---------------------------------------------------------------------------*/
void dra_calculate_shared_slots()
{
  /* First, derive dra_my_m */
  uint16_t dra_total_num_of_pkts = 0;
  uint16_t dra_avg_num_of_pkts = 0; // DRA-TODO: should be double?

  dra_total_num_of_pkts += dra_my_num_of_pkts;

  int i = 0;
  for(i = 0; i < DRA_NBR_NUM; i++) {
    dra_total_num_of_pkts += dra_nbr_info_table[i].dra_nbr_num_of_pkts;
  }

  dra_avg_num_of_pkts = dra_total_num_of_pkts / DRA_T_SLOTFRAMES;

  int dra_exponent = 0;
  dra_my_m = 0;
  while((1 << dra_exponent) < dra_avg_num_of_pkts) {
    dra_exponent++;
  }
  dra_my_m = dra_exponent;
  dra_my_m = MIN(dra_my_m, DRA_MAX_M);


  /* Second, derive dra_total_max_m */
  dra_total_max_m = 0;
  dra_total_max_m = dra_my_m;

  for(i = 0; i < DRA_NBR_NUM; i++) {
    if(dra_total_max_m < dra_nbr_info_table[i].dra_nbr_last_m) {
      dra_total_max_m = dra_nbr_info_table[i].dra_nbr_last_m;
    }
  }


#if DRA_DBG
    TSCH_LOG_ADD(tsch_log_message,
        snprintf(log->message, sizeof(log->message),
        "dra calc %u %u %u %u %u", dra_my_m,
                                   dra_total_max_m,
                                   dra_my_num_of_pkts,
                                   dra_total_num_of_pkts,
                                   dra_avg_num_of_pkts));
#endif


  /* Last, reset dra_nbr_info_table */
  dra_my_num_of_pkts = 0;

  for(i = 0; i < DRA_NBR_NUM; i++) {
    dra_nbr_info_table[i].dra_nbr_last_m = 0;
    dra_nbr_info_table[i].dra_nbr_num_of_pkts = 0;
  }

  dra_my_adapt_count++;

#if DRA_DBG
  TSCH_LOG_ADD(tsch_log_message,
      snprintf(log->message, sizeof(log->message),
      "dra adapt %u", dra_my_adapt_count));
#endif

}
#endif /* WITH_DRA */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#if WITH_TRGB
uint8_t trgb_parent_id = 0;
uint8_t trgb_grandP_id = 0;
enum TRGB_CELL trgb_my_tx_cell = TRGB_CELL_NULL;
#endif
/*---------------------------------------------------------------------------*/

/* hckim measure associated cell counts */
static uint32_t tsch_timeslots_until_last_session;
static int32_t tsch_timeslots_in_current_session; // timeslots in current session
static uint32_t tsch_total_associated_timeslots; // timeslots until last session + timeslots in current session
static struct tsch_asn_t tsch_last_asn_associated;
static uint16_t tsch_log_association_count;

/* hckim measure leaving events */
static uint16_t tsch_leaving_count;
static clock_time_t clock_last_leaving;
static clock_time_t clock_inst_leaving_time;
static clock_time_t clock_avg_leaving_time;

/* hckim measure eb packet */
static uint16_t tsch_eb_packet_qloss_count;
static uint16_t tsch_eb_packet_enqueue_count;
static uint16_t tsch_eb_packet_ok_count;
static uint16_t tsch_eb_packet_noack_count;
static uint16_t tsch_eb_packet_error_count;

/* hckim measure ka packet */
static uint16_t tsch_ka_packet_send_count;
static uint16_t tsch_ka_packet_qloss_count;
static uint16_t tsch_ka_packet_enqueue_count;
static uint32_t tsch_ka_packet_transmission_count;
static uint16_t tsch_ka_packet_ok_count;
static uint16_t tsch_ka_packet_noack_count;
static uint16_t tsch_ka_packet_error_count;

/* hckim measure ip packet */
static uint16_t tsch_ip_packet_qloss_count;
static uint16_t tsch_ip_packet_enqueue_count;
static uint16_t tsch_ip_packet_ok_count;
static uint16_t tsch_ip_packet_noack_count;
static uint16_t tsch_ip_packet_error_count;

/* hckim measure ip_icmp6 packet */
static uint16_t tsch_ip_icmp6_packet_qloss_count;
static uint16_t tsch_ip_icmp6_packet_enqueue_count;
static uint16_t tsch_ip_icmp6_packet_ok_count;
static uint16_t tsch_ip_icmp6_packet_noack_count;
static uint16_t tsch_ip_icmp6_packet_error_count;

/* hckim measure ip_udp packet */
static uint16_t tsch_ip_udp_packet_qloss_count;
static uint16_t tsch_ip_udp_packet_enqueue_count;
static uint16_t tsch_ip_udp_packet_ok_count;
static uint16_t tsch_ip_udp_packet_noack_count;
static uint16_t tsch_ip_udp_packet_error_count;

/* hckim measure rinbuf */
uint16_t tsch_input_ringbuf_full_count;
uint16_t tsch_input_ringbuf_available_count;
uint16_t tsch_dequeued_ringbuf_full_count;
uint16_t tsch_dequeued_ringbuf_available_count;

/* hckim measure cell utilization during association */
uint32_t tsch_scheduled_eb_sf_cell_count;
uint32_t tsch_scheduled_common_sf_cell_count;
uint32_t tsch_scheduled_unicast_sf_cell_count;
#if WITH_OST
uint32_t tsch_scheduled_ost_pp_sf_tx_cell_count;
uint32_t tsch_scheduled_ost_pp_sf_rx_cell_count;
uint32_t tsch_scheduled_ost_odp_sf_tx_cell_count;
uint32_t tsch_scheduled_ost_odp_sf_rx_cell_count;
#endif
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
uint32_t tsch_scheduled_common_sf_bst_tx_cell_count;
uint32_t tsch_scheduled_common_sf_bst_rx_cell_count;
uint32_t tsch_scheduled_unicast_sf_bst_tx_cell_count;
uint32_t tsch_scheduled_unicast_sf_bst_rx_cell_count;
#if WITH_OST
uint32_t tsch_scheduled_ost_pp_sf_bst_tx_cell_count;
uint32_t tsch_scheduled_ost_pp_sf_bst_rx_cell_count;
#endif
#endif

/* hckim measure tx/rx operation counts */
uint32_t tsch_eb_sf_tx_operation_count;
uint32_t tsch_eb_sf_rx_operation_count;
uint32_t tsch_common_sf_tx_operation_count;
uint32_t tsch_common_sf_rx_operation_count;
uint32_t tsch_unicast_sf_tx_operation_count;
uint32_t tsch_unicast_sf_rx_operation_count;
#if WITH_OST
uint32_t tsch_ost_pp_sf_tx_operation_count;
uint32_t tsch_ost_pp_sf_rx_operation_count;
uint32_t tsch_ost_odp_sf_tx_operation_count;
uint32_t tsch_ost_odp_sf_rx_operation_count;
#endif
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
uint32_t tsch_common_sf_bst_tx_operation_count;
uint32_t tsch_common_sf_bst_rx_operation_count;
uint32_t tsch_unicast_sf_bst_tx_operation_count;
uint32_t tsch_unicast_sf_bst_rx_operation_count;
#if WITH_OST
uint32_t tsch_ost_pp_sf_bst_tx_operation_count;
uint32_t tsch_ost_pp_sf_bst_rx_operation_count;
#endif
#endif

#if WITH_ALICE && ALICE_EARLY_PACKET_DROP
/* EB never experiences early_packet_drop
   early_packet_drop only targets unicast slotframes (ex. RB) */
uint16_t alice_early_packet_drop_count;
#endif

/*---------------------------------------------------------------------------*/
void print_log_tsch()
{
#if WITH_ALICE && ALICE_EARLY_PACKET_DROP
  LOG_HCK("e_drop %u |\n", alice_early_packet_drop_count);
#endif

  LOG_HCK("input_full %u input_avail %u dequeued_full %u dequeued_avail %u |\n", 
          tsch_input_ringbuf_full_count, 
          tsch_input_ringbuf_available_count, 
          tsch_dequeued_ringbuf_full_count, 
          tsch_dequeued_ringbuf_available_count);

  //timeslots in current session
  tsch_timeslots_in_current_session = TSCH_ASN_DIFF(tsch_current_asn, tsch_last_asn_associated);
  //timeslots until last session + timeslots in current session
  tsch_total_associated_timeslots = 
          tsch_timeslots_until_last_session + tsch_timeslots_in_current_session;

#if WITH_OST
  LOG_HCK("asso_ts %lu sch_eb %lu sch_bc %lu sch_uc %lu sch_pp_tx %lu sch_pp_rx %lu sch_odp_tx %lu sch_odp_rx %lu |\n", 
          tsch_total_associated_timeslots, 
          tsch_scheduled_eb_sf_cell_count, 
          tsch_scheduled_common_sf_cell_count, 
          tsch_scheduled_unicast_sf_cell_count,
          tsch_scheduled_ost_pp_sf_tx_cell_count, 
          tsch_scheduled_ost_pp_sf_rx_cell_count, 
          tsch_scheduled_ost_odp_sf_tx_cell_count, 
          tsch_scheduled_ost_odp_sf_rx_cell_count);
#else
#if TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
  LOG_HCK("asso_ts %lu sch_cs %lu |\n", 
          tsch_total_associated_timeslots, 
          tsch_scheduled_common_sf_cell_count);
#else
  LOG_HCK("asso_ts %lu sch_eb %lu sch_bc %lu sch_uc %lu |\n", 
          tsch_total_associated_timeslots, 
          tsch_scheduled_eb_sf_cell_count, 
          tsch_scheduled_common_sf_cell_count, 
          tsch_scheduled_unicast_sf_cell_count);
#endif
#endif

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
#if WITH_OST
  LOG_HCK("sch_bc_bst_tx %lu sch_bc_bst_rx %lu sch_uc_bst_tx %lu sch_uc_bst_rx %lu sch_pp_bst_tx %lu sch_pp_bst_rx %lu |\n", 
          tsch_scheduled_common_sf_bst_tx_cell_count,
          tsch_scheduled_common_sf_bst_rx_cell_count,
          tsch_scheduled_unicast_sf_bst_tx_cell_count,
          tsch_scheduled_unicast_sf_bst_rx_cell_count,
          tsch_scheduled_ost_pp_sf_bst_tx_cell_count,
          tsch_scheduled_ost_pp_sf_bst_rx_cell_count);
#else
  LOG_HCK("sch_bc_bst_tx %lu sch_bc_bst_rx %lu sch_uc_bst_tx %lu sch_uc_bst_rx %lu |\n", 
          tsch_scheduled_common_sf_bst_tx_cell_count,
          tsch_scheduled_common_sf_bst_rx_cell_count,
          tsch_scheduled_unicast_sf_bst_tx_cell_count,
          tsch_scheduled_unicast_sf_bst_rx_cell_count);
#endif
#endif

#if WITH_OST
  LOG_HCK("eb_tx_op %lu eb_rx_op %lu bc_tx_op %lu bc_rx_op %lu uc_tx_op %lu uc_rx_op %lu pp_tx_op %lu pp_rx_op %lu odp_tx_op %lu odp_rx_op %lu |\n", 
          tsch_eb_sf_tx_operation_count, 
          tsch_eb_sf_rx_operation_count, 
          tsch_common_sf_tx_operation_count, 
          tsch_common_sf_rx_operation_count, 
          tsch_unicast_sf_tx_operation_count, 
          tsch_unicast_sf_rx_operation_count,
          tsch_ost_pp_sf_tx_operation_count, 
          tsch_ost_pp_sf_rx_operation_count, 
          tsch_ost_odp_sf_tx_operation_count, 
          tsch_ost_odp_sf_rx_operation_count);
#else
#if TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
  LOG_HCK("cs_tx_op %lu cs_rx_op %lu |\n", 
          tsch_common_sf_tx_operation_count, 
          tsch_common_sf_rx_operation_count);
#else
  LOG_HCK("eb_tx_op %lu eb_rx_op %lu bc_tx_op %lu bc_rx_op %lu uc_tx_op %lu uc_rx_op %lu |\n", 
          tsch_eb_sf_tx_operation_count, 
          tsch_eb_sf_rx_operation_count, 
          tsch_common_sf_tx_operation_count, 
          tsch_common_sf_rx_operation_count, 
          tsch_unicast_sf_tx_operation_count, 
          tsch_unicast_sf_rx_operation_count);
#endif
#endif

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
#if WITH_OST
  LOG_HCK("bc_bst_tx_op %lu bc_bst_rx_op %lu uc_bst_tx_op %lu uc_bst_rx_op %lu pp_bst_tx_op %lu pp_bst_rx_op %lu |\n", 
          tsch_common_sf_bst_tx_operation_count,
          tsch_common_sf_bst_rx_operation_count,
          tsch_unicast_sf_bst_tx_operation_count,
          tsch_unicast_sf_bst_rx_operation_count,
          tsch_ost_pp_sf_bst_tx_operation_count,
          tsch_ost_pp_sf_bst_rx_operation_count);
#else
  LOG_HCK("bc_bst_tx_op %lu bc_bst_rx_op %lu uc_bst_tx_op %lu uc_bst_rx_op %lu |\n", 
          tsch_common_sf_bst_tx_operation_count,
          tsch_common_sf_bst_rx_operation_count,
          tsch_unicast_sf_bst_tx_operation_count,
          tsch_unicast_sf_bst_rx_operation_count);
#endif
#endif
}
/*---------------------------------------------------------------------------*/
void reset_log_tsch()
{
  print_log_tsch();
  
  tsch_timeslots_until_last_session = 0;
  TSCH_ASN_COPY(tsch_last_asn_associated, tsch_current_asn);

  tsch_log_association_count = 1; /* assume that this node is associated when bootstrap period ends */
  LOG_HCK("asso %u |\n", tsch_log_association_count);

  tsch_leaving_count = 0;
  /* do not initialize clock_last_leaving and clock_inst_leaving_time */
  clock_avg_leaving_time = 0;

  /* EB */
  tsch_eb_packet_qloss_count = 0;
  tsch_eb_packet_enqueue_count = 0;
  tsch_eb_packet_ok_count = 0;
  tsch_eb_packet_noack_count = 0;
  tsch_eb_packet_error_count = 0;

  /* keepalive */
  tsch_ka_packet_send_count = 0;
  tsch_ka_packet_qloss_count = 0;
  tsch_ka_packet_enqueue_count = 0;
  tsch_ka_packet_transmission_count = 0;
  tsch_ka_packet_ok_count = 0;
  tsch_ka_packet_noack_count = 0;
  tsch_ka_packet_error_count = 0;

  tsch_ip_packet_qloss_count = 0;
  tsch_ip_packet_enqueue_count = 0;
  tsch_ip_packet_ok_count = 0;
  tsch_ip_packet_noack_count = 0;
  tsch_ip_packet_error_count = 0;

  tsch_ip_icmp6_packet_qloss_count = 0;
  tsch_ip_icmp6_packet_enqueue_count = 0;
  tsch_ip_icmp6_packet_ok_count = 0;
  tsch_ip_icmp6_packet_noack_count = 0;
  tsch_ip_icmp6_packet_error_count = 0;

  tsch_ip_udp_packet_qloss_count = 0;
  tsch_ip_udp_packet_enqueue_count = 0;
  tsch_ip_udp_packet_ok_count = 0;
  tsch_ip_udp_packet_noack_count = 0;
  tsch_ip_udp_packet_error_count = 0;

  tsch_input_ringbuf_full_count = 0;
  tsch_input_ringbuf_available_count = 0;
  tsch_dequeued_ringbuf_full_count = 0;
  tsch_dequeued_ringbuf_available_count = 0;

  /* hckim for measure cell utilization during association */
  tsch_scheduled_eb_sf_cell_count = 0;
  tsch_scheduled_common_sf_cell_count = 0;
  tsch_scheduled_unicast_sf_cell_count = 0;
#if WITH_OST
  tsch_scheduled_ost_pp_sf_tx_cell_count = 0;
  tsch_scheduled_ost_pp_sf_rx_cell_count = 0;
  tsch_scheduled_ost_odp_sf_tx_cell_count = 0;
  tsch_scheduled_ost_odp_sf_rx_cell_count = 0;
#endif
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
  tsch_scheduled_common_sf_bst_tx_cell_count = 0;
  tsch_scheduled_common_sf_bst_rx_cell_count = 0;
  tsch_scheduled_unicast_sf_bst_tx_cell_count = 0;
  tsch_scheduled_unicast_sf_bst_rx_cell_count = 0;
#if WITH_OST
  tsch_scheduled_ost_pp_sf_bst_tx_cell_count = 0;
  tsch_scheduled_ost_pp_sf_bst_rx_cell_count = 0;
#endif
#endif

/* hckim measure tx/rx operation counts */
  tsch_eb_sf_tx_operation_count = 0;
  tsch_eb_sf_rx_operation_count = 0;
  tsch_common_sf_tx_operation_count = 0;
  tsch_common_sf_rx_operation_count = 0;
  tsch_unicast_sf_tx_operation_count = 0;
  tsch_unicast_sf_rx_operation_count = 0;
#if WITH_OST
  tsch_ost_pp_sf_tx_operation_count = 0;
  tsch_ost_pp_sf_rx_operation_count = 0;
  tsch_ost_odp_sf_tx_operation_count = 0;
  tsch_ost_odp_sf_rx_operation_count = 0;
#endif
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
  tsch_common_sf_bst_tx_operation_count = 0;
  tsch_common_sf_bst_rx_operation_count = 0;
  tsch_unicast_sf_bst_tx_operation_count = 0;
  tsch_unicast_sf_bst_rx_operation_count = 0;
#if WITH_OST
  tsch_ost_pp_sf_bst_tx_operation_count = 0;
  tsch_ost_pp_sf_bst_rx_operation_count = 0;
#endif
#endif

#if WITH_ALICE && ALICE_EARLY_PACKET_DROP
  alice_early_packet_drop_count = 0;
#endif
}
/*---------------------------------------------------------------------------*/

/* The address of the last node we received an EB from (other than our time source).
 * Used for recovery */
static linkaddr_t last_eb_nbr_addr;
/* The join priority advertised by last_eb_nbr_addr */
static uint8_t last_eb_nbr_jp;

/* Let TSCH select a time source with no help of an upper layer.
 * We do so using statistics from incoming EBs */
#if TSCH_AUTOSELECT_TIME_SOURCE
int best_neighbor_eb_count;
struct eb_stat {
  int rx_count;
  int jp;
};
NBR_TABLE(struct eb_stat, eb_stats);
#endif /* TSCH_AUTOSELECT_TIME_SOURCE */

/* TSCH channel hopping sequence */
uint8_t tsch_hopping_sequence[TSCH_HOPPING_SEQUENCE_MAX_LEN];
struct tsch_asn_divisor_t tsch_hopping_sequence_length;

/* Default TSCH timeslot timing (in micro-second) */
static const uint16_t *tsch_default_timing_us;
/* TSCH timeslot timing (in micro-second) */
uint16_t tsch_timing_us[tsch_ts_elements_count];
/* TSCH timeslot timing (in rtimer ticks) */
rtimer_clock_t tsch_timing[tsch_ts_elements_count];

#if LINKADDR_SIZE == 8
/* 802.15.4 broadcast MAC address  */
const linkaddr_t tsch_broadcast_address = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };
/* Address used for the EB virtual neighbor queue */
const linkaddr_t tsch_eb_address = { { 0, 0, 0, 0, 0, 0, 0, 0 } };
#else /* LINKADDR_SIZE == 8 */
const linkaddr_t tsch_broadcast_address = { { 0xff, 0xff } };
const linkaddr_t tsch_eb_address = { { 0, 0 } };
#endif /* LINKADDR_SIZE == 8 */

/* Is TSCH started? */
int tsch_is_started = 0;
/* Has TSCH initialization failed? */
int tsch_is_initialized = 0;
/* Are we coordinator of the TSCH network? */
int tsch_is_coordinator = 0;
/* Are we associated to a TSCH network? */
int tsch_is_associated = 0;
/* Total number of associations since boot */
int tsch_association_count = 0;
/* Is the PAN running link-layer security? */
int tsch_is_pan_secured = LLSEC802154_ENABLED;
/* The current Absolute Slot Number (ASN) */
struct tsch_asn_t tsch_current_asn;
/* Device rank or join priority:
 * For PAN coordinator: 0 -- lower is better */
uint8_t tsch_join_priority;
/* The current TSCH sequence number, used for unicast data frames only */
static uint8_t tsch_packet_seqno;
/* Current period for EB output */
static clock_time_t tsch_current_eb_period;
/* Current period for keepalive output */
static clock_time_t tsch_current_ka_timeout;

/* For scheduling keepalive messages  */
enum tsch_keepalive_status {
  KEEPALIVE_SCHEDULING_UNCHANGED,
  KEEPALIVE_SCHEDULE_OR_STOP,
  KEEPALIVE_SEND_IMMEDIATELY,
};
/* Should we send or schedule a keepalive? */
static volatile enum tsch_keepalive_status keepalive_status;

/* timer for sending keepalive messages */
static struct ctimer keepalive_timer;

/* Statistics on the current session */
unsigned long tx_count;
unsigned long rx_count;
unsigned long sync_count;
int32_t min_drift_seen;
int32_t max_drift_seen;

/* TSCH processes and protothreads */
PT_THREAD(tsch_scan(struct pt *pt));
PROCESS(tsch_process, "main process");
PROCESS(tsch_send_eb_process, "send EB process");
PROCESS(tsch_pending_events_process, "pending events process");

/* Other function prototypes */
static void packet_input(void);

/*---------------------------------------------------------------------------*/
#if WITH_ALICE
#ifdef ALICE_TIME_VARYING_SCHEDULING
/* ALICE: ASN at the start of the ongoing slot operation. */
struct tsch_asn_t alice_current_asn;
/* ALICE: ASFN of the lastly scheduled unicast slotframe. */
uint64_t alice_lastly_scheduled_asfn = 0;
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
uint64_t alice_next_asfn_of_lastly_scheduled_asfn = 0;
#endif
/* ALICE: return the current ASFN for ALICE. */
uint16_t
alice_tsch_schedule_get_current_asfn(struct tsch_slotframe *sf)
{
  uint16_t mod = TSCH_ASN_MOD(tsch_current_asn, sf->size);

  struct tsch_asn_t new_asn;
  TSCH_ASN_COPY(new_asn, tsch_current_asn);
  TSCH_ASN_DEC(new_asn, mod);
  
  return TSCH_ASN_DIVISION(new_asn, sf->size);
}
#endif
#endif
/*---------------------------------------------------------------------------*/

/* Getters and setters */

/*---------------------------------------------------------------------------*/
void
tsch_set_coordinator(int enable)
{
  if(tsch_is_coordinator != enable) {
    tsch_is_associated = 0;
  }
  tsch_is_coordinator = enable;
  tsch_set_eb_period(TSCH_EB_PERIOD);

#if HCK_FORMATION_BOOTSTRAP_STATE_INFO
  if(tsch_is_coordinator) { /* Coordinator */
    if(hck_fitst_transition_to_joined_node == 0) {
      hck_fitst_transition_to_joined_node = 1;
      uint8_t prev_bootstrap_state = hck_formation_bootstrap_state;
      uint64_t prev_state_transition_asn = hck_formation_state_transition_asn;
      hck_formation_bootstrap_state = HCK_BOOTSTRAP_STATE_3_JOINED_NODE;
      hck_formation_state_transition_asn = tsch_calculate_current_asn();
      LOG_HCK_FORMATION("rpku %u bs %u at %llx pbs %u pat %llx\n",
                        1, 
                        hck_formation_bootstrap_state, 
                        hck_formation_state_transition_asn,
                        prev_bootstrap_state,
                        prev_state_transition_asn);

      hck_first_asn_joined_node = hck_formation_state_transition_asn;
      LOG_HCK_FORMATION("FTST %u at %llx\n", 
                        hck_formation_bootstrap_state, 
                        hck_formation_state_transition_asn);
/*
      LOG_HCK_FORMATION("FASN %u TJ %llx RJ %llx JN %llx\n", 
                        hck_formation_bootstrap_state, 
                        hck_first_asn_tsch_joined,
                        hck_first_asn_rpl_joined,
                        hck_first_asn_joined_node);
*/
    }
  }
#endif

#if WITH_TRGB
  int trgb_rand_num = random_rand();
  int trgb_rand_mod = trgb_rand_num % 2;

  if(tsch_is_coordinator) { /* Coordinator */
    if(trgb_my_tx_cell == TRGB_CELL_NULL) {
      /* Configure trgb_my_tx_cell, if it is not configured */
      if(trgb_rand_mod == 0) {
        trgb_my_tx_cell = TRGB_CELL_GREEN;
      } else {
        trgb_my_tx_cell = TRGB_CELL_BLUE;
      }
    } else {
      /* Maintain previously configured trgb_my_tx_cell */
    }
  } else { /* Not coordinator - reset trgb_my_tx_cell */
    trgb_my_tx_cell = TRGB_CELL_NULL;
  }

#if TRGB_DBG
  LOG_HCK_TRGB("TRGB tx_cell_init tsch_set_coord %u %d %d %u\n", 
                tsch_is_coordinator, trgb_rand_num, trgb_rand_mod, trgb_my_tx_cell);
#endif
#endif
}
/*---------------------------------------------------------------------------*/
void
tsch_set_pan_secured(int enable)
{
  tsch_is_pan_secured = LLSEC802154_ENABLED && enable;
}
/*---------------------------------------------------------------------------*/
void
tsch_set_join_priority(uint8_t jp)
{
  tsch_join_priority = jp;
}
/*---------------------------------------------------------------------------*/
void
tsch_set_ka_timeout(uint32_t timeout)
{
  tsch_current_ka_timeout = timeout;
  tsch_schedule_keepalive(0);
}
/*---------------------------------------------------------------------------*/
void
tsch_set_eb_period(uint32_t period)
{
  tsch_current_eb_period = MIN(period, TSCH_MAX_EB_PERIOD);
}
/*---------------------------------------------------------------------------*/
static void
tsch_reset(void)
{
  int i;
  frame802154_set_pan_id(0xffff);
  /* First make sure pending packet callbacks are sent etc */
  process_post_synch(&tsch_pending_events_process, PROCESS_EVENT_POLL, NULL);
  /* Reset neighbor queues */
  tsch_queue_reset();
  /* Remove unused neighbors */
  tsch_queue_free_unused_neighbors();
  tsch_queue_update_time_source(NULL);
  /* Initialize global variables */
  tsch_join_priority = 0xff;
  TSCH_ASN_INIT(tsch_current_asn, 0, 0);
  current_link = NULL;
  /* Reset timeslot timing to defaults */
  tsch_default_timing_us = TSCH_DEFAULT_TIMESLOT_TIMING;
  for(i = 0; i < tsch_ts_elements_count; i++) {
    tsch_timing_us[i] = tsch_default_timing_us[i];
    tsch_timing[i] = US_TO_RTIMERTICKS(tsch_timing_us[i]);
  }
#ifdef TSCH_CALLBACK_LEAVING_NETWORK
  TSCH_CALLBACK_LEAVING_NETWORK();
#endif
  linkaddr_copy(&last_eb_nbr_addr, &linkaddr_null);
#if TSCH_AUTOSELECT_TIME_SOURCE
  struct nbr_sync_stat *stat;
  best_neighbor_eb_count = 0;
  /* Remove all nbr stats */
  stat = nbr_table_head(sync_stats);
  while(stat != NULL) {
    nbr_table_remove(sync_stats, stat);
    stat = nbr_table_next(sync_stats, stat);
  }
#endif /* TSCH_AUTOSELECT_TIME_SOURCE */
  tsch_set_eb_period(TSCH_EB_PERIOD);
  keepalive_status = KEEPALIVE_SCHEDULING_UNCHANGED;

#if WITH_DRA
  /* First, set DRA maximum m */
  int dra_power_of_two = 1;
  int dra_exponent = 0;

  while(dra_power_of_two * 2 <= (DRA_SLOTFRAME_LENGTH / DRA_MINIMUM_INTER_SLOT_INTERVAL)) {
    dra_power_of_two *= 2;
    dra_exponent++;
  }

  DRA_MAX_M = dra_exponent;

  /* Second, set as the minimum number of slotframes 
     that ensures a period of at least one second. */
  DRA_T_SLOTFRAMES = 1000 / (DRA_SLOTFRAME_LENGTH * 10) + 1;

#if DRA_LOG
  LOG_HCK_DRA("dra param %u %u %u %u\n", DRA_SLOTFRAME_LENGTH, 
                                         DRA_MAX_M, 
                                         DRA_MINIMUM_INTER_SLOT_INTERVAL, 
                                         DRA_T_SLOTFRAMES);
#endif

  /* Third, initialize DRA neighbor information */
  int j = 0;
  for(j = 0; j < DRA_NBR_NUM; j++) {
    dra_nbr_info_table[j].dra_nbr_id = 0;
    dra_nbr_info_table[j].dra_nbr_last_m = 0;
    dra_nbr_info_table[j].dra_nbr_last_eb_seq = 0;
    dra_nbr_info_table[j].dra_nbr_last_bc_seq = 0;
    dra_nbr_info_table[j].dra_nbr_last_uc_seq = 0;
    dra_nbr_info_table[j].dra_nbr_last_eb_adapt_count = 0;
    dra_nbr_info_table[j].dra_nbr_last_bc_adapt_count = 0;
    dra_nbr_info_table[j].dra_nbr_last_uc_adapt_count = 0;
    dra_nbr_info_table[j].dra_nbr_num_of_pkts = 0;
  }

    dra_my_adapt_count = 1; /* Reset, begins from 1 */
#endif

#if WITH_TRGB
  int trgb_rand_num = random_rand();
  int trgb_rand_mod = trgb_rand_num % 2;
  
  if(tsch_is_coordinator) { /* Coordinator */
    if(trgb_my_tx_cell == TRGB_CELL_NULL) {
      /* Configure trgb_my_tx_cell, if it is not configured */
      if(trgb_rand_mod == 0) {
        trgb_my_tx_cell = TRGB_CELL_GREEN;
      } else {
        trgb_my_tx_cell = TRGB_CELL_BLUE;
      }
    } else {
      /* Maintain previously configured trgb_my_tx_cell */
    }
  } else { /* Not coordinator - reset trgb_my_tx_cell */
    trgb_my_tx_cell = TRGB_CELL_NULL;
  }

#if TRGB_DBG
  LOG_HCK_TRGB("TRGB tx_cell_init tsch_reset %u %d %d %u\n", 
                tsch_is_coordinator, trgb_rand_num, trgb_rand_mod, trgb_my_tx_cell);
#endif
#endif
}
/* TSCH keep-alive functions */

/*---------------------------------------------------------------------------*/
/* Resynchronize to last_eb_nbr.
 * Return non-zero if this function schedules the next keepalive.
 * Return zero otherwise.
 */
static int
resynchronize(const linkaddr_t *original_time_source_addr)
{
  const struct tsch_neighbor *current_time_source = tsch_queue_get_time_source();
  const linkaddr_t *ts_addr = tsch_queue_get_nbr_address(current_time_source);
  if(ts_addr != NULL && !linkaddr_cmp(ts_addr, original_time_source_addr)) {
    /* Time source has already been changed (e.g. by RPL). Let's see if it works. */
    LOG_INFO("time source has been changed to ");
    LOG_INFO_LLADDR(ts_addr);
    LOG_INFO_("\n");
    return 0;
  }
  /* Switch time source to the last neighbor we received an EB from */
  if(linkaddr_cmp(&last_eb_nbr_addr, &linkaddr_null)) {
    LOG_WARN("not able to re-synchronize, received no EB from other neighbors\n");
    if(sync_count == 0) {
      /* We got no synchronization at all in this session, leave the network */
      tsch_disassociate();
    }
    return 0;
  } else {
    LOG_WARN("re-synchronizing on ");
    LOG_WARN_LLADDR(&last_eb_nbr_addr);
    LOG_WARN_("\n");
    /* We simply pick the last neighbor we receiver sync information from */
    tsch_queue_update_time_source(&last_eb_nbr_addr);
    tsch_join_priority = last_eb_nbr_jp + 1;
    /* Try to get in sync ASAP */
    tsch_schedule_keepalive(1);
    return 1;
  }
}

/*---------------------------------------------------------------------------*/
/* Tx callback for keepalive messages */
#if HCK_FORMATION_PACKET_TYPE_INFO
void
#else
static void
#endif
keepalive_packet_sent(void *ptr, int status, int transmissions)
{
  int schedule_next_keepalive = 1;
  /* Update neighbor link statistics */
  link_stats_packet_sent(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), status, transmissions);
  /* Call RPL callback if RPL is enabled */
#ifdef TSCH_CALLBACK_KA_SENT
  TSCH_CALLBACK_KA_SENT(status, transmissions);
#endif /* TSCH_CALLBACK_KA_SENT */
  if(status == MAC_TX_NOACK || status == MAC_TX_OK) {
    tsch_ka_packet_transmission_count += transmissions;
  }
  LOG_INFO("KA sent to ");
  LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  LOG_INFO_(", st %d-%d\n", status, transmissions);

  LOG_HCK("ka_tx %lu |\n", tsch_ka_packet_transmission_count);

  /* We got no ack, try to resynchronize */
  if(status == MAC_TX_NOACK) {
    schedule_next_keepalive = !resynchronize(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  }

  if(schedule_next_keepalive) {
    tsch_schedule_keepalive(0);
  }
}
/*---------------------------------------------------------------------------*/
/* Prepare and send a keepalive message */
static void
keepalive_send(void *ptr)
{
  /* If not here from a timer callback, the timer must be stopped */
  ctimer_stop(&keepalive_timer);

  if(tsch_is_associated) {
    struct tsch_neighbor *n = tsch_queue_get_time_source();
    if(n != NULL) {
        linkaddr_t *destination = tsch_queue_get_nbr_address(n);

        LOG_INFO("sending KA to ");
        LOG_INFO_LLADDR(destination);
        LOG_INFO_("\n");

        ++tsch_ka_packet_send_count;
        uint64_t tsch_ka_send_asn = tsch_calculate_current_asn();
        LOG_HCK("ka_send %u | at %llx\n", tsch_ka_packet_send_count, tsch_ka_send_asn);

        /* Simply send an empty packet */
        packetbuf_clear();
        packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, destination);
        NETSTACK_MAC.send(keepalive_packet_sent, NULL);
    } else {
        LOG_ERR("no timesource - KA not sent\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
void
tsch_schedule_keepalive(int immediate)
{
  if(immediate) {
    /* send as soon as possible */
    keepalive_status = KEEPALIVE_SEND_IMMEDIATELY;
  } else if(keepalive_status != KEEPALIVE_SEND_IMMEDIATELY) {
    /* send based on the tsch_current_ka_timeout */
    keepalive_status = KEEPALIVE_SCHEDULE_OR_STOP;
  }
  process_poll(&tsch_pending_events_process);
}
/*---------------------------------------------------------------------------*/
static void
tsch_keepalive_process_pending(void)
{
  if(keepalive_status != KEEPALIVE_SCHEDULING_UNCHANGED) {
    /* first, save and reset the old status */
    enum tsch_keepalive_status scheduled_status = keepalive_status;
    keepalive_status = KEEPALIVE_SCHEDULING_UNCHANGED;

    if(!tsch_is_coordinator && tsch_is_associated) {
      switch(scheduled_status) {
      case KEEPALIVE_SEND_IMMEDIATELY:
        /* always send, and as soon as possible (now) */
        keepalive_send(NULL);
        break;

      case KEEPALIVE_SCHEDULE_OR_STOP:
        if(tsch_current_ka_timeout > 0) {
          /* Pick a delay in the range [tsch_current_ka_timeout*0.9, tsch_current_ka_timeout[ */
          unsigned long delay;
          if(tsch_current_ka_timeout >= 10) {
            delay = (tsch_current_ka_timeout - tsch_current_ka_timeout / 10)
                + random_rand() % (tsch_current_ka_timeout / 10);
          } else {
            delay = tsch_current_ka_timeout - 1;
          }
          ctimer_set(&keepalive_timer, delay, keepalive_send, NULL);
        } else {
          /* zero timeout set, stop sending keepalives */
          ctimer_stop(&keepalive_timer);
        }
        break;

      default:
        break;
      }
    } else {
      /* either coordinator or not associated */
      ctimer_stop(&keepalive_timer);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
eb_input(struct input_packet *current_input)
{
  /* LOG_INFO("EB received\n"); */
  frame802154_t frame;
  /* Verify incoming EB (does its ASN match our Rx time?),
   * and update our join priority. */
  struct ieee802154_ies eb_ies;

  if(tsch_packet_parse_eb(current_input->payload, current_input->len,
                          &frame, &eb_ies, NULL, 1)) {
    /* PAN ID check and authentication done at rx time */

    /* Got an EB from a different neighbor than our time source, keep enough data
     * to switch to it in case we lose the link to our time source */
    struct tsch_neighbor *ts = tsch_queue_get_time_source();
    linkaddr_t *ts_addr = tsch_queue_get_nbr_address(ts);
    if(ts_addr == NULL || !linkaddr_cmp(&last_eb_nbr_addr, ts_addr)) {
      linkaddr_copy(&last_eb_nbr_addr, (linkaddr_t *)&frame.src_addr);
      last_eb_nbr_jp = eb_ies.ie_join_priority;
    }

#if TSCH_AUTOSELECT_TIME_SOURCE
    if(!tsch_is_coordinator) {
      /* Maintain EB received counter for every neighbor */
      struct eb_stat *stat = (struct eb_stat *)nbr_table_get_from_lladdr(eb_stats, (linkaddr_t *)&frame.src_addr);
      if(stat == NULL) {
        stat = (struct eb_stat *)nbr_table_add_lladdr(eb_stats, (linkaddr_t *)&frame.src_addr, NBR_TABLE_REASON_MAC, NULL);
      }
      if(stat != NULL) {
        stat->rx_count++;
        stat->jp = eb_ies.ie_join_priority;
        best_neighbor_eb_count = MAX(best_neighbor_eb_count, stat->rx_count);
      }
      /* Select best time source */
      struct eb_stat *best_stat = NULL;
      stat = nbr_table_head(eb_stats);
      while(stat != NULL) {
        /* Is neighbor eligible as a time source? */
        if(stat->rx_count > best_neighbor_eb_count / 2) {
          if(best_stat == NULL ||
             stat->jp < best_stat->jp) {
            best_stat = stat;
          }
        }
        stat = nbr_table_next(eb_stats, stat);
      }
      /* Update time source */
      if(best_stat != NULL) {
        tsch_queue_update_time_source(nbr_table_get_lladdr(eb_stats, best_stat));
        tsch_join_priority = best_stat->jp + 1;
      }
    }
#endif /* TSCH_AUTOSELECT_TIME_SOURCE */

    /* Did the EB come from our time source? */
    if(ts_addr != NULL && linkaddr_cmp((linkaddr_t *)&frame.src_addr, ts_addr)) {
      /* Check for ASN drift */
      int32_t asn_diff = TSCH_ASN_DIFF(current_input->rx_asn, eb_ies.ie_asn);
      if(asn_diff != 0) {
        /* We disagree with our time source's ASN -- leave the network */
        LOG_WARN("! ASN drifted by %ld, leaving the network\n", asn_diff);
        tsch_disassociate();
      }

      if(eb_ies.ie_join_priority >= TSCH_MAX_JOIN_PRIORITY) {
        /* Join priority unacceptable. Leave network. */
        LOG_WARN("! EB JP too high %u, leaving the network\n",
               eb_ies.ie_join_priority);
        tsch_disassociate();
      } else {
#if TSCH_AUTOSELECT_TIME_SOURCE
        /* Update join priority */
        if(tsch_join_priority != eb_ies.ie_join_priority + 1) {
          LOG_INFO("update JP from EB %u -> %u\n",
                 tsch_join_priority, eb_ies.ie_join_priority + 1);
          tsch_join_priority = eb_ies.ie_join_priority + 1;
        }
#endif /* TSCH_AUTOSELECT_TIME_SOURCE */
      }

      /* TSCH hopping sequence */
      if(eb_ies.ie_channel_hopping_sequence_id != 0) {
        if(eb_ies.ie_hopping_sequence_len != tsch_hopping_sequence_length.val
            || memcmp((uint8_t *)tsch_hopping_sequence, eb_ies.ie_hopping_sequence_list, tsch_hopping_sequence_length.val)) {
          if(eb_ies.ie_hopping_sequence_len <= sizeof(tsch_hopping_sequence)) {
            memcpy((uint8_t *)tsch_hopping_sequence, eb_ies.ie_hopping_sequence_list,
                   eb_ies.ie_hopping_sequence_len);
            TSCH_ASN_DIVISOR_INIT(tsch_hopping_sequence_length, eb_ies.ie_hopping_sequence_len);

            LOG_WARN("Updating TSCH hopping sequence from EB\n");
          } else {
            LOG_WARN("TSCH:! parse_eb: hopping sequence too long (%u)\n", eb_ies.ie_hopping_sequence_len);
          }
        }
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
/* Process pending input packet(s) */
static void
tsch_rx_process_pending()
{
  int16_t input_index;
  /* Loop on accessing (without removing) a pending input packet */
  while((input_index = ringbufindex_peek_get(&input_ringbuf)) != -1) {
    struct input_packet *current_input = &input_array[input_index];

#if WITH_OST /* OST-09: Post process received N */
    ost_post_process_rx_N(current_input);
#endif

    frame802154_t frame;
    uint8_t ret = frame802154_parse(current_input->payload, current_input->len, &frame);
    int is_data = ret && frame.fcf.frame_type == FRAME802154_DATAFRAME;
    int is_eb = ret
      && frame.fcf.frame_version == FRAME802154_IEEE802154_2015
      && frame.fcf.frame_type == FRAME802154_BEACONFRAME;

    if(is_data) {
      /* Skip EBs and other control messages */
      /* Copy to packetbuf for processing */
      packetbuf_copyfrom(current_input->payload, current_input->len);
      packetbuf_set_attr(PACKETBUF_ATTR_RSSI, current_input->rssi);
      packetbuf_set_attr(PACKETBUF_ATTR_CHANNEL, current_input->channel);
    }

    if(is_data) {
      /* Pass to upper layers */
      packet_input();
    } else if(is_eb) {
      eb_input(current_input);
    }

    /* Remove input from ringbuf */
    ringbufindex_get(&input_ringbuf);
  }
}
/*---------------------------------------------------------------------------*/
/* Pass sent packets to upper layer */
static void
tsch_tx_process_pending(void)
{
  int16_t dequeued_index;
  /* Loop on accessing (without removing) a pending input packet */
  while((dequeued_index = ringbufindex_peek_get(&dequeued_ringbuf)) != -1) {
    struct tsch_packet *p = dequeued_array[dequeued_index];

#if WITH_OST
    ost_post_process_rx_t_offset(p);
#endif

    /* Put packet into packetbuf for packet_sent callback */
    queuebuf_to_packetbuf(p->qb);
    LOG_INFO("packet sent to ");
    LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    LOG_INFO_(", seqno %u, status %d, tx %d\n",
      packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO), p->ret, p->transmissions);

    if(p->sent == NULL) { // EB
      if(p->ret == MAC_TX_NOACK) {
        ++tsch_eb_packet_noack_count;
        LOG_HCK("eb_noack %u |\n", tsch_eb_packet_noack_count);
      } else if(p->ret == MAC_TX_OK) {
        ++tsch_eb_packet_ok_count;
        LOG_HCK("eb_ok %u |\n", tsch_eb_packet_ok_count);
      } else if(p->ret == MAC_TX_ERR || p->ret == MAC_TX_ERR_FATAL) {
        ++tsch_eb_packet_error_count;
        LOG_HCK("eb_err %u |\n", tsch_eb_packet_error_count);
      }
    } else if(p->sent == keepalive_packet_sent) { // KA
      if(p->ret == MAC_TX_NOACK) {
        ++tsch_ka_packet_noack_count;
        LOG_HCK("ka_noack %u |\n", tsch_ka_packet_noack_count);
      } else if(p->ret == MAC_TX_OK) {
        ++tsch_ka_packet_ok_count;
        LOG_HCK("ka_ok %u |\n", tsch_ka_packet_ok_count);
      } else if(p->ret == MAC_TX_ERR || p->ret == MAC_TX_ERR_FATAL) {
        ++tsch_ka_packet_error_count;
        LOG_HCK("ka_err %u |\n", tsch_ka_packet_error_count);
      }
    } else {
      if(p->ret == MAC_TX_NOACK) { // IP layer packet
        ++tsch_ip_packet_noack_count;
        if(packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6) {
          ++tsch_ip_icmp6_packet_noack_count;
          LOG_HCK("ip_noack %u ip_icmp6_noack %u |\n", 
                tsch_ip_packet_noack_count, 
                tsch_ip_icmp6_packet_noack_count);
        } else {
          ++tsch_ip_udp_packet_noack_count;
          LOG_HCK("ip_noack %u ip_udp_noack %u |\n", 
                tsch_ip_packet_noack_count, 
                tsch_ip_udp_packet_noack_count);
        }

      } else if(p->ret == MAC_TX_OK) {
        ++tsch_ip_packet_ok_count;
        if(packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6) {
          ++tsch_ip_icmp6_packet_ok_count;
          LOG_HCK("ip_ok %u ip_icmp6_ok %u |\n", 
                tsch_ip_packet_ok_count, 
                tsch_ip_icmp6_packet_ok_count);
        } else {
          ++tsch_ip_udp_packet_ok_count;
          LOG_HCK("ip_ok %u ip_udp_ok %u |\n", 
                tsch_ip_packet_ok_count, 
                tsch_ip_udp_packet_ok_count);
        }
      } else if(p->ret == MAC_TX_ERR || p->ret == MAC_TX_ERR_FATAL) {
        ++tsch_ip_packet_error_count;
        if(packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6) {
          ++tsch_ip_icmp6_packet_error_count;
          LOG_HCK("ip_err %u ip_icmp6_err %u |\n", 
                tsch_ip_packet_error_count, 
                tsch_ip_icmp6_packet_error_count);
        } else {
          ++tsch_ip_udp_packet_error_count;
          LOG_HCK("ip_err %u ip_udp_err %u |\n", 
                tsch_ip_packet_error_count, 
                tsch_ip_udp_packet_error_count);
        }
      }
    }
    /* Call packet_sent callback */
    mac_call_sent_callback(p->sent, p->ptr, p->ret, p->transmissions);
    /* Free packet queuebuf */
    tsch_queue_free_packet(p);
    /* Free all unused neighbors */
    tsch_queue_free_unused_neighbors();
    /* Remove dequeued packet from ringbuf */
    ringbufindex_get(&dequeued_ringbuf);
  }
}
/*---------------------------------------------------------------------------*/
/* Setup TSCH as a coordinator */
static void
tsch_start_coordinator(void)
{
  frame802154_set_pan_id(IEEE802154_PANID);
  /* Initialize hopping sequence as default */
  memcpy(tsch_hopping_sequence, TSCH_DEFAULT_HOPPING_SEQUENCE, sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE));
  TSCH_ASN_DIVISOR_INIT(tsch_hopping_sequence_length, sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE));
#if TSCH_SCHEDULE_WITH_6TISCH_MINIMAL
  tsch_schedule_create_minimal();
#endif

  tsch_is_associated = 1;
  tsch_join_priority = 0;

  LOG_INFO("starting as coordinator, PAN ID %x, asn-%x.%lx\n",
      frame802154_get_pan_id(), tsch_current_asn.ms1b, tsch_current_asn.ls4b);

  /* Start slot operation */
  tsch_slot_operation_sync(RTIMER_NOW(), &tsch_current_asn);
}
/*---------------------------------------------------------------------------*/
/* Leave the TSCH network */
void
tsch_disassociate(void)
{
  if(tsch_is_associated == 1) {
    tsch_is_associated = 0;
    tsch_adaptive_timesync_reset();
    process_poll(&tsch_process);
  }
}
/*---------------------------------------------------------------------------*/
/* Attempt to associate to a network form an incoming EB */
static int
tsch_associate(const struct input_packet *input_eb, rtimer_clock_t timestamp)
{
  frame802154_t frame;
  struct ieee802154_ies ies;
  uint8_t hdrlen;
  int i;

  if(input_eb == NULL || tsch_packet_parse_eb(input_eb->payload, input_eb->len,
                                              &frame, &ies, &hdrlen, 0) == 0) {
    LOG_DBG("! failed to parse packet as EB while scanning (len %u)\n",
        input_eb->len);
    return 0;
  }

  tsch_current_asn = ies.ie_asn;
  tsch_join_priority = ies.ie_join_priority + 1;

#if WITH_DRA
  dra_current_asn = ies.ie_asn;
#endif

#if WITH_ALICE
  alice_current_asn = ies.ie_asn;
#endif

#if TSCH_JOIN_SECURED_ONLY
  if(frame.fcf.security_enabled == 0) {
    LOG_ERR("! parse_eb: EB is not secured\n");
    return 0;
  }
#endif /* TSCH_JOIN_SECURED_ONLY */
#if LLSEC802154_ENABLED
  if(!tsch_security_parse_frame(input_eb->payload, hdrlen,
      input_eb->len - hdrlen - tsch_security_mic_len(&frame),
      &frame, (linkaddr_t*)&frame.src_addr, &tsch_current_asn)) {
    LOG_ERR("! parse_eb: failed to authenticate\n");
    return 0;
  }
#endif /* LLSEC802154_ENABLED */

#if !LLSEC802154_ENABLED
  if(frame.fcf.security_enabled == 1) {
    LOG_ERR("! parse_eb: we do not support security, but EB is secured\n");
    return 0;
  }
#endif /* !LLSEC802154_ENABLED */

#if TSCH_JOIN_MY_PANID_ONLY
  /* Check if the EB comes from the PAN ID we expect */
  if(frame.src_pid != IEEE802154_PANID) {
    LOG_ERR("! parse_eb: PAN ID %x != %x\n", frame.src_pid, IEEE802154_PANID);
    return 0;
  }
#endif /* TSCH_JOIN_MY_PANID_ONLY */

  /* There was no join priority (or 0xff) in the EB, do not join */
  if(ies.ie_join_priority == 0xff) {
    LOG_ERR("! parse_eb: no join priority\n");
    return 0;
  }

  /* TSCH timeslot timing */
  for(i = 0; i < tsch_ts_elements_count; i++) {
    if(ies.ie_tsch_timeslot_id == 0) {
      tsch_timing_us[i] = tsch_default_timing_us[i];
    } else {
      tsch_timing_us[i] = ies.ie_tsch_timeslot[i];
    }
    tsch_timing[i] = US_TO_RTIMERTICKS(tsch_timing_us[i]);
  }

  /* TSCH hopping sequence */
  if(ies.ie_channel_hopping_sequence_id == 0) {
    memcpy(tsch_hopping_sequence, TSCH_DEFAULT_HOPPING_SEQUENCE, sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE));
    TSCH_ASN_DIVISOR_INIT(tsch_hopping_sequence_length, sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE));
  } else {
    if(ies.ie_hopping_sequence_len <= sizeof(tsch_hopping_sequence)) {
      memcpy(tsch_hopping_sequence, ies.ie_hopping_sequence_list, ies.ie_hopping_sequence_len);
      TSCH_ASN_DIVISOR_INIT(tsch_hopping_sequence_length, ies.ie_hopping_sequence_len);
    } else {
      LOG_ERR("! parse_eb: hopping sequence too long (%u)\n", ies.ie_hopping_sequence_len);
      return 0;
    }
  }

#if TSCH_CHECK_TIME_AT_ASSOCIATION > 0
  /* Divide by 4k and multiply again to avoid integer overflow */
  uint32_t expected_asn = 4096 * TSCH_CLOCK_TO_SLOTS(clock_time() / 4096, tsch_timing_timeslot_length); /* Expected ASN based on our current time*/
  int32_t asn_threshold = TSCH_CHECK_TIME_AT_ASSOCIATION * 60ul * TSCH_CLOCK_TO_SLOTS(CLOCK_SECOND, tsch_timing_timeslot_length);
  int32_t asn_diff = (int32_t)tsch_current_asn.ls4b - expected_asn;
  if(asn_diff > asn_threshold) {
    LOG_ERR("! EB ASN rejected %lx %lx %ld\n",
           tsch_current_asn.ls4b, expected_asn, asn_diff);
    return 0;
  }
#endif

#if TSCH_INIT_SCHEDULE_FROM_EB
  /* Create schedule */
  if(ies.ie_tsch_slotframe_and_link.num_slotframes == 0) {
#if TSCH_SCHEDULE_WITH_6TISCH_MINIMAL
    LOG_INFO("parse_eb: no schedule, setting up minimal schedule\n");
    tsch_schedule_create_minimal();
#else
    LOG_INFO("parse_eb: no schedule\n");
#endif
  } else {
    /* First, empty current schedule */
    tsch_schedule_remove_all_slotframes();
    /* We support only 0 or 1 slotframe in this IE */
    int num_links = ies.ie_tsch_slotframe_and_link.num_links;
    if(num_links <= FRAME802154E_IE_MAX_LINKS) {
      int i;
      struct tsch_slotframe *sf = tsch_schedule_add_slotframe(
          ies.ie_tsch_slotframe_and_link.slotframe_handle,
          ies.ie_tsch_slotframe_and_link.slotframe_size);
      for(i = 0; i < num_links; i++) {
        tsch_schedule_add_link(sf,
            ies.ie_tsch_slotframe_and_link.links[i].link_options,
            LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
            ies.ie_tsch_slotframe_and_link.links[i].timeslot,
            ies.ie_tsch_slotframe_and_link.links[i].channel_offset, 1);
      }
    } else {
      LOG_ERR("! parse_eb: too many links in schedule (%u)\n", num_links);
      return 0;
    }
  }
#endif /* TSCH_INIT_SCHEDULE_FROM_EB */

  if(tsch_join_priority < TSCH_MAX_JOIN_PRIORITY) {
    struct tsch_neighbor *n;

    /* Add coordinator to list of neighbors, lock the entry */
    n = tsch_queue_add_nbr((linkaddr_t *)&frame.src_addr);

    if(n != NULL) {
      tsch_queue_update_time_source((linkaddr_t *)&frame.src_addr);

      /* Set PANID */
      frame802154_set_pan_id(frame.src_pid);

      /* Synchronize on EB */
      tsch_slot_operation_sync(timestamp - tsch_timing[tsch_ts_tx_offset], &tsch_current_asn);

      /* Update global flags */
      tsch_is_associated = 1;
      tsch_is_pan_secured = frame.fcf.security_enabled;
      tx_count = 0;
      rx_count = 0;
      sync_count = 0;
      min_drift_seen = 0;
      max_drift_seen = 0;

      /* Start sending keep-alives now that tsch_is_associated is set */
      tsch_schedule_keepalive(0);

#ifdef TSCH_CALLBACK_JOINING_NETWORK
      TSCH_CALLBACK_JOINING_NETWORK();
#endif

      tsch_association_count++;
      LOG_INFO("association done (%u), sec %u, PAN ID %x, asn-%x.%lx, jp %u, timeslot id %u, hopping id %u, slotframe len %u with %u links, from ",
             tsch_association_count,
             tsch_is_pan_secured,
             frame.src_pid,
             tsch_current_asn.ms1b, tsch_current_asn.ls4b, tsch_join_priority,
             ies.ie_tsch_timeslot_id,
             ies.ie_channel_hopping_sequence_id,
             ies.ie_tsch_slotframe_and_link.slotframe_size,
             ies.ie_tsch_slotframe_and_link.num_links);
      LOG_INFO_LLADDR((const linkaddr_t *)&frame.src_addr);
      LOG_INFO_("\n");

#if HCK_FORMATION_BOOTSTRAP_STATE_INFO
      uint8_t prev_bootstrap_state = hck_formation_bootstrap_state;
      uint64_t prev_state_transition_asn = hck_formation_state_transition_asn;
      hck_formation_bootstrap_state = HCK_BOOTSTRAP_STATE_1_TSCH_JOINED;
      //hck_formation_state_transition_asn = tsch_calculate_current_asn();
      hck_formation_state_transition_asn = (uint64_t)(tsch_current_asn.ls4b) 
                                         + ((uint64_t)(tsch_current_asn.ms1b) << 32);
      LOG_HCK_FORMATION("asso %u bs %u at %llx pbs %u pat %llx\n", 
                        tsch_association_count,
                        hck_formation_bootstrap_state, 
                        hck_formation_state_transition_asn,
                        prev_bootstrap_state,
                        prev_state_transition_asn);

    if(hck_fitst_transition_to_tsch_joined == 0) {
      hck_fitst_transition_to_tsch_joined = 1;
      hck_first_asn_tsch_joined = hck_formation_state_transition_asn;
      LOG_HCK_FORMATION("FTST %u at %llx\n", 
                        hck_formation_bootstrap_state, 
                        hck_formation_state_transition_asn);

      LOG_HCK_FORMATION("FASN %u TJ %llx RJ %llx JN %llx\n", 
                        hck_formation_bootstrap_state, 
                        hck_first_asn_tsch_joined,
                        hck_first_asn_rpl_joined,
                        hck_first_asn_joined_node);
    }
#endif

      tsch_log_association_count++;
      if(tsch_log_association_count > 1) {
        clock_inst_leaving_time = clock_time() - clock_last_leaving;
        clock_avg_leaving_time = (clock_avg_leaving_time * (tsch_log_association_count - 2) + clock_inst_leaving_time) / (tsch_log_association_count - 1);
      }
      LOG_HCK("asso %u leave_time %lu inst_l_time %lu |\n",
             tsch_log_association_count,
             clock_avg_leaving_time, 
             clock_inst_leaving_time);

      return 1;
    }
  }
  LOG_ERR("! did not associate.\n");
  return 0;
}
/* Processes and protothreads used by TSCH */

/*---------------------------------------------------------------------------*/
/* Scanning protothread, called by tsch_process:
 * Listen to different channels, and when receiving an EB,
 * attempt to associate.
 */
PT_THREAD(tsch_scan(struct pt *pt))
{
  PT_BEGIN(pt);

  static struct input_packet input_eb;
  static struct etimer scan_timer;
  /* Time when we started scanning on current_channel */
  static clock_time_t current_channel_since;

  TSCH_ASN_INIT(tsch_current_asn, 0, 0);

  etimer_set(&scan_timer, CLOCK_SECOND / TSCH_ASSOCIATION_POLL_FREQUENCY);
  current_channel_since = clock_time();

  while(!tsch_is_associated && !tsch_is_coordinator) {
    /* Hop to any channel offset */
    static uint8_t current_channel = 0;

    /* We are not coordinator, try to associate */
    rtimer_clock_t t0;
    int is_packet_pending = 0;
    clock_time_t now_time = clock_time();

    /* Switch to a (new) channel for scanning */
    if(current_channel == 0 || now_time - current_channel_since > TSCH_CHANNEL_SCAN_DURATION) {
      /* Pick a channel at random in TSCH_JOIN_HOPPING_SEQUENCE */
      uint8_t scan_channel = TSCH_JOIN_HOPPING_SEQUENCE[
          random_rand() % sizeof(TSCH_JOIN_HOPPING_SEQUENCE)];

      NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, scan_channel);
      current_channel = scan_channel;
      LOG_INFO("scanning on channel %u\n", scan_channel);

      current_channel_since = now_time;
    }

    /* Turn radio on and wait for EB */
    NETSTACK_RADIO.on();

    is_packet_pending = NETSTACK_RADIO.pending_packet();
    if(!is_packet_pending && NETSTACK_RADIO.receiving_packet()) {
      /* If we are currently receiving a packet, wait until end of reception */
      t0 = RTIMER_NOW();
      RTIMER_BUSYWAIT_UNTIL_ABS((is_packet_pending = NETSTACK_RADIO.pending_packet()), t0, RTIMER_SECOND / 100);
    }

    if(is_packet_pending) {
      rtimer_clock_t t1;
      /* Read packet */
      input_eb.len = NETSTACK_RADIO.read(input_eb.payload, TSCH_PACKET_MAX_LEN);

      if(input_eb.len > 0) {
        /* Save packet timestamp */
        NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &t0, sizeof(rtimer_clock_t));
        t1 = RTIMER_NOW();

        /* Parse EB and attempt to associate */
        LOG_INFO("scan: received packet (%u bytes) on channel %u\n", input_eb.len, current_channel);

        /* Sanity-check the timestamp */
        if(ABS(RTIMER_CLOCK_DIFF(t0, t1)) < 2ul * RTIMER_SECOND) {
          tsch_associate(&input_eb, t0);
        } else {
          LOG_WARN("scan: dropping packet, timestamp too far from current time %u %u\n",
            (unsigned)t0,
            (unsigned)t1
        );
        }
      }
    }

    if(tsch_is_associated) {
      /* End of association, turn the radio off */
      NETSTACK_RADIO.off();
    } else if(!tsch_is_coordinator) {
      /* Go back to scanning */
      etimer_reset(&scan_timer);
      PT_WAIT_UNTIL(pt, etimer_expired(&scan_timer));
    }
  }

  PT_END(pt);
}

/*---------------------------------------------------------------------------*/
/* The main TSCH process */
PROCESS_THREAD(tsch_process, ev, data)
{
  static struct pt scan_pt;

  PROCESS_BEGIN();

  while(1) {

    while(!tsch_is_associated) {
      if(tsch_is_coordinator) {
        /* We are coordinator, start operating now */
        tsch_start_coordinator();
      } else {
        /* Start scanning, will attempt to join when receiving an EB */
        PROCESS_PT_SPAWN(&scan_pt, tsch_scan(&scan_pt));
      }
    }

    //hckim record the asn when association is achieved
    TSCH_ASN_COPY(tsch_last_asn_associated, tsch_current_asn);

    /* We are part of a TSCH network, start slot operation */
    tsch_slot_operation_start();

    /* Yield our main process. Slot operation will re-schedule itself
     * as long as we are associated */
    PROCESS_YIELD_UNTIL(!tsch_is_associated);

#if HCK_FORMATION_BOOTSTRAP_STATE_INFO
    uint8_t prev_bootstrap_state = hck_formation_bootstrap_state;
    uint64_t prev_state_transition_asn = hck_formation_state_transition_asn;
    hck_formation_bootstrap_state = HCK_BOOTSTRAP_STATE_0_NEW_NODE;
    hck_formation_state_transition_asn = tsch_calculate_current_asn();
    LOG_HCK_FORMATION("leave %u bs %u at %llx pbs %u pat %llx\n", 
                      tsch_leaving_count + 1,
                      hck_formation_bootstrap_state,
                      hck_formation_state_transition_asn,
                      prev_bootstrap_state,
                      prev_state_transition_asn);
#endif

    print_log_tsch();

    tsch_timeslots_until_last_session = tsch_total_associated_timeslots;

    clock_last_leaving = clock_time();

    LOG_WARN("leaving the network, stats: tx %lu, rx %lu, sync %lu\n",
      tx_count, rx_count, sync_count);

    ++tsch_leaving_count;
    LOG_HCK("leaving %u |\n", tsch_leaving_count);

    /* Will need to re-synchronize */
    tsch_reset();
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/* A periodic process to send TSCH Enhanced Beacons (EB) */
PROCESS_THREAD(tsch_send_eb_process, ev, data)
{
  static struct etimer eb_timer;

  PROCESS_BEGIN();

  /* Wait until association */
  etimer_set(&eb_timer, CLOCK_SECOND / 10);
  while(!tsch_is_associated) {
    PROCESS_WAIT_UNTIL(etimer_expired(&eb_timer));
    etimer_reset(&eb_timer);
  }

  /* Set an initial delay except for coordinator, which should send an EB asap */
  if(!tsch_is_coordinator) {
    etimer_set(&eb_timer, TSCH_EB_PERIOD ? random_rand() % TSCH_EB_PERIOD : 0);
    PROCESS_WAIT_UNTIL(etimer_expired(&eb_timer));
  }

  while(1) {
    unsigned long delay;

    if(tsch_is_associated && tsch_current_eb_period > 0
#ifdef TSCH_RPL_CHECK_DODAG_JOINED
      /* Implementation section 6.3 of RFC 8180 */
      && TSCH_RPL_CHECK_DODAG_JOINED()
#endif /* TSCH_RPL_CHECK_DODAG_JOINED */
      /* don't send when in leaf mode */
      && !NETSTACK_ROUTING.is_in_leaf_mode()
        ) {
#if WITH_DRA
      dra_my_eb_seq++;
      dra_my_num_of_pkts++;
#if DRA_DBG
      LOG_HCK_DRA("dra send EB seq %u\n", dra_my_eb_seq);
#endif
#endif
      /* Enqueue EB only if there isn't already one in queue */
      if(tsch_queue_nbr_packet_count(n_eb) == 0) {
        uint8_t hdr_len = 0;
        uint8_t tsch_sync_ie_offset;
        /* Prepare the EB packet and schedule it to be sent */
        if(tsch_packet_create_eb(&hdr_len, &tsch_sync_ie_offset) > 0) {
          struct tsch_packet *p;
          /* Enqueue EB packet, for a single transmission only */
          if(!(p = tsch_queue_add_packet(&tsch_eb_address, 1, NULL, NULL))) {
            LOG_ERR("! could not enqueue EB packet\n");

            ++tsch_eb_packet_qloss_count;
            uint64_t tsch_eb_qloss_asn = tsch_calculate_current_asn();
            LOG_HCK("eb_qloss %u | at %llx\n", tsch_eb_packet_qloss_count, tsch_eb_qloss_asn);
          } else {
            LOG_INFO("TSCH: enqueue EB packet %u %u\n",
                     packetbuf_totlen(), packetbuf_hdrlen());

            ++tsch_eb_packet_enqueue_count;
            uint64_t tsch_eb_enqueue_asn = tsch_calculate_current_asn();
            LOG_HCK("eb_enq %u | at %llx\n", tsch_eb_packet_enqueue_count, tsch_eb_enqueue_asn);

            p->tsch_sync_ie_offset = tsch_sync_ie_offset;
            p->header_len = hdr_len;
          }
        }
      }
    }
    if(tsch_current_eb_period > 0) {
      /* Next EB transmission with a random delay
       * within [tsch_current_eb_period*0.75, tsch_current_eb_period[ */
      delay = (tsch_current_eb_period - tsch_current_eb_period / 4)
        + random_rand() % (tsch_current_eb_period / 4);
    } else {
      delay = TSCH_EB_PERIOD;
    }
    etimer_set(&eb_timer, delay);
    PROCESS_WAIT_UNTIL(etimer_expired(&eb_timer));
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/* A process that is polled from interrupt and calls tx/rx input
 * callbacks, outputs pending logs. */
PROCESS_THREAD(tsch_pending_events_process, ev, data)
{
  PROCESS_BEGIN();
  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
#if HCK_MOD_TSCH_SWAP_TX_RX_PROCESS_PENDING
    tsch_tx_process_pending();
    tsch_rx_process_pending();
#else
    tsch_rx_process_pending();
    tsch_tx_process_pending();
#endif
    tsch_log_process_pending();
    tsch_keepalive_process_pending();
#ifdef TSCH_CALLBACK_SELECT_CHANNELS
    TSCH_CALLBACK_SELECT_CHANNELS();
#endif
  }
  PROCESS_END();
}

/* Functions from the Contiki MAC layer driver interface */

/*---------------------------------------------------------------------------*/
static void
tsch_init(void)
{
  radio_value_t radio_rx_mode;
  radio_value_t radio_tx_mode;
  radio_value_t radio_max_payload_len;

  rtimer_clock_t t;

  /* Check that the platform provides a TSCH timeslot timing template */
  if(TSCH_DEFAULT_TIMESLOT_TIMING == NULL) {
    LOG_ERR("! platform does not provide a timeslot timing template.\n");
    return;
  }

  /* Check that the radio can correctly report its max supported payload */
  if(NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN, &radio_max_payload_len) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_CONST_MAX_PAYLOAD_LEN. Abort init.\n");
    return;
  }

  /* Radio Rx mode */
  if(NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_PARAM_RX_MODE. Abort init.\n");
    return;
  }
  /* Disable radio in frame filtering */
  radio_rx_mode &= ~RADIO_RX_MODE_ADDRESS_FILTER;
  /* Unset autoack */
  radio_rx_mode &= ~RADIO_RX_MODE_AUTOACK;
  /* Set radio in poll mode */
  radio_rx_mode |= RADIO_RX_MODE_POLL_MODE;
  if(NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support setting required RADIO_PARAM_RX_MODE. Abort init.\n");
    return;
  }

  /* Radio Tx mode */
  if(NETSTACK_RADIO.get_value(RADIO_PARAM_TX_MODE, &radio_tx_mode) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting RADIO_PARAM_TX_MODE. Abort init.\n");
    return;
  }
  /* Unset CCA */
  radio_tx_mode &= ~RADIO_TX_MODE_SEND_ON_CCA;
  if(NETSTACK_RADIO.set_value(RADIO_PARAM_TX_MODE, radio_tx_mode) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support setting required RADIO_PARAM_TX_MODE. Abort init.\n");
    return;
  }
  /* Test setting channel */
  if(NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, TSCH_DEFAULT_HOPPING_SEQUENCE[0]) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support setting channel. Abort init.\n");
    return;
  }
  /* Test getting timestamp */
  if(NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &t, sizeof(rtimer_clock_t)) != RADIO_RESULT_OK) {
    LOG_ERR("! radio does not support getting last packet timestamp. Abort init.\n");
    return;
  }
  /* Check max hopping sequence length vs default sequence length */
  if(TSCH_HOPPING_SEQUENCE_MAX_LEN < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE)) {
    LOG_ERR("! TSCH_HOPPING_SEQUENCE_MAX_LEN < sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE). Abort init.\n");
    return;
  }

  /* Init TSCH sub-modules */
  tsch_reset();
  tsch_queue_init();
  tsch_schedule_init();
  tsch_log_init();
  ringbufindex_init(&input_ringbuf, TSCH_MAX_INCOMING_PACKETS);
  ringbufindex_init(&dequeued_ringbuf, TSCH_DEQUEUED_ARRAY_SIZE);
#if TSCH_AUTOSELECT_TIME_SOURCE
  nbr_table_register(sync_stats, NULL);
  LOG_INFO("nbr_tbl_reg: sync_stats %d\n", sync_stats->index);
#endif /* TSCH_AUTOSELECT_TIME_SOURCE */

  tsch_packet_seqno = random_rand();
  tsch_is_initialized = 1;

#if TSCH_AUTOSTART
  /* Start TSCH operation.
   * If TSCH_AUTOSTART is not set, one needs to call NETSTACK_MAC.on() to start TSCH. */
  NETSTACK_MAC.on();
#endif /* TSCH_AUTOSTART */

#if TSCH_WITH_SIXTOP
  sixtop_init();
#endif

  tsch_stats_init();
}
/*---------------------------------------------------------------------------*/
/* Function send for TSCH-MAC, puts the packet in packetbuf in the MAC queue */
static void
send_packet(mac_callback_t sent, void *ptr)
{
  int ret = MAC_TX_DEFERRED;
  int hdr_len = 0;
  const linkaddr_t *addr = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  uint8_t max_transmissions = 0;

  if(!tsch_is_associated) {
    if(!tsch_is_initialized) {
      LOG_WARN("! not initialized (see earlier logs), drop outgoing packet\n");
    } else {
      LOG_WARN("! not associated, drop outgoing packet\n");
    }
    ret = MAC_TX_ERR;
    mac_call_sent_callback(sent, ptr, ret, 1);
    return;
  }

  /* Ask for ACK if we are sending anything other than broadcast */
  if(!linkaddr_cmp(addr, &linkaddr_null)) {
    /* PACKETBUF_ATTR_MAC_SEQNO cannot be zero, due to a pecuilarity
           in framer-802154.c. */
    if(++tsch_packet_seqno == 0) {
      tsch_packet_seqno++;
    }
    packetbuf_set_attr(PACKETBUF_ATTR_MAC_SEQNO, tsch_packet_seqno);
    packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);
  } else {
    /* Broadcast packets shall be added to broadcast queue
     * The broadcast address in Contiki is linkaddr_null which is equal
     * to tsch_eb_address */
    addr = &tsch_broadcast_address;
  }

  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);

#if HCK_MOD_TSCH_PIGGYBACKING_HEADER_IE_32BITS || HCK_FORMATION_PACKET_TYPE_INFO
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_METADATA, 1);
#endif

#if WITH_DRA
    if(linkaddr_cmp(addr, &linkaddr_null)) {
      dra_my_bc_seq++;
#if DRA_DBG
      LOG_HCK_DRA("dra send_pkt bc seq %u\n", dra_my_bc_seq);
#endif
    } else {
      dra_my_uc_seq++;
#if DRA_DBG
      LOG_HCK_DRA("dra send_pkt uc seq %u\n", dra_my_uc_seq);
#endif
    }
    dra_my_num_of_pkts++;
#endif

#if LLSEC802154_ENABLED
  tsch_security_set_packetbuf_attr(FRAME802154_DATAFRAME);
#endif /* LLSEC802154_ENABLED */

#if !NETSTACK_CONF_BRIDGE_MODE
  /*
   * In the Contiki stack, the source address of a frame is set at the RDC
   * layer. Since TSCH doesn't use any RDC protocol and bypasses the layer to
   * transmit a frame, it should set the source address by itself.
   */
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif

  max_transmissions = packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS);
  if(max_transmissions == 0) {
    /* If not set by the application, use the default TSCH value */
    max_transmissions = TSCH_MAC_MAX_FRAME_RETRIES + 1;
  }

  /* OST-03-01: Piggybacks N */
  if((hdr_len = NETSTACK_FRAMER.create()) < 0) {
    LOG_ERR("! can't send packet due to framer error\n");
    ret = MAC_TX_ERR;
  } else {
    struct tsch_packet *p;
    struct tsch_neighbor *n;
    /* Enqueue packet */
    p = tsch_queue_add_packet(addr, max_transmissions, sent, ptr);
    n = tsch_queue_get_nbr(addr);
    if(p == NULL) {
      LOG_ERR("! can't send packet to ");
      LOG_ERR_LLADDR(addr);
      LOG_ERR_(" with seqno %u, queue %u/%u %u/%u\n",
          tsch_packet_seqno, tsch_queue_nbr_packet_count(n),
          TSCH_QUEUE_NUM_PER_NEIGHBOR, tsch_queue_global_packet_count(),
          QUEUEBUF_NUM);
      ret = MAC_TX_ERR;
      if(sent == keepalive_packet_sent) {
        ++tsch_ka_packet_qloss_count;
        LOG_HCK("ka_qloss %u | to %u seq %u\n", tsch_ka_packet_qloss_count,
                                              HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                              tsch_packet_seqno);
      } else {
        ++tsch_ip_packet_qloss_count;
        if(packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6) {
          ++tsch_ip_icmp6_packet_qloss_count;
          LOG_HCK("ip_qloss %u ip_icmp6_qloss %u | to %u seq %u\n", 
                tsch_ip_packet_qloss_count, 
                tsch_ip_icmp6_packet_qloss_count,
                HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                tsch_packet_seqno);
        } else {
          ++tsch_ip_udp_packet_qloss_count;
          LOG_HCK("ip_qloss %u ip_udp_qloss %u | to %u seq %u\n", 
                tsch_ip_packet_qloss_count, 
                tsch_ip_udp_packet_qloss_count,
                HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                tsch_packet_seqno);
        }
      }
    } else {
      p->header_len = hdr_len;
      LOG_INFO("send packet to ");
      LOG_INFO_LLADDR(addr);
      LOG_INFO_(" with seqno %u, queue %u/%u %u/%u, len %u %u\n",
             tsch_packet_seqno, tsch_queue_nbr_packet_count(n),
             TSCH_QUEUE_NUM_PER_NEIGHBOR, tsch_queue_global_packet_count(),
             QUEUEBUF_NUM, p->header_len, queuebuf_datalen(p->qb));
      if(sent == keepalive_packet_sent) {
        ++tsch_ka_packet_enqueue_count;
        LOG_HCK("ka_enq %u | to %u seq %u len %u %u\n", tsch_ka_packet_enqueue_count,
                                                      HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                                      tsch_packet_seqno,
                                                      p->header_len, 
                                                      queuebuf_datalen(p->qb));
      } else {
        ++tsch_ip_packet_enqueue_count;
        if(packetbuf_attr(PACKETBUF_ATTR_NETWORK_ID) == UIP_PROTO_ICMP6) {
          ++tsch_ip_icmp6_packet_enqueue_count;
          LOG_HCK("ip_enq %u ip_icmp6_enq %u | to %u seq %u len %u %u\n", 
                tsch_ip_packet_enqueue_count, 
                tsch_ip_icmp6_packet_enqueue_count,
                HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                tsch_packet_seqno,
                p->header_len, 
                queuebuf_datalen(p->qb));
        } else {
          ++tsch_ip_udp_packet_enqueue_count;
          LOG_HCK("ip_enq %u ip_udp_enq %u | to %u seq %u len %u %u\n", 
                tsch_ip_packet_enqueue_count, 
                tsch_ip_udp_packet_enqueue_count,
                HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                tsch_packet_seqno,
                p->header_len, 
                queuebuf_datalen(p->qb));
        }
      }

    }
  }
  if(ret != MAC_TX_DEFERRED) {
    mac_call_sent_callback(sent, ptr, ret, 1);
  }
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
  int frame_parsed = 1;

  frame_parsed = NETSTACK_FRAMER.parse();

  if(frame_parsed < 0) {
    LOG_ERR("! failed to parse %u\n", packetbuf_datalen());
  } else {
    int duplicate = 0;

    /* Seqno of 0xffff means no seqno */
    if(packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO) != 0xffff) {
      /* Check for duplicates */
      duplicate = mac_sequence_is_duplicate();
      if(duplicate) {
        /* Drop the packet. */
        LOG_WARN("! drop dup ll from ");
        LOG_WARN_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
        LOG_WARN_(" seqno %u\n", packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
      } else {
        mac_sequence_register_seqno();
      }
    }

    if(!duplicate) {
      LOG_INFO("received from ");
      LOG_INFO_LLADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
      LOG_INFO_(" with seqno %u\n", packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
#if TSCH_WITH_SIXTOP
      sixtop_input();
#endif /* TSCH_WITH_SIXTOP */
      NETSTACK_NETWORK.input();
    }
  }
}
/*---------------------------------------------------------------------------*/
static int
turn_on(void)
{
  if(tsch_is_initialized == 1 && tsch_is_started == 0) {
    tsch_is_started = 1;
    /* Process tx/rx callback and log messages whenever polled */
    process_start(&tsch_pending_events_process, NULL);
    if(TSCH_EB_PERIOD > 0) {
      /* periodically send TSCH EBs */
      process_start(&tsch_send_eb_process, NULL);
    }
    /* try to associate to a network or start one if setup as coordinator */
    process_start(&tsch_process, NULL);
    LOG_INFO("starting as %s\n", tsch_is_coordinator ? "coordinator": "node");
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
turn_off(void)
{
  NETSTACK_RADIO.off();
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
max_payload(void)
{
  int framer_hdrlen;
  radio_value_t max_radio_payload_len;
  radio_result_t res;

  if(!tsch_is_associated) {
    LOG_WARN("Cannot compute max payload size: not associated\n");
    return 0;
  }

  res = NETSTACK_RADIO.get_value(RADIO_CONST_MAX_PAYLOAD_LEN,
                                 &max_radio_payload_len);

  if(res == RADIO_RESULT_NOT_SUPPORTED) {
    LOG_ERR("Failed to retrieve max radio driver payload length\n");
    return 0;
  }

  /* Set packetbuf security attributes */
  tsch_security_set_packetbuf_attr(FRAME802154_DATAFRAME);

  framer_hdrlen = NETSTACK_FRAMER.length();
  if(framer_hdrlen < 0) {
    return 0;
  }

  /* Setup security... before. */
  return MIN(max_radio_payload_len, TSCH_PACKET_MAX_LEN)
    - framer_hdrlen
    - LLSEC802154_PACKETBUF_MIC_LEN();
}
/*---------------------------------------------------------------------------*/
const struct mac_driver tschmac_driver = {
  "TSCH",
  tsch_init,
  send_packet,
  packet_input,
  turn_on,
  turn_off,
  max_payload,
};
/*---------------------------------------------------------------------------*/
/** @} */
