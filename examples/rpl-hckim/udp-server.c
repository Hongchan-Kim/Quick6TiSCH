/*
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

#include "contiki.h"
#include "net/routing/routing.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

/* to reset log */
#include "net/ipv6/tcpip.h"
#include "net/ipv6/sicslowpan.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/rpl-classic/rpl.h"
#include "services/simple-energest/simple-energest.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "node-info.h"

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#ifndef APP_START_DELAY
#define APP_START_DELAY     (1 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_PRINT_DELAY
#define APP_PRINT_DELAY     (1 * 30 * CLOCK_SECOND)
#endif

#ifndef APP_DOWNWARD_SEND_INTERVAL
#define APP_DOWNWARD_SEND_INTERVAL   (1 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_DOWNWARD_MAX_TX
#define APP_DOWNWARD_MAX_TX          100
#endif

static struct simple_udp_connection udp_conn;

#if DOWNWARD_TRAFFIC
#define APP_DOWN_INTERVAL   (APP_DOWNWARD_SEND_INTERVAL / (NON_ROOT_NUM + 1))
#endif

#if WITH_OST && OST_HANDLE_QUEUED_PACKETS
extern uint8_t bootstrap_period;
#endif

/*---------------------------------------------------------------------------*/
#if APP_SEQNO_DUPLICATE_CHECK
struct app_up_seqno {
  clock_time_t app_up_timestamp;
  uint8_t app_up_seqno;
};
struct app_up_seqnos_from_sender {
  struct app_up_seqno app_up_seqno_array[APP_SEQNO_HISTORY];
};

static struct app_up_seqnos_from_sender app_up_received_seqnos[NODE_NUM];

