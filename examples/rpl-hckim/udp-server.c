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

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define DOWNWARD_TRAFFIC  1
#if DOWNWARD_TRAFFIC
#define START_DELAY       (1 * 60 * CLOCK_SECOND)
#define SEND_INTERVAL		  (60 * CLOCK_SECOND)
#endif

static struct simple_udp_connection udp_conn;

#define NON_ROOT_NUM 16
static uint16_t non_root_info[NON_ROOT_NUM][3] = { // id, addr, rx
    {1, 0x9768, 0},
    {2, 0x8867, 0},
    {3, 0x8676, 0},
    {4, 0xb181, 0},
    {5, 0x8968, 0},
    {6, 0xc279, 0},
    //{7, 0xa371, 0}, // root node
    {8, 0xa683, 0},
    //{9, 0xb677, 0}, // disabled
    {10, 0x8976, 0},
    {11, 0x8467, 0},
    {12, 0xb682, 0},
    {13, 0xb176, 0},
    {14, 0x2860, 0},
    {15, 0xa377, 0},
    {16, 0xb978, 0},
    {17, 0xa168, 0},
    {18, 0x3261, 0}
};
#define DOWN_INTERVAL     (SEND_INTERVAL / NON_ROOT_NUM)

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static uint16_t
sender_index_from_addr(const uip_ipaddr_t *sender_addr)
{
  uint16_t sender_uid = (sender_addr->u8[14] << 8) + sender_addr->u8[15];

  uint16_t i = 0;
  for(i = 0; i < NON_ROOT_NUM; i++) {
    if(non_root_info[i][1] == sender_uid) {
      return i;
    }
  }
  return NON_ROOT_NUM;
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
  uint16_t sender_index = sender_index_from_addr(sender_addr);
  if(sender_index == NON_ROOT_NUM) {
    LOG_INFO("Fail to receive: out of index\n");
    return;
  }
  LOG_INFO("HCK rxu %u from %u %x | Received message '%.*s' from ", 
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

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

#if DOWNWARD_TRAFFIC
  etimer_set(&periodic_timer, (START_DELAY + random_rand() % DOWN_INTERVAL));
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, non_root_info[curr][1]);

    /* Send to clients */
    LOG_INFO("HCK txd %u to %u %x | Sending message %u to ", 
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

    /* Add some jitter */
    etimer_set(&periodic_timer, DOWN_INTERVAL);
    //  - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
  }
#endif

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
