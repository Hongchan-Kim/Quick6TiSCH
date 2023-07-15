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
 *         IEEE 802.15.4 TSCH MAC schedule manager.
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 *         Beshr Al Nahas <beshr@sics.se>
 *         Atis Elsts <atis.elsts@edi.lv>
 */

/**
 * \addtogroup tsch
 * @{
*/

#include "contiki.h"
#include "dev/leds.h"
#include "lib/memb.h"
#include "net/nbr-table.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/framer/frame802154.h"
#include "sys/process.h"
#include "sys/rtimer.h"
#include <string.h>

#if WITH_OST
#include "orchestra.h"
#endif

#if WITH_A3
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/routing/rpl-classic/rpl.h"
#endif

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "TSCH Sched"
#define LOG_LEVEL LOG_LEVEL_MAC

#if WITH_ALICE
#ifdef ALICE_TIME_VARYING_SCHEDULING
void ALICE_TIME_VARYING_SCHEDULING(); 
#endif
#endif

/* Pre-allocated space for links */
MEMB(link_memb, struct tsch_link, TSCH_SCHEDULE_MAX_LINKS);
/* Pre-allocated space for slotframes */
MEMB(slotframe_memb, struct tsch_slotframe, TSCH_SCHEDULE_MAX_SLOTFRAMES);
/* List of slotframes (each slotframe holds its own list of links) */
LIST(slotframe_list);

/* Adds and returns a slotframe (NULL if failure) */
struct tsch_slotframe *
tsch_schedule_add_slotframe(uint16_t handle, uint16_t size)
{
  if(size == 0) {
    return NULL;
  }

  if(tsch_schedule_get_slotframe_by_handle(handle)) {
    /* A slotframe with this handle already exists */
    return NULL;
  }

  if(tsch_get_lock()) {
    struct tsch_slotframe *sf = memb_alloc(&slotframe_memb);
    if(sf != NULL) {
      /* Initialize the slotframe */
      sf->handle = handle;
      TSCH_ASN_DIVISOR_INIT(sf->size, size);
      LIST_STRUCT_INIT(sf, links_list);
      /* Add the slotframe to the global list */
      list_add(slotframe_list, sf);
    }
    LOG_INFO("add_slotframe %u %u\n",
           handle, size);
    tsch_release_lock();
    return sf;
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION && MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION
uint8_t
tsch_schedule_get_next_timeslot_available_or_not(struct tsch_asn_t *asn, uint16_t *time_to_earliest)
{
#if WITH_ALICE
  /* ALICE: ASFN at the start of the ongoing slot operation. 
    Derived from 'alice_current_asn'. */
  uint64_t dbt_current_asfn = 0;
  struct tsch_slotframe *alice_uc_sf = tsch_schedule_get_slotframe_by_handle(ALICE_UNICAST_SF_HANDLE);
  struct tsch_asn_t temp_asn;
  TSCH_ASN_COPY(temp_asn, *asn);
  uint16_t mod1 = TSCH_ASN_MOD(temp_asn, alice_uc_sf->size);
  TSCH_ASN_DEC(temp_asn, mod1);
  dbt_current_asfn = TSCH_ASN_DIVISION(temp_asn, alice_uc_sf->size);
#endif

  uint16_t minimum_time_to_timeslot = 0;

  /* Check slotframe schedule */
  if(!tsch_is_locked()) { 
    struct tsch_slotframe *sf = list_head(slotframe_list);
    while(sf != NULL) {
#if !WITH_ALICE
      uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
      struct tsch_link *l = list_head(sf->links_list);

      while(l != NULL) {
        uint16_t time_to_timeslot =
          l->timeslot > timeslot ?
          l->timeslot - timeslot :
          sf->size.val + l->timeslot - timeslot;

        if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
          minimum_time_to_timeslot = time_to_timeslot;
        }

        if(time_to_timeslot == 1) {
          *time_to_earliest = minimum_time_to_timeslot;
          return 0;
        }

        l = list_item_next(l);
      }
#else /* WITH_ALICE */
      /* 
       * First case: dbt_current_asfn < alice_lastly_scheduled_asfn < alice_next_asfn_of_lastly_scheduled_asfn.
       * Then, check unicast slotframes of alice_lastly_scheduled_asfn and alice_next_asfn_of_lastly_scheduled_asfn.
       * We do not need to check the unicast slotframe of current ASFN,
       * because the difference between dbt_current_asfn and alice_lastly_scheduled_asfn means 
       * that all links of unicast slotframe of current ASFN are expired and
       * unicast slotframe already has links of alice_lastly_scheduled_asfn.
       */
      if(dbt_current_asfn != alice_lastly_scheduled_asfn && dbt_current_asfn != alice_next_asfn_of_lastly_scheduled_asfn) {
        if(sf->handle != ALICE_UNICAST_SF_HANDLE && sf->handle != ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) {
          /* EB slotframe or CS slotframe */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            uint16_t time_to_timeslot =
              l->timeslot > timeslot ?
              l->timeslot - timeslot :
              sf->size.val + l->timeslot - timeslot;

            if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
              minimum_time_to_timeslot = time_to_timeslot;
            }

            if(time_to_timeslot == 1) {
              *time_to_earliest = minimum_time_to_timeslot;
              return 0;
            }

            l = list_item_next(l);
          }
        } else if(sf->handle == ALICE_UNICAST_SF_HANDLE) { /* Unicast slotframe of alice_lastly_scheduled_asfn */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            uint16_t time_to_timeslot = sf->size.val + l->timeslot - timeslot;

            if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
              minimum_time_to_timeslot = time_to_timeslot;
            }

            if(time_to_timeslot == 1) {
              *time_to_earliest = minimum_time_to_timeslot;
              return 0;
            }

            l = list_item_next(l);
          }
        } else if(sf->handle == ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) { /* Unicast slotframe of alice_next_asfn_of_lastly_scheduled_asfn */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            uint16_t time_to_timeslot = 2 * sf->size.val + l->timeslot - timeslot;

            if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
              minimum_time_to_timeslot = time_to_timeslot;
            }

            if(time_to_timeslot == 1) {
              *time_to_earliest = minimum_time_to_timeslot;
              return 0;
            }

            l = list_item_next(l);
          }
        }
      /* 
       * Second case: alice_lastly_scheduled_asfn == dbt_current_asfn < alice_next_asfn_of_lastly_scheduled_asfn.
       * Then, check unicast slotframes of alice_lastly_scheduled_asfn and alice_next_asfn_of_lastly_scheduled_asfn.
       */
      } else if(dbt_current_asfn == alice_lastly_scheduled_asfn) {
        if(sf->handle != ALICE_UNICAST_SF_HANDLE && sf->handle != ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) {
          /* EB slotframe or CS slotframe */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            uint16_t time_to_timeslot =
              l->timeslot > timeslot ?
              l->timeslot - timeslot :
              sf->size.val + l->timeslot - timeslot;

            if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
              minimum_time_to_timeslot = time_to_timeslot;
            }

            if(time_to_timeslot == 1) {
              *time_to_earliest = minimum_time_to_timeslot;
              return 0;
            }

            l = list_item_next(l);
          }
        } else if(sf->handle == ALICE_UNICAST_SF_HANDLE) { /* Unicast slotframe of alice_lastly_scheduled_asfn */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            if(l->timeslot > timeslot) {
              uint16_t time_to_timeslot = l->timeslot - timeslot;

              if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
                minimum_time_to_timeslot = time_to_timeslot;
              }

              if(time_to_timeslot == 1) {
                *time_to_earliest = minimum_time_to_timeslot;
                return 0;
              }
            }

            l = list_item_next(l);
          }
        } else if(sf->handle == ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) { /* Unicast slotframe of alice_next_asfn_of_lastly_scheduled_asfn */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            uint16_t time_to_timeslot = sf->size.val + l->timeslot - timeslot;

            if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
              minimum_time_to_timeslot = time_to_timeslot;
            }

            if(time_to_timeslot == 1) {
              *time_to_earliest = minimum_time_to_timeslot;
              return 0;
            }

            l = list_item_next(l);
          }
        }
      /* 
       * Third case: alice_lastly_scheduled_asfn < dbt_current_asfn == alice_next_asfn_of_lastly_scheduled_asfn.
       * Then, check unicast slotframe of alice_next_asfn_of_lastly_scheduled_asfn.
       */
      } else if(dbt_current_asfn == alice_next_asfn_of_lastly_scheduled_asfn) {
        if(sf->handle != ALICE_UNICAST_SF_HANDLE && sf->handle != ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) {
          /* EB slotframe or CS slotframe */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            uint16_t time_to_timeslot =
              l->timeslot > timeslot ?
              l->timeslot - timeslot :
              sf->size.val + l->timeslot - timeslot;

            if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
              minimum_time_to_timeslot = time_to_timeslot;
            }

            if(time_to_timeslot == 1) {
              *time_to_earliest = minimum_time_to_timeslot;
              return 0;
            }

            l = list_item_next(l);
          }
        } else if(sf->handle == ALICE_UNICAST_SF_HANDLE) { /* Unicast slotframe of alice_lastly_scheduled_asfn */
          sf = list_item_next(sf);
          continue;

        } else if(sf->handle == ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) { /* Unicast slotframe of alice_next_asfn_of_lastly_scheduled_asfn */
          uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
          struct tsch_link *l = list_head(sf->links_list);

          while(l != NULL) {
            if(l->timeslot > timeslot) {
              uint16_t time_to_timeslot = l->timeslot - timeslot;

              if(minimum_time_to_timeslot == 0 || time_to_timeslot < minimum_time_to_timeslot) {
                minimum_time_to_timeslot = time_to_timeslot;
              }

              if(time_to_timeslot == 1) {
                *time_to_earliest = minimum_time_to_timeslot;
                return 0;
              }
            }

            l = list_item_next(l);
          }
        }
      }
