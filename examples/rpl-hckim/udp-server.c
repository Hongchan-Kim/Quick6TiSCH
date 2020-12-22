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

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "node-info.h"

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#ifndef APP_START_DELAY
#define APP_START_DELAY     (1 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_SEND_INTERVAL
#define APP_SEND_INTERVAL   (1 * 60 * CLOCK_SECOND)
#endif

static struct simple_udp_connection udp_conn;

#if DOWNWARD_TRAFFIC
#define APP_DOWN_INTERVAL     (APP_SEND_INTERVAL / NON_ROOT_NUM)
#endif

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
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
  uint16_t sender_index = non_root_index_from_addr(sender_addr);
  if(sender_index == NON_ROOT_NUM) {
    LOG_INFO("Fail to receive: out of index\n");
    return;
  }
  LOG_INFO("HCK rx_up %u from %u %x | Received message '%.*s' from ", 
            ++non_root_info[sender_index][2], non_root_info[sender_index][0],
            non_root_info[sender_index][1],
            datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
#if DOWNWARD_TRAFFIC
  static struct etimer periodic_timer;
  static unsigned count = 1;
  static unsigned curr = 0;
  static char str[32];
  uip_ipaddr_t dest_ipaddr;
#endif

  PROCESS_BEGIN();

  print_node_info();

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

#if DOWNWARD_TRAFFIC
  etimer_set(&periodic_timer, (APP_START_DELAY + random_rand() % APP_DOWN_INTERVAL));
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, non_root_info[curr][1]);

    /* Send to clients */
    LOG_INFO("HCK tx_down %u to %u %x | Sending message %u to ", 
              count, non_root_info[curr][0], non_root_info[curr][1], count);
    LOG_INFO_6ADDR(&dest_ipaddr);
    LOG_INFO_("\n");
    snprintf(str, sizeof(str), "hello %d", count);
    simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);

    curr++;
    if(curr >= NON_ROOT_NUM) {
      curr = 0;
      count++;
    }

    /* Add no jitter */
    etimer_set(&periodic_timer, APP_DOWN_INTERVAL);
  }
#endif

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
