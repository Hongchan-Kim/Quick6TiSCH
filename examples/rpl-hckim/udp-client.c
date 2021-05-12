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

#ifndef APP_SEND_INTERVAL
#define APP_SEND_INTERVAL   (1 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_MAX_TX
#define APP_MAX_TX          100
#endif

static struct simple_udp_connection udp_conn;

static uint16_t app_rxd_count;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
static void
reset_log()
{
  LOG_INFO("HCK reset_log\n");
  reset_log_tcpip();              /* tcpip.c */
  reset_log_sicslowpan();         /* sicslowpan.c */
  reset_log_tsch();               /* tsch.c */
  reset_log_rpl_icmp6();          /* rpl-icmp6.c */
  reset_log_rpl_dag();            /* rpl-dag.c */
  reset_log_rpl_timers();         /* rpl-timers.c */
  reset_log_rpl();                /* rpl.c */
  simple_energest_init();         /* simple-energest.c */
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
  LOG_INFO("HCK rx_down %u | Received message '%.*s' from ", ++app_rxd_count, datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer start_timer;
  static struct etimer print_timer;
  static struct etimer reset_log_timer;
  static struct etimer periodic_timer;
  static struct etimer send_timer;

  static unsigned count = 1;
  static char str[32];
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&start_timer, (APP_START_DELAY + random_rand() % (APP_SEND_INTERVAL)));
  etimer_set(&print_timer, APP_PRINT_DELAY);
  etimer_set(&reset_log_timer, APP_START_DELAY);

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_TIMER);
    if(data == &print_timer) {
      print_node_info();
    } else if(data == &reset_log_timer) {
      reset_log();
    } else if(data == &start_timer || data == &periodic_timer) {
      etimer_set(&send_timer, random_rand() % (APP_SEND_INTERVAL / 2));
      etimer_set(&periodic_timer, APP_SEND_INTERVAL);
    } else if(data == &send_timer) {
      if(count <= APP_MAX_TX) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, IOTLAB_ROOT_ID);
        /* Send to DAG root */
        LOG_INFO("HCK tx_up %u | Sending message %u to ", count, count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        snprintf(str, sizeof(str), "hello %d", count);
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
        count++;
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