#endif
      sf = list_item_next(sf);
    }
    *time_to_earliest = minimum_time_to_timeslot;
    return 1;
  }
  *time_to_earliest = minimum_time_to_timeslot;
  return 0; /* TSCH locked, unable to check slotframe schedule */
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_OST && OST_ON_DEMAND_PROVISION
uint16_t
tsch_schedule_get_subsequent_schedule(struct tsch_asn_t *asn)
{
  uint16_t ssq_schedule = 0;
  uint8_t used[16]; /* 0 ~ 15th slot is used */
  
  /* Check slotframe schedule */
  if(!tsch_is_locked()) { 
    struct tsch_slotframe *sf = list_head(slotframe_list);
    while(sf != NULL) {
      uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
      struct tsch_link *l = list_head(sf->links_list);

      while(l != NULL) {
        uint16_t time_to_timeslot =
          l->timeslot > timeslot ?
          l->timeslot - timeslot :
          sf->size.val + l->timeslot - timeslot;

        if((time_to_timeslot - 1) < 16) {
          used[time_to_timeslot - 1] = 1;
        }

        l = list_item_next(l);
      }
      sf = list_item_next(sf);
    }
  }

  /* Check matching slot schedule (check ssq up to now) */
  uint64_t curr_ASN = (uint64_t)(asn->ls4b) + ((uint64_t)(asn->ms1b) << 32);
  uint64_t ssq_ASN;

  uint8_t i;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == 0 && ost_ssq_schedule_list[i].asn.ms1b == 0) {
      /* Do nothing */
    } else {
      ssq_ASN = (uint64_t)(ost_ssq_schedule_list[i].asn.ls4b) + ((uint64_t)(ost_ssq_schedule_list[i].asn.ms1b) << 32);
      uint64_t time_to_timeslot = ssq_ASN - curr_ASN;
      if(time_to_timeslot == 0) {
        /* Current asn */
      } else if(0 < time_to_timeslot && (time_to_timeslot - 1) < 16) {
        used[time_to_timeslot - 1] = 1;
      } else {
        /* Do nothing */
      }
    }
  }

  for(i = 0; i < 16; i++) {
    if(used[i] == 1) {
      ssq_schedule = ssq_schedule | (1 << i);
    }
  }

  return ssq_schedule;
}
/*---------------------------------------------------------------------------*/
uint8_t
ost_earlier_ssq_schedule_list(uint16_t *time_to_orig_schedule, struct tsch_link **link)
{
  struct tsch_link *earliest_link;
  uint64_t curr_ASN = (uint64_t)(tsch_current_asn.ls4b) + ((uint64_t)(tsch_current_asn.ms1b) << 32);
  uint64_t ssq_ASN;
  uint64_t earliest_ASN = 0;

  /* First, choose the earliest ssq schedule */
  uint8_t i;
  uint8_t earliest_i;
  for(i = 0; i < 16; i++) {
    if(ost_ssq_schedule_list[i].asn.ls4b == 0 && ost_ssq_schedule_list[i].asn.ms1b == 0) {
      /* Do nothing */
    } else {
      ssq_ASN = (uint64_t)(ost_ssq_schedule_list[i].asn.ls4b) + ((uint64_t)(ost_ssq_schedule_list[i].asn.ms1b) << 32);
      if(earliest_ASN == 0 || ssq_ASN < earliest_ASN) {
        earliest_ASN = ssq_ASN;
        earliest_link = &(ost_ssq_schedule_list[i].link);
        earliest_i = i;
        if(earliest_link == NULL) {
          /* ERROR: Null temp earliest_link */
        }
      }

      if(ssq_ASN <= curr_ASN) {
        /* ERROR: ssq_ASN <= curr_ASN */
        ost_ssq_schedule_list[i].asn.ls4b = 0;
        ost_ssq_schedule_list[i].asn.ms1b = 0; 
        return 0; 
      }
    }
  }

  if(earliest_ASN == 0) {
    /* No pending ssq schedule */
    return 0;
  } else {
    uint64_t time_to_earliest = earliest_ASN - curr_ASN;

    if(time_to_earliest < *time_to_orig_schedule) {
      /* Earlier ssq exists */
      *time_to_orig_schedule = (uint16_t)time_to_earliest;
      *link = earliest_link;
      if(link == NULL) {
        /* ERROR: Null earliest_link */
      }
      return 1;
    } else if(time_to_earliest == *time_to_orig_schedule) {
      /* ERROR: ssq overlap with orig schedule */
      ost_ssq_schedule_list[earliest_i].asn.ms1b = 0;
      ost_ssq_schedule_list[earliest_i].asn.ls4b = 0;

      return 0;
    } else {
      return 0;
    }
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if WITH_ALICE /* alice implementation */
/* Thomas Wang 32bit-Interger Mix Function */
uint16_t
alice_real_hash(uint32_t value, uint16_t mod)
{
  uint32_t a = value;

  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  
  return (uint16_t)(a % (uint32_t)mod);
}
/*---------------------------------------------------------------------------*/
/* Thomas Wang 32bit-Interger Mix Function */
uint16_t
alice_real_hash5(uint32_t value, uint16_t mod) //ksh..
{
  uint32_t a = value;
  a = ((((a + (a >> 16)) ^ (a >> 9)) ^ (a << 3)) ^ (a >> 5));
  return (uint16_t)(a % (uint32_t)mod);
}
#endif
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Removes all slotframes, resulting in an empty schedule */
int
tsch_schedule_remove_all_slotframes(void)
{
  struct tsch_slotframe *sf;
  while((sf = list_head(slotframe_list))) {
    if(tsch_schedule_remove_slotframe(sf) == 0) {
      return 0;
    }
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Removes a slotframe Return 1 if success, 0 if failure */
int
tsch_schedule_remove_slotframe(struct tsch_slotframe *slotframe)
{
  if(slotframe != NULL) {
    /* Remove all links belonging to this slotframe */
    struct tsch_link *l;
    while((l = list_head(slotframe->links_list))) {
      tsch_schedule_remove_link(slotframe, l);
    }

    /* Now that the slotframe has no links, remove it. */
    if(tsch_get_lock()) {
      LOG_INFO("remove slotframe %u %u\n", slotframe->handle, slotframe->size.val);
      memb_free(&slotframe_memb, slotframe);
      list_remove(slotframe_list, slotframe);
      tsch_release_lock();
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Looks for a slotframe from a handle */
struct tsch_slotframe *
tsch_schedule_get_slotframe_by_handle(uint16_t handle)
{
  if(!tsch_is_locked()) {
    struct tsch_slotframe *sf = list_head(slotframe_list);
    while(sf != NULL) {
      if(sf->handle == handle) {
        return sf;
      }
      sf = list_item_next(sf);
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Looks for a link from a handle */
struct tsch_link *
tsch_schedule_get_link_by_handle(uint16_t handle)
{
  if(!tsch_is_locked()) {
    struct tsch_slotframe *sf = list_head(slotframe_list);
    while(sf != NULL) {
      struct tsch_link *l = list_head(sf->links_list);
      /* Loop over all items. Assume there is max one link per timeslot */
      while(l != NULL) {
        if(l->handle == handle) {
          return l;
        }
        l = list_item_next(l);
      }
      sf = list_item_next(sf);
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
#if HCK_LOG_TSCH_LINK_ADD_REMOVE || ENABLE_LOG_ALICE_LINK_ADD_REMOVE
static const char *
print_link_options(uint16_t link_options)
{
  static char buffer[20];
  unsigned length;

  buffer[0] = '\0';
  if(link_options & LINK_OPTION_TX) {
    strcat(buffer, "Tx|");
  }
  if(link_options & LINK_OPTION_RX) {
    strcat(buffer, "Rx|");
  }
  if(link_options & LINK_OPTION_SHARED) {
    strcat(buffer, "Sh|");
  }
  length = strlen(buffer);
  if(length > 0) {
    buffer[length - 1] = '\0';
  }

  return buffer;
}
/*---------------------------------------------------------------------------*/
static const char *
print_link_type(uint16_t link_type)
{
  switch(link_type) {
  case LINK_TYPE_NORMAL:
    return "NORMAL";
  case LINK_TYPE_ADVERTISING:
    return "ADV";
  case LINK_TYPE_ADVERTISING_ONLY:
    return "ADV_ONLY";
  default:
    return "?";
  }
}
#endif /* HCK_LOG_TSCH_LINK_ADD_REMOVE || ENABLE_LOG_ALICE_LINK_ADD_REMOVE */
/*---------------------------------------------------------------------------*/
/* Adds a link to a slotframe, return a pointer to it (NULL if failure) */
struct tsch_link *
tsch_schedule_add_link(struct tsch_slotframe *slotframe,
                       uint8_t link_options, enum link_type link_type, const linkaddr_t *address,
                       uint16_t timeslot, uint16_t channel_offset, uint8_t do_remove)
{
  struct tsch_link *l = NULL;
  if(slotframe != NULL) {
    /* We currently support only one link per timeslot in a given slotframe. */

    /* Validation of specified timeslot and channel_offset */
    if(timeslot > (slotframe->size.val - 1)) {
      LOG_ERR("! add_link invalid timeslot: %u\n", timeslot);
      return NULL;
    }
#if WITH_ALICE /* alice implementation */
    else if(channel_offset > 15) {
      LOG_ERR("! add_link invalid channel_offset: %u\n", channel_offset);
      return NULL;
    }
#endif

    if(do_remove) {
      /* Start with removing the link currently installed at this timeslot (needed
       * to keep neighbor state in sync with link options etc.) */
      tsch_schedule_remove_link_by_timeslot(slotframe, timeslot, channel_offset);
    }
    if(!tsch_get_lock()) {
      LOG_ERR("! add_link memb_alloc couldn't take lock\n");
    } else {
      l = memb_alloc(&link_memb);
      if(l == NULL) {
        LOG_ERR("! add_link memb_alloc failed\n");
        tsch_release_lock();
      } else {
        static int current_link_handle = 0;
        struct tsch_neighbor *n;
        /* Add the link to the slotframe */
        list_add(slotframe->links_list, l);
        /* Initialize link */
        l->handle = current_link_handle++;
        l->link_options = link_options;
        l->link_type = link_type;
        l->slotframe_handle = slotframe->handle;
        l->timeslot = timeslot;
        l->channel_offset = channel_offset;
        l->data = NULL;
        if(address == NULL) {
          address = &linkaddr_null;
        }
        linkaddr_copy(&l->addr, address);

#if HCK_LOG_TSCH_LINK_ADD_REMOVE
        LOG_INFO("add_link sf=%u opt=%s type=%s ts=%u ch=%u addr=",
                 slotframe->handle,
                 print_link_options(link_options),
                 print_link_type(link_type), timeslot, channel_offset);
        LOG_INFO_LLADDR(address);
        LOG_INFO_("\n");
#endif /* HCK_LOG_TSCH_LINK_ADD_REMOVE */

        /* Release the lock before we update the neighbor (will take the lock) */
        tsch_release_lock();

        if(l->link_options & LINK_OPTION_TX) {
          n = tsch_queue_add_nbr(&l->addr);
          /* We have a tx link to this neighbor, update counters */
          if(n != NULL) {
            n->tx_links_count++;
            if(!(l->link_options & LINK_OPTION_SHARED)) {
              n->dedicated_tx_links_count++;
            }
          }
        }
      }
    }
  }
  return l;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#if WITH_ALICE /* alice implementation */
/* set link option by timeslot and channel offset */
struct tsch_link *
alice_tsch_schedule_set_link_option_by_ts_choff(struct tsch_slotframe *slotframe, uint16_t timeslot, uint16_t channel_offset, uint8_t* link_options)
{
#ifdef ALICE_TIME_VARYING_SCHEDULING
  if(1) { //original: if(!tsch_is_locked()) {
#else
  if(!tsch_is_locked()) { 
#endif
    if(slotframe != NULL) {
      struct tsch_link *l = list_head(slotframe->links_list);
      /* Loop over all items. Assume there is max one link per timeslot */
      while(l != NULL) {
        if(l->timeslot == timeslot && l->channel_offset == channel_offset) {
          *link_options |= l->link_options;
          l->link_options |= *link_options;
        }
        l = list_item_next(l);
      }
      return l;
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
/* Adds a link to a slotframe, return a pointer to it (NULL if failure) */
struct tsch_link *
alice_tsch_schedule_add_link(struct tsch_slotframe *slotframe,
                       uint8_t link_options, enum link_type link_type, const linkaddr_t *address, 
                       uint16_t timeslot, uint16_t channel_offset,
                       const linkaddr_t *nbr_addr)
{
  struct tsch_link *l = NULL;
  if(slotframe != NULL) {
    /* We currently support only one link per timeslot in a given slotframe. */

    /* Validation of specified timeslot and channel_offset */
    if(timeslot > (slotframe->size.val - 1)) {
      LOG_ERR("! add_link invalid timeslot: %u\n", timeslot);
      return NULL;
    } else if(channel_offset > 15) {
      LOG_ERR("! add_link invalid channel_offset: %u\n", channel_offset);
      return NULL;
    }

#ifdef ALICE_TIME_VARYING_SCHEDULING
    if(0) {//if(!tsch_get_lock()) {
#else
    if(!tsch_get_lock()) {
#endif
      LOG_ERR("! add_link memb_alloc couldn't take lock\n");
    } else {
      l = memb_alloc(&link_memb);
      if(l == NULL) {
        LOG_ERR("! add_link memb_alloc failed\n");
        tsch_release_lock();
      } else {
        static int current_link_handle = 0;
        struct tsch_neighbor *n;
        /* Add the link to the slotframe */
        list_add(slotframe->links_list, l);
        /* Initialize link */
        l->handle = current_link_handle++;
        
        /* alice-implementation */
        alice_tsch_schedule_set_link_option_by_ts_choff(slotframe, timeslot, channel_offset, &link_options);

        l->link_options = link_options;
        l->link_type = link_type;
        l->slotframe_handle = slotframe->handle;
        l->timeslot = timeslot;
        l->channel_offset = channel_offset;
        l->data = NULL;
        if(address == NULL) {
          address = &linkaddr_null;
        }
        linkaddr_copy(&l->addr, address);
#if WITH_A3
        linkaddr_copy(&l->a3_nbr_addr, nbr_addr);
#endif

#if ENABLE_LOG_ALICE_LINK_ADD_REMOVE
        TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "a_l id=%u ts=%u ch=%u op=%s ty=%s",
                    HCK_GET_NODE_ID_FROM_LINKADDR(nbr_addr),
                    timeslot, 
                    channel_offset,
                    print_link_options(link_options),
                    print_link_type(link_type));
        );
#endif

        /* Release the lock before we update the neighbor (will take the lock) */
        tsch_release_lock();

        if(l->link_options & LINK_OPTION_TX) {
          n = tsch_queue_add_nbr(&l->addr);
          /* We have a tx link to this neighbor, update counters */
          if(n != NULL) {
            n->tx_links_count++;
            if(!(l->link_options & LINK_OPTION_SHARED)) {
              n->dedicated_tx_links_count++;
            }
          }
        }
      }
    }
  }
  return l;
}
#endif
/*---------------------------------------------------------------------------*/
/* Removes a link from slotframe. Return 1 if success, 0 if failure */
int
tsch_schedule_remove_link(struct tsch_slotframe *slotframe, struct tsch_link *l)
{
  if(slotframe != NULL && l != NULL && l->slotframe_handle == slotframe->handle) {
#if WITH_ALICE /* alice implementation */
#ifdef ALICE_TIME_VARYING_SCHEDULING
    if(1) { //original: if(tsch_get_lock()) {
#else
    if(tsch_get_lock()) {
#endif
#else
    if(tsch_get_lock()) {
#endif
      uint8_t link_options;
      linkaddr_t addr;

      /* Save link option and addr in local variables as we need them
       * after freeing the link */
      link_options = l->link_options;
      linkaddr_copy(&addr, &l->addr);

      /* The link to be removed is scheduled as next, set it to NULL
       * to abort the next link operation */
      if(l == current_link) {
        current_link = NULL;
      }

#if HCK_LOG_TSCH_LINK_ADD_REMOVE
      LOG_INFO("remove_link sf=%u opt=%s type=%s ts=%u ch=%u addr=",
               slotframe->handle,
               print_link_options(l->link_options),
               print_link_type(l->link_type), l->timeslot, l->channel_offset);
      LOG_INFO_LLADDR(&l->addr);
      LOG_INFO_("\n");
#endif /* HCK_LOG_TSCH_LINK_ADD_REMOVE */

      list_remove(slotframe->links_list, l);
      memb_free(&link_memb, l);

      /* Release the lock before we update the neighbor (will take the lock) */
      tsch_release_lock();

      /* This was a tx link to this neighbor, update counters */
      if(link_options & LINK_OPTION_TX) {
        struct tsch_neighbor *n = tsch_queue_get_nbr(&addr);
        if(n != NULL) {
          n->tx_links_count--;
          if(!(link_options & LINK_OPTION_SHARED)) {
            n->dedicated_tx_links_count--;
          }
        }
      }

      return 1;
    } else {
      LOG_ERR("! remove_link memb_alloc couldn't take lock\n");
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Removes a link from slotframe and timeslot. Return a 1 if success, 0 if failure */
int
tsch_schedule_remove_link_by_timeslot(struct tsch_slotframe *slotframe,
                                      uint16_t timeslot, uint16_t channel_offset)
{
  int ret = 0;
  if(!tsch_is_locked()) {
    if(slotframe != NULL) {
      struct tsch_link *l = list_head(slotframe->links_list);
      /* Loop over all items and remove all matching links */
      while(l != NULL) {
        struct tsch_link *next = list_item_next(l);
        if(l->timeslot == timeslot && l->channel_offset == channel_offset) {
          if(tsch_schedule_remove_link(slotframe, l)) {
            ret = 1;
          }
        }
        l = next;
      }
    }
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
/* Looks within a slotframe for a link with a given timeslot */
struct tsch_link *
tsch_schedule_get_link_by_timeslot(struct tsch_slotframe *slotframe,
                                   uint16_t timeslot, uint16_t channel_offset)
{
  if(!tsch_is_locked()) {
    if(slotframe != NULL) {
      struct tsch_link *l = list_head(slotframe->links_list);
      /* Loop over all items. Assume there is max one link per timeslot and channel_offset */
      while(l != NULL) {
        if(l->timeslot == timeslot && l->channel_offset == channel_offset) {
          return l;
        }
        l = list_item_next(l);
      }
      return l;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
static struct tsch_link *
default_tsch_link_comparator(struct tsch_link *a, struct tsch_link *b)
{
  if(!(a->link_options & LINK_OPTION_TX)) {
    /* None of the links are Tx: simply return the first link */
    return a;
  }

  /* Two Tx links at the same slotframe; return the one with most packets to send */
  if(!linkaddr_cmp(&a->addr, &b->addr)) {
    struct tsch_neighbor *an = tsch_queue_get_nbr(&a->addr);
    struct tsch_neighbor *bn = tsch_queue_get_nbr(&b->addr);
    int a_packet_count = an ? ringbufindex_elements(&an->tx_ringbuf) : 0;
    int b_packet_count = bn ? ringbufindex_elements(&bn->tx_ringbuf) : 0;
    /* Compare the number of packets in the queue */
    return a_packet_count >= b_packet_count ? a : b;
  }

  /* Same neighbor address; simply return the first link */
  return a;
}

/*---------------------------------------------------------------------------*/
/* Returns the next active link after a given ASN, and a backup link (for the same ASN, with Rx flag) */
struct tsch_link *
tsch_schedule_get_next_active_link(struct tsch_asn_t *asn, uint16_t *time_offset,
    struct tsch_link **backup_link)
{
#if WITH_ALICE
  /*
   * ALICE: First, derive 'alice_current_asfn', which is the ASFN at the start of the ongoing slot operation
   * from 'alice_current_asn', which is the ASN at the start of the ongoing slot operation.
   */
  uint64_t alice_current_asfn = 0;
  struct tsch_slotframe *alice_uc_sf = tsch_schedule_get_slotframe_by_handle(ALICE_UNICAST_SF_HANDLE);
  struct tsch_asn_t temp_asn;
  TSCH_ASN_COPY(temp_asn, alice_current_asn);
  uint16_t mod1 = TSCH_ASN_MOD(temp_asn, alice_uc_sf->size);
  TSCH_ASN_DEC(temp_asn, mod1);
  alice_current_asfn = TSCH_ASN_DIVISION(temp_asn, alice_uc_sf->size);
#endif

  uint16_t time_to_curr_best = 0;
  struct tsch_link *curr_best = NULL;
  struct tsch_link *curr_backup = NULL; /* Keep a back link in case the current link
  turns out useless when the time comes. For instance, for a Tx-only link, if there is
  no outgoing packet in queue. In that case, run the backup link instead. The backup link
  must have Rx flag set. */
  if(!tsch_is_locked()) {
    struct tsch_slotframe *sf = list_head(slotframe_list);
    /* For each slotframe, look for the earliest occurring link */
    while(sf != NULL) {

#if WITH_ALICE /* alice implementation */
#ifdef ALICE_TIME_VARYING_SCHEDULING
      if(sf->handle == ALICE_UNICAST_SF_HANDLE) {
        /*
         * Second, check whether any remaining unicast slotframe link exists after 'alice_current_asn'
         * within the slotframe of 'alice_current_asfn'.
         */
        int alice_remaining_uicast_sf_link_exists = 0;

        uint16_t timeslot = TSCH_ASN_MOD(alice_current_asn, sf->size);
        struct tsch_link *l = list_head(sf->links_list);
        while(l != NULL) {
          /* Check if any link exists after this timeslot in the current unicast slotframe */
          if(l->timeslot > timeslot) {
            /* In the current unicast slotframe schedule, the current timeslot is not a last one. */
            alice_remaining_uicast_sf_link_exists = 1;
            break;
          }
          l = list_item_next(l);
        }

        int alice_remaining_uicast_sf_link_exists_at_tsch_current_asn = 0;

        timeslot = TSCH_ASN_MOD(*asn, sf->size);
        l = list_head(sf->links_list);
        while(l != NULL) {
          /* Check if any link exists after this timeslot in the current unicast slotframe */
          if(l->timeslot > timeslot) {
            /* In the current unicast slotframe schedule, the current timeslot is not a last one. */
            alice_remaining_uicast_sf_link_exists_at_tsch_current_asn = 1;
            break;
          }
          l = list_item_next(l);
        }

        uint64_t alice_current_asfn_at_tsch_current_asn = 0;
        struct tsch_asn_t temp_asn_at_tsch_current_asn;
        TSCH_ASN_COPY(temp_asn_at_tsch_current_asn, *asn);
        uint16_t mod1_at_tsch_current_asn = TSCH_ASN_MOD(temp_asn_at_tsch_current_asn, sf->size);
        TSCH_ASN_DEC(temp_asn_at_tsch_current_asn, mod1_at_tsch_current_asn);
        alice_current_asfn_at_tsch_current_asn = TSCH_ASN_DIVISION(temp_asn_at_tsch_current_asn, sf->size);

        /*
         * Third, if 'alice_remaining_uicast_sf_link_exists' is zero,
         * there is no more unicast slotframe link within the current ASFN (alice_current_asfn).
         * Then, re-schedule unicast slotframe for the next ASFN,
         * only when lastly scheduled ASFN (alice_lastly_scheduled_asfn) is different from
         * the next ASFN (alice_next_asfn).
         */
        if(alice_remaining_uicast_sf_link_exists == 0 
          || alice_remaining_uicast_sf_link_exists_at_tsch_current_asn == 0
          || alice_current_asfn_at_tsch_current_asn != alice_current_asfn) {
          uint64_t alice_next_asfn = 0;
          struct tsch_asn_t asn_of_next_asfn;
          TSCH_ASN_COPY(asn_of_next_asfn, alice_current_asn);
          TSCH_ASN_INC(asn_of_next_asfn, sf->size.val);
          uint16_t mod2 = TSCH_ASN_MOD(asn_of_next_asfn, sf->size);
          TSCH_ASN_DEC(asn_of_next_asfn, mod2);
          alice_next_asfn = TSCH_ASN_DIVISION(asn_of_next_asfn, sf->size);

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
          uint64_t alice_after_next_asfn = 0;
          struct tsch_asn_t asn_of_after_next_asfn;
          TSCH_ASN_COPY(asn_of_after_next_asfn, alice_current_asn);
          TSCH_ASN_INC(asn_of_after_next_asfn, sf->size.val);
          TSCH_ASN_INC(asn_of_after_next_asfn, sf->size.val);
          uint16_t mod3 = TSCH_ASN_MOD(asn_of_after_next_asfn, sf->size);
          TSCH_ASN_DEC(asn_of_after_next_asfn, mod3);
          alice_after_next_asfn = TSCH_ASN_DIVISION(asn_of_after_next_asfn, sf->size);
#endif

          if(alice_next_asfn != alice_lastly_scheduled_asfn) {
            alice_lastly_scheduled_asfn = alice_next_asfn;
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
            alice_next_asfn_of_lastly_scheduled_asfn = alice_after_next_asfn;
#endif

#if WITH_A3
            double alpha = 0.02 + ((double)ORCHESTRA_CONF_UNICAST_PERIOD * (double)0.001);
            if(alpha > 0.15) {
              alpha = 0.15;
            }
            double alphaPlus = alpha * 0.2;
            double dynamicAlpha = 0;

            static double txIncreaseThresh = A3_TX_INCREASE_THRESH;
            static double txDecreaseThresh = A3_TX_DECREASE_THRESH;
            static double rxIncreaseThresh = A3_RX_INCREASE_THRESH;
            static double rxDecreaseThresh = A3_RX_DECREASE_THRESH;

            static double maxErr = A3_MAX_ERR_PROB;

            uint8_t sumTx = 0;
            uint8_t sumRx = 0;
            uint8_t diffRx = 0;
            double newVal = 0;

            /* Calculate EWMA of 'Tx attempt rate' toward parent */
            sumTx = a3_p_num_tx_pkt_success + a3_p_num_tx_pkt_collision;

            newVal = (double)(sumTx) / (double)(a3_p_num_tx_slot);
            if(newVal > 1) {
              newVal = 1;
            }
            a3_p_tx_attempt_rate_ewma = (1 - alpha) * a3_p_tx_attempt_rate_ewma + alpha * newVal;

#if A3_DBG_VALUE
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_t_a %u | %u %u %u", 
                        HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                        a3_p_num_tx_pkt_success, a3_p_num_tx_pkt_collision, a3_p_num_tx_slot);
            );

            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_t_a alpha %.3f", alpha);
            );
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_t_a newVal %.3f", newVal);
            );
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_t_a ewma %.3f", a3_p_tx_attempt_rate_ewma);
            );
#endif

            /* Calculate EWMA of 'Tx success rate' toward parent */
            newVal = (double)(a3_p_num_tx_pkt_success) / (double)(a3_p_num_tx_slot);
            if(newVal > 1) {
              newVal = 1;
            }
            a3_p_tx_success_rate_ewma = (1 - alpha) * a3_p_tx_success_rate_ewma + alpha * newVal;

#if A3_DBG_VALUE
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_t_s newVal %.3f", newVal);
            );
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_t_s ewma %.3f", a3_p_tx_success_rate_ewma);
            );
#endif

            /* Calculate EWMA of 'Rx attempt rate' from parent */
            sumRx = a3_p_num_rx_pkt_success 
                  + a3_p_num_rx_pkt_collision 
                  + a3_p_num_rx_pkt_idle 
                  + a3_p_num_rx_pkt_others;
            diffRx = (a3_p_num_rx_slot - sumRx); //difference
            if(a3_p_num_rx_slot < sumRx) { //minus value
              diffRx = 0;
            }
            a3_p_num_rx_pkt_unscheduled = diffRx;
            sumRx += diffRx;

            newVal = ((double)a3_p_num_rx_pkt_success 
                      + a3_p_rx_attempt_rate_ewma * (double)(a3_p_num_rx_pkt_collision + a3_p_num_rx_pkt_unscheduled)) 
                        / (double)(sumRx);
            if(newVal > 1) {
              newVal = 1;
            }

            dynamicAlpha = alpha;
            if(newVal > a3_p_rx_attempt_rate_ewma) {
              dynamicAlpha += alphaPlus;
            }
            a3_p_rx_attempt_rate_ewma = (1 - dynamicAlpha) * a3_p_rx_attempt_rate_ewma + dynamicAlpha * newVal;

#if A3_DBG_VALUE
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_r_a %u | %u %u %u %u %u | %u", 
                        HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                        a3_p_num_rx_pkt_success, a3_p_num_rx_pkt_collision, 
                        a3_p_num_rx_pkt_idle, a3_p_num_rx_pkt_others, 
                        a3_p_num_rx_pkt_unscheduled, sumRx);
            );
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_r_a dalpha %.3f", dynamicAlpha);
            );
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_r_a newVal %.3f", newVal);
            );
            TSCH_LOG_ADD(tsch_log_message,
                    snprintf(log->message, sizeof(log->message),
                        "A3-v p_r_a ewma %.3f", a3_p_rx_attempt_rate_ewma);
            );
#endif

            int txChangedFlag = 0;

#if A3_ALICE1_ORB2_OSB3 != 2 //O-SB, ALICE
            if((a3_p_tx_attempt_rate_ewma - a3_p_tx_success_rate_ewma) / (a3_p_tx_attempt_rate_ewma) > maxErr 
                && a3_p_num_tx_slot > 1) {
#if A3_DBG
              TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "A3-a p_t_half_c %u | %u", 
                          HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                          a3_p_num_tx_slot);
              );
#endif
              a3_p_num_tx_slot /= 2;
              txChangedFlag = 1;

              a3_p_tx_attempt_rate_ewma = A3_INITIAL_TX_ATTEMPT_RATE_EWMA;
              a3_p_tx_success_rate_ewma = A3_INITIAL_TX_SUCCESS_RATE_EWMA;
            }
#endif

            if(txChangedFlag == 0) {
              if(a3_p_tx_attempt_rate_ewma > txIncreaseThresh && a3_p_num_tx_slot < A3_MAX_ZONE) {
#if A3_DBG
                TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "A3-a p_t_double %u | %u", 
                            HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                            a3_p_num_tx_slot);
                );
#endif
                a3_p_num_tx_slot *= 2;
                a3_p_tx_attempt_rate_ewma /= 2;
              } else if(a3_p_tx_attempt_rate_ewma < txDecreaseThresh && a3_p_num_tx_slot > 1) {
#if A3_DBG
                TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "A3-a p_t_half %u | %u", 
                            HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                            a3_p_num_tx_slot);
                );
#endif
                a3_p_num_tx_slot /= 2;
                a3_p_tx_attempt_rate_ewma *= 2;
              }
            }

            if(a3_p_rx_attempt_rate_ewma > rxIncreaseThresh && a3_p_num_rx_slot < A3_MAX_ZONE) {
#if A3_DBG
              TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "A3-a p_r_double %u | %u", 
                          HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                          a3_p_num_rx_slot);
              );
#endif
              a3_p_num_rx_slot *= 2;
              a3_p_rx_attempt_rate_ewma /= 2;
            } else if(a3_p_rx_attempt_rate_ewma < rxDecreaseThresh && a3_p_num_rx_slot > 1) {
#if A3_DBG
              TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "A3-a p_r_half %u | %u", 
                          HCK_GET_NODE_ID_FROM_LINKADDR(tsch_queue_get_nbr_address(tsch_queue_get_time_source())),
                          a3_p_num_rx_slot);
              );
#endif
              a3_p_num_rx_slot /= 2;
              a3_p_rx_attempt_rate_ewma *= 2;
            }

            a3_p_num_tx_pkt_success = A3_INITIAL_NUM_OF_PKTS;
            a3_p_num_tx_pkt_collision = A3_INITIAL_NUM_OF_PKTS;

            a3_p_num_rx_pkt_success = A3_INITIAL_NUM_OF_PKTS;
            a3_p_num_rx_pkt_collision = A3_INITIAL_NUM_OF_PKTS;
            a3_p_num_rx_pkt_idle = A3_INITIAL_NUM_OF_PKTS;
            a3_p_num_rx_pkt_unscheduled = A3_INITIAL_NUM_OF_PKTS;
            a3_p_num_rx_pkt_others = A3_INITIAL_NUM_OF_PKTS;

            /* Consider child nodes */
            nbr_table_item_t *item = nbr_table_head(nbr_routes);
            while(item != NULL) {
              linkaddr_t *addr = nbr_table_get_lladdr(nbr_routes, item);
              if(addr != NULL) {
                uip_ds6_nbr_t *it = uip_ds6_nbr_ll_lookup((uip_lladdr_t *)addr);
                if(it != NULL) {
                  sumTx = it->a3_c_num_tx_pkt_success + it->a3_c_num_tx_pkt_collision;

                  newVal = (double)(sumTx) / (double)(it->a3_c_num_tx_slot);
                  if(newVal > 1) {
                    newVal = 1;
                  }
                  it->a3_c_tx_attempt_rate_ewma = (1 - alpha) * it->a3_c_tx_attempt_rate_ewma + alpha * newVal;

#if A3_DBG_VALUE
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_t_a %u | %u %u %u", 
                              HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                              it->a3_c_num_tx_pkt_success, it->a3_c_num_tx_pkt_collision, it->a3_c_num_tx_slot);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_t_a alpha %.3f", alpha);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_t_a newVal %.3f", newVal);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_t_a ewma %.3f", it->a3_c_tx_attempt_rate_ewma);
                  );
#endif

                  newVal = (double)(it->a3_c_num_tx_pkt_success) / (double)(it->a3_c_num_tx_slot);
                  if(newVal > 1) {
                    newVal = 1;
                  }
                  it->a3_c_tx_success_rate_ewma = (1 - alpha) * it->a3_c_tx_success_rate_ewma + alpha * newVal;

#if A3_DBG_VALUE
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_t_s newVal %.3f", newVal);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_t_s ewma %.3f", it->a3_c_tx_success_rate_ewma);
                  );
#endif

                  sumRx = it->a3_c_num_rx_pkt_success 
                        + it->a3_c_num_rx_pkt_collision  
                        + it->a3_c_num_rx_pkt_idle 
                        + it->a3_c_num_rx_pkt_others;
                  diffRx = (it->a3_c_num_rx_slot - sumRx); // differance
                  if(it->a3_c_num_rx_slot < sumRx) {
                    diffRx = 0;
                  }
                  it->a3_c_num_rx_pkt_unscheduled = diffRx;
                  sumRx += diffRx;

                  newVal = ((double)it->a3_c_num_rx_pkt_success 
                            + it->a3_c_rx_attempt_rate_ewma * (double)(it->a3_c_num_rx_pkt_collision + it->a3_c_num_rx_pkt_unscheduled))
                              / (double)(sumRx);
                  if(newVal > 1) {
                    newVal = 1;
                  }

                  dynamicAlpha = alpha;
                  if(newVal > it->a3_c_rx_attempt_rate_ewma) {
                    dynamicAlpha += alphaPlus;
                  }
                  it->a3_c_rx_attempt_rate_ewma = (1 - dynamicAlpha) * it->a3_c_rx_attempt_rate_ewma + dynamicAlpha * newVal;

#if A3_DBG_VALUE
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_r_a %u | %u %u %u %u %u | %u", 
                              HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                              it->a3_c_num_rx_pkt_success, it->a3_c_num_rx_pkt_collision, 
                              it->a3_c_num_rx_pkt_idle, it->a3_c_num_rx_pkt_others, 
                              it->a3_c_num_rx_pkt_unscheduled, sumRx);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_r_a dalpha %.3f", dynamicAlpha);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_r_a newVal %.3f", newVal);
                  );
                  TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "A3-v c_r_a ewma %.3f", it->a3_c_rx_attempt_rate_ewma);
                  );
#endif

                  int txChangedForChildFlag = 0;

#if A3_ALICE1_ORB2_OSB3 != 2 //O-SB, ALICE
                  if((it->a3_c_tx_attempt_rate_ewma - it->a3_c_tx_success_rate_ewma) / (it->a3_c_tx_attempt_rate_ewma) > maxErr 
                      && it->a3_c_num_tx_slot > 1) {

#if A3_DBG
                    TSCH_LOG_ADD(tsch_log_message,
                            snprintf(log->message, sizeof(log->message),
                                "A3-a c_t_half_c %u | %u", 
                                HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                it->a3_c_num_tx_slot);
                    );
#endif

                    it->a3_c_num_tx_slot /= 2;
                    txChangedForChildFlag = 1;

                    it->a3_c_tx_attempt_rate_ewma = A3_INITIAL_TX_ATTEMPT_RATE_EWMA;
                    it->a3_c_tx_success_rate_ewma = A3_INITIAL_TX_SUCCESS_RATE_EWMA;
                  }
#endif

                  if(txChangedForChildFlag == 0) {
                    if(it->a3_c_tx_attempt_rate_ewma > txIncreaseThresh && it->a3_c_num_tx_slot < A3_MAX_ZONE) {

#if A3_DBG
                      TSCH_LOG_ADD(tsch_log_message,
                              snprintf(log->message, sizeof(log->message),
                                  "A3-a c_t_double %u | %u", 
                                  HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                  it->a3_c_num_tx_slot);
                      );
#endif

                      it->a3_c_num_tx_slot *= 2;
                      it->a3_c_tx_attempt_rate_ewma /= 2;
                    } else if(it->a3_c_tx_attempt_rate_ewma < txDecreaseThresh && it->a3_c_num_tx_slot > 1) {

#if A3_DBG
                      TSCH_LOG_ADD(tsch_log_message,
                              snprintf(log->message, sizeof(log->message),
                                  "A3-a c_t_half %u | %u", 
                                  HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                  it->a3_c_num_tx_slot);
                      );
#endif

                      it->a3_c_num_tx_slot /= 2;
                      it->a3_c_tx_attempt_rate_ewma *= 2;
                    }
                  }

                  if(it->a3_c_rx_attempt_rate_ewma > rxIncreaseThresh && it->a3_c_num_rx_slot < A3_MAX_ZONE) {

#if A3_DBG
                      TSCH_LOG_ADD(tsch_log_message,
                              snprintf(log->message, sizeof(log->message),
                                  "A3-a c_r_double %u | %u", 
                                  HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                  it->a3_c_num_rx_slot);
                      );
#endif

                    it->a3_c_num_rx_slot *= 2;
                    it->a3_c_rx_attempt_rate_ewma /= 2;
                  } else if(it->a3_c_rx_attempt_rate_ewma < rxDecreaseThresh && it->a3_c_num_rx_slot > 1) {

#if A3_DBG
                      TSCH_LOG_ADD(tsch_log_message,
                              snprintf(log->message, sizeof(log->message),
                                  "A3-a c_r_half %u | %u", 
                                  HCK_GET_NODE_ID_FROM_LINKADDR(addr),
                                  it->a3_c_num_rx_slot);
                      );
#endif

                    it->a3_c_num_rx_slot /= 2;
                    it->a3_c_rx_attempt_rate_ewma *= 2;
                  }

                  it->a3_c_num_tx_pkt_success = A3_INITIAL_NUM_OF_PKTS;
                  it->a3_c_num_tx_pkt_collision = A3_INITIAL_NUM_OF_PKTS;

                  it->a3_c_num_rx_pkt_success = A3_INITIAL_NUM_OF_PKTS;
                  it->a3_c_num_rx_pkt_collision = A3_INITIAL_NUM_OF_PKTS;
                  it->a3_c_num_rx_pkt_idle = A3_INITIAL_NUM_OF_PKTS;
                  it->a3_c_num_rx_pkt_others = A3_INITIAL_NUM_OF_PKTS;
                  it->a3_c_num_rx_pkt_unscheduled = A3_INITIAL_NUM_OF_PKTS;
                }
              }

              item = nbr_table_next(nbr_routes, item);
            }
#endif /* WITH_A3 */

            ALICE_TIME_VARYING_SCHEDULING();
          }
        }
      }

