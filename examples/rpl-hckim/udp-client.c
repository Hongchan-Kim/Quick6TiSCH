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
#include "orchestra.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "node-info.h"
#include "sys/node-id.h"

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#ifndef APP_PRINT_NODE_INFO_DELAY
#define APP_PRINT_NODE_INFO_DELAY (1 * 30 * CLOCK_SECOND)
#endif

#ifndef APP_RESET_LOG_DELAY
#define APP_RESET_LOG_DELAY       (25 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_DATA_START_DELAY
#define APP_DATA_START_DELAY      (30 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_UPWARD_SEND_INTERVAL
#define APP_UPWARD_SEND_INTERVAL  (1 * 60 * CLOCK_SECOND)
#endif

#ifndef APP_UPWARD_MAX_TX
#define APP_UPWARD_MAX_TX          100
#endif

static struct simple_udp_connection udp_conn;

static unsigned count = 1;
static uint16_t app_rxd_count;
static uint64_t lt_down_sum = 0;

#if HCK_ASAP_EVAL_01_SINGLE_HOP_UPA_GAIN
static uint32_t  eval_01_count = 1;
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
reset_log_app_client()
{
  count = 1;
}
/*---------------------------------------------------------------------------*/
static void
reset_log()
{
#if APP_TOPOLOGY_OPT_DURING_BOOTSTRAP
  tsch_queue_reset_except_n_eb();
  tsch_queue_free_unused_neighbors();
#endif
  reset_log_app_client();         /* udp-client.c */
  reset_log_tcpip();              /* tcpip.c */
  reset_log_sicslowpan();         /* sicslowpan.c */
  reset_log_tsch();               /* tsch.c */
  reset_log_rpl_icmp6();          /* rpl-icmp6.c */
  reset_log_rpl_dag();            /* rpl-dag.c */
  reset_log_rpl_timers();         /* rpl-timers.c */
  reset_log_rpl();                /* rpl.c */
  reset_log_rpl_ext_header();     /* rpl-ext-header.c */
  reset_log_simple_energest();    /* simple-energest.c */

  uint64_t app_client_reset_log_asn = tsch_calculate_current_asn();
  LOG_HK("reset_log 1 rs_q_except_eb %d rs_opkn %u | eb_q %d at %llx\n", 
        tsch_queue_global_packet_count() - tsch_queue_nbr_packet_count(n_eb), 
        orchestra_parent_knows_us,
        tsch_queue_nbr_packet_count(n_eb),
        app_client_reset_log_asn);
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

  uint64_t app_tx_down_asn = 0;
  memcpy(&app_tx_down_asn, data + datalen - 14, 8);

  uint32_t app_received_seqno = 0;
  memcpy(&app_received_seqno, data + datalen - 6, 4);

  uint16_t app_received_seqno_count = app_received_seqno % (1 << 16);

  if(app_down_sequence_is_duplicate(app_received_seqno_count)) {
    LOG_HK("| dup_down from %u a_seq %lx at %llx\n",
              HCK_GET_NODE_ID_FROM_IPADDR(sender_addr),
              app_received_seqno,
              app_rx_down_asn);
  } else {
    app_down_sequence_register_seqno(app_received_seqno_count);
    LOG_INFO("Received message from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    lt_down_sum += (app_rx_down_asn - app_tx_down_asn);
    ++app_rxd_count;
    LOG_HK("rx_down %u lt_down_sum %llu | from %u a_seq %lx len %u at %llx (%llu %llx)\n",
              app_rxd_count,
              lt_down_sum, //
              HCK_GET_NODE_ID_FROM_IPADDR(sender_addr),
              app_received_seqno,
              datalen, 
              app_rx_down_asn,
              app_tx_down_asn,
              app_rx_down_asn - app_tx_down_asn);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer print_node_info_timer;
  static struct etimer reset_log_timer;

#if APP_TOPOLOGY_OPT_DURING_BOOTSTRAP
  static struct etimer opt_start_timer;
  static struct etimer opt_periodic_timer;
#endif

  static struct etimer data_start_timer;
  static struct etimer data_periodic_timer;
  static struct etimer data_send_timer;

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

  etimer_set(&print_node_info_timer, APP_PRINT_NODE_INFO_DELAY);
  etimer_set(&reset_log_timer, APP_RESET_LOG_DELAY);

#if APP_TOPOLOGY_OPT_DURING_BOOTSTRAP
  etimer_set(&opt_start_timer, (APP_TOPOLOGY_OPT_START_DELAY + random_rand() % (APP_TOPOLOGY_OPT_SEND_INTERVAL / 2)));
#endif

#if WITH_VARYING_PPM
  etimer_set(&data_start_timer, (APP_DATA_START_DELAY + random_rand() % APP_UPWARD_SEND_VARYING_INTERVAL[0]));
#else
  etimer_set(&data_start_timer, (APP_DATA_START_DELAY + random_rand() % (APP_UPWARD_SEND_INTERVAL)));
#endif

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_TIMER);
    if(data == &print_node_info_timer) {
#if WITH_IOTLAB
      print_iotlab_node_info();
#endif
    }
#if APP_TOPOLOGY_OPT_DURING_BOOTSTRAP
    else if(data == &opt_start_timer || data == &opt_periodic_timer) {
      if(data == &opt_start_timer) {
        uint64_t app_opt_start_asn = tsch_calculate_current_asn();
        LOG_HK("| opt_start at %llx \n", app_opt_start_asn);
      }
      if(count <= APP_TOPOLOGY_OPT_MAX_TX) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, APP_ROOT_ID);

        uint64_t app_tx_up_asn = tsch_calculate_current_asn();

        app_seqno = (1 << 28) + ((uint32_t)node_id << 16) + count;
        app_magic = (uint16_t)APP_DATA_MAGIC;

        memcpy(app_payload + current_payload_len - sizeof(app_tx_up_asn) - sizeof(app_seqno) - sizeof(app_magic), 
              &app_tx_up_asn, sizeof(app_tx_up_asn));
        memcpy(app_payload + current_payload_len - sizeof(app_seqno) - sizeof(app_magic), &app_seqno, sizeof(app_seqno));
        memcpy(app_payload + current_payload_len - sizeof(app_magic), &app_magic, sizeof(app_magic));

        /* Send to DAG root */
        LOG_INFO("Sending message to ");
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        LOG_HK("tx_up %u | opt to %u a_seq %lx len %u at %llx\n", 
                  count,
                  HCK_GET_NODE_ID_FROM_IPADDR(&dest_ipaddr),
                  app_seqno,
                  current_payload_len,
                  app_tx_up_asn);

        simple_udp_sendto(&udp_conn, app_payload, current_payload_len, &dest_ipaddr);

        count++;
      }
      if(count <= APP_TOPOLOGY_OPT_MAX_TX) {
        etimer_set(&opt_periodic_timer, APP_TOPOLOGY_OPT_SEND_INTERVAL);
      }
    }
