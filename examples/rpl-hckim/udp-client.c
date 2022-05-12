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

#ifndef APP_UPWARD_SEND_INTERVAL
#define APP_UPWARD_SEND_INTERVAL   (1 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_UPWARD_MAX_TX
#define APP_UPWARD_MAX_TX          100
#endif

static struct simple_udp_connection udp_conn;

static uint16_t app_rxd_count;

#if WITH_OST && OST_HANDLE_QUEUED_PACKETS
extern uint8_t bootstrap_period;
#endif

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
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
#if PPSD_LONGER_FRAME
  static char str[PPSD_LONGER_FRAME];
#else
  static char str[32];
#endif
  uip_ipaddr_t dest_ipaddr;

#if WITH_VARYING_PPM
  static int APP_UPWARD_SEND_VARYING_PPM[VARY_LENGTH] = {1, 2, 4, 8, 6, 4, 1, 8};
  static int APP_UPWARD_SEND_VARYING_INTERVAL[VARY_LENGTH];
  static int APP_UPWARD_VARYING_MAX_TX[VARY_LENGTH];

  for(int k = 0; k < VARY_LENGTH; k++){
    APP_UPWARD_SEND_VARYING_INTERVAL[k] = (1 * 60 * CLOCK_SECOND / APP_UPWARD_SEND_VARYING_PPM[k]);
    APP_UPWARD_VARYING_MAX_TX[k] = (APP_DATA_PERIOD / VARY_LENGTH) / APP_UPWARD_SEND_VARYING_INTERVAL[k];
  }

  static unsigned varycount = 1;
  static int index = 0;
#endif

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

#if WITH_VARYING_PPM
  etimer_set(&start_timer, (APP_START_DELAY + random_rand() % APP_UPWARD_SEND_VARYING_INTERVAL[0]));
#else
  etimer_set(&start_timer, (APP_START_DELAY + random_rand() % (APP_UPWARD_SEND_INTERVAL)));
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
    } else if(data == &start_timer || data == &periodic_timer) {
#if WITH_VARYING_PPM
      etimer_set(&send_timer, random_rand() % (APP_UPWARD_SEND_VARYING_INTERVAL[index] / 2));
      etimer_set(&periodic_timer, APP_UPWARD_SEND_VARYING_INTERVAL[index]);
#else
      etimer_set(&send_timer, random_rand() % (APP_UPWARD_SEND_INTERVAL / 2));
      etimer_set(&periodic_timer, APP_UPWARD_SEND_INTERVAL);
#endif
    } else if(data == &send_timer) {
#if WITH_VARYING_PPM
      if(varycount < APP_UPWARD_VARYING_MAX_TX[index]) {
#if WITH_COOJA
        uip_ip6addr((&dest_ipaddr), 0xfe80, 0, 0, 0, 0, 0, 0, COOJA_ROOT_ID);
#elif WITH_IOTLAB
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, IOTLAB_ROOT_ID);
#endif
        /* Send to DAG root */
        LOG_INFO("HCK tx_up %u | Sending message %u to ", count, count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        snprintf(str, sizeof(str), "hello %d", count);
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
        count++;
        varycount++;
      } else if (index < VARY_LENGTH - 1){
        index++;
        varycount = 1;
      }
#else /* WITH_VARYING_PPM */
      if(count <= APP_UPWARD_MAX_TX) {
#if WITH_COOJA
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, COOJA_ROOT_ID);
#elif WITH_IOTLAB
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, IOTLAB_ROOT_ID);
#endif
        /* Send to DAG root */
        LOG_INFO("HCK tx_up %u | Sending message %u to ", count, count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

#if PPSD_LONGER_FRAME
        printf("tx_up sizeof str %d\n", sizeof(str));
        snprintf(str, sizeof(str), "hello %d zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz", count);
#else
        snprintf(str, sizeof(str), "hello %d", count);
#endif
        simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
        count++;
      }
#endif /* WITH_VARYING_PPM */
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