#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
      else if(sf->handle == ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE) {
        sf = list_item_next(sf);
        continue;
      }
#endif
#endif
#endif

      /* Get timeslot from ASN, given the slotframe length */
      uint16_t timeslot = TSCH_ASN_MOD(*asn, sf->size);
      struct tsch_link *l = list_head(sf->links_list);
      while(l != NULL) {
        uint16_t time_to_timeslot =
          l->timeslot > timeslot ?
          l->timeslot - timeslot :
          sf->size.val + l->timeslot - timeslot;

#if WITH_ALICE
#ifdef ALICE_TIME_VARYING_SCHEDULING
        /* 
         * ALICE: To prevent the link of the following ASFN from being executed within the current ASFN,
         * add 'sf->size.val' to the links of the next ASFN and located after the current timeslot.
         */
        if(sf->handle == ALICE_UNICAST_SF_HANDLE 
            && alice_current_asfn != alice_lastly_scheduled_asfn /* alice final check */
            && l->timeslot > timeslot) {
          time_to_timeslot += sf->size.val;
        }
#endif
#endif

        if(curr_best == NULL || time_to_timeslot < time_to_curr_best) {
          time_to_curr_best = time_to_timeslot;
          curr_best = l;
          curr_backup = NULL;
        } else if(time_to_timeslot == time_to_curr_best) {
          struct tsch_link *new_best = NULL;
          /* Two links are overlapping, we need to select one of them.
           * By standard: prioritize Tx links first, second by lowest handle */
          if((curr_best->link_options & LINK_OPTION_TX) == (l->link_options & LINK_OPTION_TX)) {
            /* Both or neither links have Tx, select the one with lowest handle */
            if(l->slotframe_handle != curr_best->slotframe_handle) {
              if(l->slotframe_handle < curr_best->slotframe_handle) {
                new_best = l;
              }
            } else {
              /* compare the link against the current best link and return the newly selected one */
              new_best = TSCH_LINK_COMPARATOR(curr_best, l);
            }

#if WITH_OST
            if((curr_best->slotframe_handle == 1) 
              && (curr_best->link_options & LINK_OPTION_TX) 
              && (l->slotframe_handle == 2)) {
              /*
              Prevent Autonomous unicast Tx from interfering autonomous broadcast Tx/Rx
              They share the same c_offset in OST
              Prioritize Autonomous broadcast Tx/Rx to Autonomous unicast Tx
              */
              new_best = l;
            }
#endif

          } else {
            /* Select the link that has the Tx option */
            if(l->link_options & LINK_OPTION_TX) {
              new_best = l;
            }
          }

#if HCK_MOD_TSCH_APPLY_LATEST_CONTIKI
          /* Maintain backup_link */
          /* Check if 'l' best can be used as backup */
          if(new_best != l && (l->link_options & LINK_OPTION_RX)) { /* Does 'l' have Rx flag? */
            if(curr_backup == NULL || l->slotframe_handle < curr_backup->slotframe_handle) {
              curr_backup = l;
            }
          }
          /* Check if curr_best can be used as backup */
          if(new_best != curr_best && (curr_best->link_options & LINK_OPTION_RX)) { /* Does curr_best have Rx flag? */
            if(curr_backup == NULL || curr_best->slotframe_handle < curr_backup->slotframe_handle) {
              curr_backup = curr_best;
            }
          }
#else
          /* Maintain backup_link */
          if(curr_backup == NULL) {
            /* Check if 'l' best can be used as backup */
            if(new_best != l && (l->link_options & LINK_OPTION_RX)) { /* Does 'l' have Rx flag? */
              curr_backup = l;
            }
            /* Check if curr_best can be used as backup */
            if(new_best != curr_best && (curr_best->link_options & LINK_OPTION_RX)) { /* Does curr_best have Rx flag? */
              curr_backup = curr_best;
            }
          }
#endif

          /* Maintain curr_best */
          if(new_best != NULL) {
            curr_best = new_best;
          }
        }

        l = list_item_next(l);
      }
      sf = list_item_next(sf);
    }
    if(time_offset != NULL) {
      *time_offset = time_to_curr_best;
    }