#endif
    else if(data == &reset_log_timer) {
      reset_log();
    } else if(data == &data_start_timer || data == &data_periodic_timer) {
#if WITH_VARYING_PPM
      etimer_set(&data_send_timer, random_rand() % (APP_UPWARD_SEND_VARYING_INTERVAL[index] / 2));
      etimer_set(&data_periodic_timer, APP_UPWARD_SEND_VARYING_INTERVAL[index]);
#else
      etimer_set(&data_send_timer, random_rand() % (APP_UPWARD_SEND_INTERVAL / 2));
      etimer_set(&data_periodic_timer, APP_UPWARD_SEND_INTERVAL);
#endif
    } else if(data == &data_send_timer) {
#if WITH_VARYING_PPM
      if(varycount < APP_UPWARD_VARYING_MAX_TX[index]) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, APP_ROOT_ID);

        uint64_t app_tx_up_asn = tsch_calculate_current_asn();

        app_seqno = (1 << 28) + ((uint32_t)node_id << 16) + count;
        app_magic = (uint16_t)APP_DATA_MAGIC;

        memcpy(app_payload + current_payload_len - sizeof(app_tx_up_asn) - sizeof(app_seqno) - sizeof(app_magic), 
              &app_tx_up_asn, sizeof(app_tx_up_asn));
        memcpy(app_payload + current_payload_len - sizeof(app_seqno) - sizeof(app_magic), &app_seqno, sizeof(app_seqno));
        memcpy(app_payload + current_payload_len - sizeof(app_magic), &app_magic, sizeof(app_magic));

        /* Send to DAG root */
        LOG_INFO("Sending message to ");
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        LOG_HK("tx_up %u | to %u a_seq %lx len %u at %llx\n", 
                  count,
                  HCK_GET_NODE_ID_FROM_IPADDR(&dest_ipaddr),
                  app_seqno,
                  current_payload_len,
                  app_tx_up_asn);

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

#if HCK_ASAP_EVAL_01_SINGLE_HOP_UPA_GAIN
        current_payload_len = APP_PAYLOAD_LEN_MIN + (eval_01_count - 1) / NUM_OF_PACKETS_PER_EACH_APP_PAYLOAD_LEN;
#endif

        memcpy(app_payload + current_payload_len - sizeof(app_tx_up_asn) - sizeof(app_seqno) - sizeof(app_magic), 
              &app_tx_up_asn, sizeof(app_tx_up_asn));
        memcpy(app_payload + current_payload_len - sizeof(app_seqno) - sizeof(app_magic), &app_seqno, sizeof(app_seqno));
        memcpy(app_payload + current_payload_len - sizeof(app_magic), &app_magic, sizeof(app_magic));

        /* Send to DAG root */
        LOG_INFO("Sending message to ");
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        LOG_HK("tx_up %u | to %u a_seq %lx len %u at %llx\n", 
                  count,
                  HCK_GET_NODE_ID_FROM_IPADDR(&dest_ipaddr),
                  app_seqno,
                  current_payload_len,
                  app_tx_up_asn);

        simple_udp_sendto(&udp_conn, app_payload, current_payload_len, &dest_ipaddr);

        count++;
#if HCK_ASAP_EVAL_01_SINGLE_HOP_UPA_GAIN
        eval_01_count++;
#endif
      }
#endif /* WITH_VARYING_PPM */
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