/*---------------------------------------------------------------------------*/
int
app_up_sequence_is_duplicate(uint16_t sender_id, uint16_t current_app_up_seqno)
{
  int i;
  clock_time_t now = clock_time();

  /*
   * Check for duplicate packet by comparing the sequence number of the incoming
   * packet with the last few ones we saw.
   */
  uint16_t sender_index = sender_id - 1;

  for(i = 0; i < APP_SEQNO_HISTORY; ++i) {
    if(current_app_up_seqno == app_up_received_seqnos[sender_index].app_up_seqno_array[i].app_up_seqno) {
#if APP_SEQNO_MAX_AGE > 0
      if(now - app_up_received_seqnos[sender_index].app_up_seqno_array[i].app_up_timestamp <= APP_SEQNO_MAX_AGE) {
        /* Duplicate packet. */
        return 1;
      }
      break;
#else /* APP_SEQNO_MAX_AGE > 0 */
      return 1;
#endif /* APP_SEQNO_MAX_AGE > 0 */
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
app_up_sequence_register_seqno(uint16_t sender_id, uint16_t current_app_up_seqno)
{
  int i, j;

  uint16_t sender_index = sender_id - 1;

  /* Locate possible previous sequence number for this address. */
  for(i = 0; i < APP_SEQNO_HISTORY; ++i) {
    if(current_app_up_seqno == app_up_received_seqnos[sender_index].app_up_seqno_array[i].app_up_seqno) {
      i++;
      break;
    }
  }


  /* Keep the last sequence number for each address as per 802.15.4e. */
  for(j = i - 1; j > 0; --j) {
    memcpy(&app_up_received_seqnos[sender_index].app_up_seqno_array[j], &app_up_received_seqnos[sender_index].app_up_seqno_array[j - 1], sizeof(struct app_up_seqno));
  }
  app_up_received_seqnos[sender_index].app_up_seqno_array[0].app_up_seqno = current_app_up_seqno;
  app_up_received_seqnos[sender_index].app_up_seqno_array[0].app_up_timestamp = clock_time();
}
#endif /* APP_SEQNO_DUPLICATE_CHECK */
/*---------------------------------------------------------------------------*/

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
#if WITH_COOJA
static void
reset_cooja_nodes()
{
  uint8_t i = 0;
  for(i = 0; i < NODE_NUM; i++) {
    cooja_nodes[i] = 0;
  }
}
#endif
/*---------------------------------------------------------------------------*/
static void
reset_log()
{
  LOG_INFO("HCK reset_log |\n");
  reset_log_tcpip();              /* tcpip.c */
  reset_log_sicslowpan();         /* sicslowpan.c */
  reset_log_tsch();               /* tsch.c */
  reset_log_rpl_icmp6();          /* rpl-icmp6.c */
  reset_log_rpl_dag();            /* rpl-dag.c */
  reset_log_rpl_timers();         /* rpl-timers.c */
  reset_log_rpl();                /* rpl.c */
  reset_log_rpl_ext_header();     /* rpl-ext-header.c */
  reset_log_simple_energest();    /* simple-energest.c */
}
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  uint16_t sender_index = ((sender_addr->u8[14]) << 8) + sender_addr->u8[15] - 1;
  if(sender_index == NODE_NUM) {
    LOG_INFO("Fail to receive: out of index\n");
    return;
  }
#if WITH_COOJA
  LOG_INFO("HCK rx_up %u from %u | Received message '%.*s' from ", 
            ++cooja_nodes[sender_index], sender_index + 1, datalen, (char *) data);
#elif WITH_IOTLAB
#if WITH_APP_DATA_FOOTER
  uint32_t app_received_seqno = 0;
  memcpy(&app_received_seqno, data + datalen - 6, 4);
  app_received_seqno = UIP_HTONL(app_received_seqno);
  
  LOG_INFO("HCK rx_up %u from %u %x (%u %x) | Received message of %lx with len %u from ", 
            ++iotlab_nodes[sender_index][2],
            sender_index + 1, sender_index + 1, 
            iotlab_nodes[sender_index][0], iotlab_nodes[sender_index][1],
            app_received_seqno,
            datalen);
#else
  LOG_INFO("HCK rx_up %u from %u %x (%u %x) | Received message '%.*s' from ", 
            ++iotlab_nodes[sender_index][2],
            sender_index + 1, sender_index + 1, 
            iotlab_nodes[sender_index][0], iotlab_nodes[sender_index][1],
            datalen, (char *) data);
#endif
#endif
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  static struct etimer print_timer;
  static struct etimer reset_log_timer;

#if DOWNWARD_TRAFFIC
  static struct etimer start_timer;
  static struct etimer periodic_timer;
  static struct etimer send_timer;
  static unsigned count = 1;
#if WITH_COOJA
  static unsigned dest_id = COOJA_ROOT_ID + 1;
#elif WITH_IOTLAB
  static unsigned dest_id = IOTLAB_ROOT_ID + 1;
#endif
  static char str[32];
  uip_ipaddr_t dest_ipaddr;

#if WITH_VARYING_PPM
  static int APP_DOWNWARD_SEND_VARYING_PPM[VARY_LENGTH] = {1, 8, 2, 4, 6, 8, 4, 1};
  static int APP_DOWNWARD_SEND_VARYING_INTERVAL[VARY_LENGTH];
  static int APP_DOWN_VARYING_INTERVAL[VARY_LENGTH];
  static int APP_DOWNWARD_VARYING_MAX_TX[VARY_LENGTH];

  for(int k = 0; k < VARY_LENGTH; k++) {
    APP_DOWNWARD_SEND_VARYING_INTERVAL[k] = (1 * 60 * CLOCK_SECOND / APP_DOWNWARD_SEND_VARYING_PPM[k]);
    APP_DOWNWARD_VARYING_MAX_TX[k] = (APP_DATA_PERIOD / VARY_LENGTH) / APP_DOWNWARD_SEND_VARYING_INTERVAL[k]);
    APP_DOWN_VARYING_INTERVAL[k] = APP_DOWNWARD_SEND_VARYING_INTERVAL[k] / (NON_ROOT_NUM + 1);
  }

  static unsigned varycount = 1;
  static int index = 0;
#endif
#endif

  PROCESS_BEGIN();

#if WITH_COOJA
  reset_cooja_nodes();
#endif

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

#if DOWNWARD_TRAFFIC
#if WITH_VARYING_PPM
  etimer_set(&start_timer, (APP_START_DELAY + random_rand() % (APP_DOWNWARD_SEND_VARYING_INTERVAL[0] / 2)));
#else
  etimer_set(&start_timer, (APP_START_DELAY + random_rand() % (APP_DOWNWARD_SEND_INTERVAL / 2)));
#endif
#endif

  etimer_set(&print_timer, APP_PRINT_DELAY);
  etimer_set(&reset_log_timer, APP_START_DELAY);
#if WITH_OST && OST_HANDLE_QUEUED_PACKETS
  bootstrap_period = 1;
#endif

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_TIMER);
    if(data == &print_timer) {
#if WITH_IOTLAB
      print_iotlab_node_info();
#endif
    } else if(data == &reset_log_timer) {
#if WITH_OST && OST_HANDLE_QUEUED_PACKETS
      bootstrap_period = 0;
#endif
      reset_log();
    }
#if DOWNWARD_TRAFFIC
    else if(data == &start_timer || data == &periodic_timer) {
#if WITH_VARYING_PPM
      etimer_set(&send_timer, APP_DOWN_VARYING_INTERVAL[index]);
      etimer_set(&periodic_timer, APP_DOWNWARD_SEND_VARYING_INTERVAL[index]);
#else
      etimer_set(&send_timer, APP_DOWN_INTERVAL);
      etimer_set(&periodic_timer, APP_DOWNWARD_SEND_INTERVAL);
#endif
    }
    else if(data == &send_timer) {
#if WITH_VARYING_PPM
      if(varycount <= APP_DOWNWARD_VARYING_MAX_TX[index]) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, dest_id);
        /* Send to clients */
        uint16_t dest_index = dest_id - 1;
#if WITH_COOJA
        LOG_INFO("HCK tx_down %u to %u | Sending message %u to ", 
                  count, dest_id, count);
#elif WITH_IOTLAB
        LOG_INFO("HCK tx_down %u to %u %x (%u %x) | Sending message %u to ", 
                  count, dest_id, dest_id,
                  iotlab_nodes[dest_index][0], iotlab_nodes[dest_index][1],
                  count);
#endif
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");
        snprintf(str, sizeof(str), "hello %d", count);
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);

        dest_id++;
        if(dest_id > NODE_NUM) { /* the last non-root node */
          dest_id = 2;
          varycount++;
          count++;
        } else { /* not the last non-root node yet */
          etimer_reset(&send_timer);
        }
      }
      else if (index < VARY_LENGTH - 1){
        index++;
        varycount = 1;
      }
#else /* WITH_VARYING_PPM */
      if(count <= APP_DOWNWARD_MAX_TX) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, dest_id);
        /* Send to clients */
        uint16_t dest_index = dest_id - 1;
#if WITH_COOJA
        LOG_INFO("HCK tx_down %u to %u | Sending message %u to ", 
                  count, dest_id, count);
#elif WITH_IOTLAB
        LOG_INFO("HCK tx_down %u to %u %x (%u %x) | Sending message %u to ", 
                  count, dest_id, dest_id,
                  iotlab_nodes[dest_index][0], iotlab_nodes[dest_index][1],
                  count);
#endif
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");
        snprintf(str, sizeof(str), "hello %d", count);
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);

        dest_id++;
        if(dest_id > NODE_NUM) { /* the last non-root node */
          dest_id = 2;
          count++;
        } else { /* not the last non-root node yet */
          etimer_reset(&send_timer);
        }
      }
#endif
    }
#endif
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