#if WITH_OST && OST_ON_DEMAND_PROVISION
    struct tsch_link *ssq_link = NULL;
    uint16_t new_time_offset = *time_offset; /* Initialize */
    if(ost_earlier_ssq_schedule_list(&new_time_offset, &ssq_link)) {
      if(ssq_link != NULL) {
        /* Changed time_offset to the new_time_offset */
        *time_offset = new_time_offset;
        *backup_link = NULL;
        return ssq_link;
      } else {
        /* ERROR: ssq_link is NULL */
      }
    }
#endif

  }
  if(backup_link != NULL) {
    *backup_link = curr_backup;
  }
  return curr_best;
}
/*---------------------------------------------------------------------------*/
/* Module initialization, call only once at startup. Returns 1 is success, 0 if failure. */
int
tsch_schedule_init(void)
{
  if(tsch_get_lock()) {
    memb_init(&link_memb);
    memb_init(&slotframe_memb);
    list_init(slotframe_list);
    tsch_release_lock();
    return 1;
  } else {
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
/* Create a 6TiSCH minimal schedule */
void
tsch_schedule_create_minimal(void)
{
  struct tsch_slotframe *sf_min;
  /* First, empty current schedule */
  tsch_schedule_remove_all_slotframes();
  /* Build 6TiSCH minimal schedule.
   * We pick a slotframe length of TSCH_SCHEDULE_DEFAULT_LENGTH */
  sf_min = tsch_schedule_add_slotframe(0, TSCH_SCHEDULE_DEFAULT_LENGTH);
  /* Add a single Tx|Rx|Shared slot using broadcast address (i.e. usable for unicast and broadcast).
   * We set the link type to advertising, which is not compliant with 6TiSCH minimal schedule
   * but is required according to 802.15.4e if also used for EB transmission.
   * Timeslot: 0, channel offset: 0. */
  tsch_schedule_add_link(sf_min,
      (LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING),
      LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
      0, 0, 1);
}
/*---------------------------------------------------------------------------*/
struct tsch_slotframe *
tsch_schedule_slotframe_head(void)
{
  return list_head(slotframe_list);
}
/*---------------------------------------------------------------------------*/
struct tsch_slotframe *
tsch_schedule_slotframe_next(struct tsch_slotframe *sf)
{
  return list_item_next(sf);
}
/*---------------------------------------------------------------------------*/
/* Prints out the current schedule (all slotframes and links) */
void
tsch_schedule_print(void)
{
  if(!tsch_is_locked()) {
    struct tsch_slotframe *sf = list_head(slotframe_list);

    LOG_PRINT("----- start slotframe list -----\n");

    while(sf != NULL) {
      struct tsch_link *l = list_head(sf->links_list);

      LOG_PRINT("Slotframe Handle %u, size %u\n", sf->handle, sf->size.val);

      while(l != NULL) {
        LOG_PRINT("* Link Options %02x, type %u, timeslot %u, channel offset %u, address %u\n",
               l->link_options, l->link_type, l->timeslot, l->channel_offset, l->addr.u8[7]);
        l = list_item_next(l);
      }

      sf = list_item_next(sf);
    }

    LOG_PRINT("----- end slotframe list -----\n");
  }
}
/*---------------------------------------------------------------------------*/
/* Prints out the current schedule (all slotframes and links) */
#if WITH_OST
void
tsch_schedule_print_ost(void)
{
  if(!tsch_is_locked()) {
    struct tsch_slotframe *sf = list_head(slotframe_list);

    printf("[SLOTFRAMES] Opt / Size / Timeslot\n");

    while(sf != NULL) {
      if(sf->handle > 2) {
        if(sf->handle % 2 == 0) {
          printf("[ID:%u] Rx / %u / ", sf->handle / 2 - 1, sf->size.val);
        } else {
          printf("[ID:%u] Tx / %u / ", sf->handle / 2, sf->size.val);
        }

        struct tsch_link *l = list_head(sf->links_list);

        printf("[Slotframe] Handle %u, size %u\n", sf->handle, sf->size.val);
        printf("List of links:\n");

        while(l != NULL) {
          printf("%u\n", l->timeslot);
          l = list_item_next(l);
        }
      }
      sf = list_item_next(sf);
    }
    printf("\n");
  }
}
/*---------------------------------------------------------------------------*/
struct tsch_slotframe *
ost_tsch_schedule_get_slotframe_head(void)
{
  struct tsch_slotframe *sf = list_head(slotframe_list);
  return sf;
}
#endif
/*---------------------------------------------------------------------------*/
/** @} */
