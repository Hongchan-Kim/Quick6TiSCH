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
#include "sys/node-id.h"

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
#if APP_SEQNO_DUPLICATE_CHECK
struct app_down_seqno {
  clock_time_t app_down_timestamp;
  uint8_t app_down_seqno;
};
static struct app_down_seqno received_seqnos[APP_SEQNO_HISTORY];
/*---------------------------------------------------------------------------*/
int
app_down_sequence_is_duplicate(uint16_t current_app_down_seqno)
{
  int i;
  clock_time_t now = clock_time();

  /*
   * Check for duplicate packet by comparing the sequence number of the incoming
   * packet with the last few ones we saw.
   */
  for(i = 0; i < APP_SEQNO_HISTORY; ++i) {
    if(current_app_down_seqno == received_seqnos[i].app_down_seqno) {
#if APP_SEQNO_MAX_AGE > 0
      if(now - received_seqnos[i].app_down_timestamp <= APP_SEQNO_MAX_AGE) {
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
app_down_sequence_register_seqno(uint16_t current_app_down_seqno)
{
  int i, j;

  /* Locate possible previous sequence number for this address. */
  for(i = 0; i < APP_SEQNO_HISTORY; ++i) {
    if(current_app_down_seqno == received_seqnos[i].app_down_seqno) {
      i++;
      break;
    }
  }

  /* Keep the last sequence number for each address as per 802.15.4e. */
  for(j = i - 1; j > 0; --j) {
    memcpy(&received_seqnos[j], &received_seqnos[j - 1], sizeof(struct app_down_seqno));
  }
  received_seqnos[0].app_down_seqno = current_app_down_seqno;
  received_seqnos[0].app_down_timestamp = clock_time();
}
#endif /* APP_SEQNO_DUPLICATE_CHECK */
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
  uint64_t app_rx_down_asn = tsch_calculate_current_asn();

  uint32_t app_received_seqno = 0;
  memcpy(&app_received_seqno, data + datalen - 6, 4);

  uint16_t app_received_seqno_count = app_received_seqno % (1 << 16);

  if(app_down_sequence_is_duplicate(app_received_seqno_count)) {
    LOG_INFO("HCK dup_down a_seq %lx asn %llu from ", 
              app_received_seqno,
              app_rx_down_asn);
  } else {
    app_down_sequence_register_seqno(app_received_seqno_count);
    LOG_INFO("HCK rx_down %u a_seq %lx asn %llu len %u | Received message from ",
              ++app_rxd_count,
              app_received_seqno,
              app_rx_down_asn,
              datalen);
  }
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer start_timer;
  static struct etimer print_timer;
  static struct etimer reset_log_timer;
  static struct etimer periodic_timer;
  static struct etimer send_timer;

  static unsigned count = 1;
  uip_ipaddr_t dest_ipaddr;

  static uint8_t app_payload[128];
  static uint16_t current_payload_len = APP_PAYLOAD_LEN;
  static uint32_t app_seqno = 0;
  static uint16_t app_magic = (uint16_t)APP_DATA_MAGIC;


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
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, APP_ROOT_ID);

        uint64_t app_tx_up_asn = tsch_calculate_current_asn();

        app_seqno = (1 << 28) + ((uint32_t)node_id << 16) + count;
        app_magic = (uint16_t)APP_DATA_MAGIC;

        memcpy(app_payload + current_payload_len - sizeof(app_seqno) - sizeof(app_magic), &app_seqno, sizeof(app_seqno));
        memcpy(app_payload + current_payload_len - sizeof(app_magic), &app_magic, sizeof(app_magic));

        /* Send to DAG root */
        LOG_INFO("HCK tx_up %u a_seq %lx asn %llu len %u | Sending message to ", 
                  count, app_seqno, app_tx_up_asn, current_payload_len);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        simple_udp_sendto(&udp_conn, app_payload, current_payload_len, &dest_ipaddr);

        count++;
        varycount++;
      } else if (index < VARY_LENGTH - 1) {
        index++;
        varycount = 1;
      }
#else /* WITH_VARYING_PPM */
      if(count <= APP_UPWARD_MAX_TX) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, APP_ROOT_ID);

        uint64_t app_tx_up_asn = tsch_calculate_current_asn();

        app_seqno = (1 << 28) + ((uint32_t)node_id << 16) + count;
        app_magic = (uint16_t)APP_DATA_MAGIC;

        memcpy(app_payload + current_payload_len - sizeof(app_seqno) - sizeof(app_magic), &app_seqno, sizeof(app_seqno));
        memcpy(app_payload + current_payload_len - sizeof(app_magic), &app_magic, sizeof(app_magic));

        /* Send to DAG root */
        LOG_INFO("HCK tx_up %u a_seq %lx asn %llu len %u | Sending message to ", 
                  count, app_seqno, app_tx_up_asn, current_payload_len);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        simple_udp_sendto(&udp_conn, app_payload, current_payload_len, &dest_ipaddr);

        count++;
      }
#endif /* WITH_VARYING_PPM */
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
