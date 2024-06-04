#include "contiki.h"
#include "net/routing/routing.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

/* to reset log */
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/tcpip.h"
#include "net/ipv6/sicslowpan.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/rpl-classic/rpl.h"
#include "services/simple-energest/simple-energest.h"
#if !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
#include "orchestra.h"
#endif

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP // LOG_LEVEL_INFO

#include "node-info.h"
#include "sys/node-id.h"

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

static struct simple_udp_connection udp_conn;

#if WITH_UPWARD_TRAFFIC
static unsigned data_count = 1;
#endif

static uint16_t app_rxd_count;

/*---------------------------------------------------------------------------*/
#if HCK_MOD_APP_SEQNO_DUPLICATE_CHECK
struct app_down_seqno {
  clock_time_t app_down_timestamp;
  uint16_t app_down_seqno;
};
static struct app_down_seqno app_down_received_seqnos[APP_SEQNO_HISTORY];
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
    if(current_app_down_seqno == app_down_received_seqnos[i].app_down_seqno) {
#if APP_SEQNO_MAX_AGE > 0
      if(now - app_down_received_seqnos[i].app_down_timestamp <= APP_SEQNO_MAX_AGE) {
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
    if(current_app_down_seqno == app_down_received_seqnos[i].app_down_seqno) {
      i++;
      break;
    }
  }

  /* Keep the last sequence number for each address as per 802.15.4e. */
  for(j = i - 1; j > 0; --j) {
    memcpy(&app_down_received_seqnos[j], &app_down_received_seqnos[j - 1], sizeof(struct app_down_seqno));
  }
  app_down_received_seqnos[0].app_down_seqno = current_app_down_seqno;
  app_down_received_seqnos[0].app_down_timestamp = clock_time();
}
#endif /* HCK_MOD_APP_SEQNO_DUPLICATE_CHECK */
/*---------------------------------------------------------------------------*/
static void
reset_log_app_client()
{
  app_rxd_count = 0;

#if HCK_MOD_APP_SEQNO_DUPLICATE_CHECK
  uint8_t j = 0;
  for(j = 0; j < APP_SEQNO_HISTORY; j++) {
    app_down_received_seqnos[j].app_down_seqno = 0;
    app_down_received_seqnos[j].app_down_timestamp = 0;
  }
#endif
}
/*---------------------------------------------------------------------------*/
static void
reset_eval(uint8_t phase)
{
  tsch_queue_reset_except_n_eb();
  tsch_queue_free_unused_neighbors();

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
#if !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
  LOG_HCK("reset_eval %u rs_rpku %u rs_q %d | rs_q_eb %d at %llx\n", 
        phase,
        orchestra_parent_knows_us,
        tsch_queue_global_packet_count(),
        tsch_queue_nbr_packet_count(n_eb),
        app_client_reset_log_asn);
#else /* !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL */
  LOG_HCK("reset_eval %u rs_rpku %u rs_q %d | rs_q_eb %d at %llx\n", 
        phase,
        mc_parent_knows_us,
        tsch_queue_global_packet_count(),
        tsch_queue_nbr_packet_count(n_eb),
        app_client_reset_log_asn);
#endif /* !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL */

#if HCK_LOG_EVAL_CONFIG
  LOG_HCK("eval_config 1 lite_log %u |\n", 
          HCK_LOG_LEVEL_LITE);
#if TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
  LOG_HCK("eval_config 2 mcsf %u |\n",
          TSCH_SCHEDULE_DEFAULT_LENGTH);
#else
  LOG_HCK("eval_config 2 ebsf %u cssf %u ucsf %u |\n",
          ORCHESTRA_CONF_EBSF_PERIOD, 
          ORCHESTRA_CONF_COMMON_SHARED_PERIOD, 
          ORCHESTRA_CONF_UNICAST_PERIOD);
#endif /* !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL */
#if WITH_TRGB
  LOG_HCK("eval_config 3 with_trgb %u |\n", WITH_TRGB);
#elif 0//WITH_QUICK6
#if QUICK6_OFFSET_ASSIGNMENT == QUICK6_OFFSET_ASSIGNMENT_RANDOM
  LOG_HCK("eval_config 3 with_top rand offs %u |\n",
          QUICK6_NUM_OF_OFFSETS);
#elif QUICK6_OFFSET_ASSIGNMENT == QUICK6_OFFSET_ASSIGNMENT_STATE_BASED
  LOG_HCK("eval_config 3 with_top s_b offs %u thr %u fco %u nco %u que %u esc %u sel %u retx %u bac %u |\n", 
          QUICK6_NUM_OF_OFFSETS,
          QUICK6_OFFSET_EB_DIO_CRITICAL_THRESH,
          QUICK6_OFFSET_ASSIGNMENT_CRITICAL_PKTS_OFFSET,
          QUICK6_OFFSET_ASSIGNMENT_NON_CRITICAL_PKTS_OFFSET,
          QUICK6_DUPLICATE_PACKET_MANAGEMENT,
          QUICK6_NO_TX_COUNT_INCREASE_FOR_POSTPONED_PACKETS,
          QUICK6_BACKOFF_FOR_BCAST_PACKETS);
#endif
#endif
#endif
}
/*---------------------------------------------------------------------------*/
static void
print_log()
{
  print_log_tsch();
  print_log_rpl_timers();
  print_log_rpl();
  print_log_simple_energest();
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

#if HCK_MOD_APP_SEQNO_DUPLICATE_CHECK
  uint16_t app_received_seqno_count = app_received_seqno % (1 << 16);

  if(app_down_sequence_is_duplicate(app_received_seqno_count)) {
    LOG_HCK("| dup_down from %u a_seq %lx at %llx\n",
              HCK_GET_NODE_ID_FROM_IPADDR(sender_addr),
              app_received_seqno,
              app_rx_down_asn);
  } else {
    app_down_sequence_register_seqno(app_received_seqno_count);
#endif /* HCK_MOD_APP_SEQNO_DUPLICATE_CHECK */
    LOG_INFO("Received message from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    ++app_rxd_count;
    uint8_t hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;

    LOG_HCK("rx_down %u | from %u a_seq %lx len %u lt_down_r %llx lt_down_t %llx hops %u\n",
              app_rxd_count, //
              HCK_GET_NODE_ID_FROM_IPADDR(sender_addr),
              app_received_seqno,
              datalen, 
              app_rx_down_asn,
              app_tx_down_asn,
              hops);
#if HCK_MOD_APP_SEQNO_DUPLICATE_CHECK
  }
#endif /* HCK_MOD_APP_SEQNO_DUPLICATE_CHECK */
}
/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer print_node_info_timer;
  static struct etimer reset_before_data_timer;
  static struct etimer print_log_timer;

#if WITH_UPWARD_TRAFFIC
  static struct etimer data_start_timer;
  static struct etimer data_periodic_timer;
  static struct etimer data_send_timer;

  uip_ipaddr_t dest_ipaddr;
  static uint8_t app_payload[128];
  static uint16_t current_payload_len = APP_PAYLOAD_LEN;
  static uint32_t app_seqno = 0;
  static uint16_t app_magic = (uint16_t)APP_DATA_MAGIC;
#endif

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&print_node_info_timer, APP_PRINT_NODE_INFO_DELAY);
  etimer_set(&reset_before_data_timer, APP_RESET_BEFORE_DATA_DELAY);
  etimer_set(&print_log_timer, APP_PRINT_LOG_DELAY);

#if WITH_UPWARD_TRAFFIC
  etimer_set(&data_start_timer, (APP_DATA_START_DELAY + random_rand() % (APP_UPWARD_SEND_INTERVAL)));
#endif

#if HCK_LOG_EVAL_CONFIG
  LOG_HCK("eval_config 1 lite_log %u |\n", 
          HCK_LOG_LEVEL_LITE);
#if TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
  LOG_HCK("eval_config 2 mcsf %u |\n",
          TSCH_SCHEDULE_DEFAULT_LENGTH);
#else
  LOG_HCK("eval_config 2 ebsf %u cssf %u ucsf %u |\n",
          ORCHESTRA_CONF_EBSF_PERIOD, 
          ORCHESTRA_CONF_COMMON_SHARED_PERIOD, 
          ORCHESTRA_CONF_UNICAST_PERIOD);
#endif /* !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL */

#if WITH_TRGB
  LOG_HCK("eval_config 3 with_trgb %u |\n", WITH_TRGB);
#elif 0//WITH_QUICK6
#if QUICK6_OFFSET_ASSIGNMENT == QUICK6_OFFSET_ASSIGNMENT_RANDOM
  LOG_HCK("eval_config 3 with_top rand offs %u |\n",
          QUICK6_NUM_OF_OFFSETS);
#elif QUICK6_OFFSET_ASSIGNMENT == QUICK6_OFFSET_ASSIGNMENT_STATE_BASED
  LOG_HCK("eval_config 3 with_top s_b offs %u thr %u fco %u nco %u que %u esc %u sel %u retx %u bac %u |\n", 
          QUICK6_NUM_OF_OFFSETS,
          QUICK6_OFFSET_EB_DIO_CRITICAL_THRESH,
          QUICK6_OFFSET_ASSIGNMENT_CRITICAL_PKTS_OFFSET,
          QUICK6_OFFSET_ASSIGNMENT_NON_CRITICAL_PKTS_OFFSET,
          QUICK6_DUPLICATE_PACKET_MANAGEMENT,
          QUICK6_NO_TX_COUNT_INCREASE_FOR_POSTPONED_PACKETS,
          QUICK6_BACKOFF_FOR_BCAST_PACKETS);
#endif
#endif
#endif

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_TIMER);
    if(data == &print_node_info_timer) {
      print_iotlab_node_info();
    }
    else if(data == &reset_before_data_timer) {
      reset_eval(1);
    }
#if WITH_UPWARD_TRAFFIC
    else if(data == &data_start_timer || data == &data_periodic_timer) {
      if(data == &data_start_timer) {
        uint64_t app_data_start_asn = tsch_calculate_current_asn();
        LOG_HCK("| data_start at %llx \n", app_data_start_asn);
      }
      if(data_count <= APP_UPWARD_MAX_TX) {
        etimer_set(&data_send_timer, random_rand() % (APP_UPWARD_SEND_INTERVAL / 2));
      }
      if(data_count < APP_UPWARD_MAX_TX) {
        etimer_set(&data_periodic_timer, APP_UPWARD_SEND_INTERVAL);
      }
    } else if(data == &data_send_timer) {
      if(data_count <= APP_UPWARD_MAX_TX) {
        uip_ip6addr((&dest_ipaddr), 0xfd00, 0, 0, 0, 0, 0, 0, APP_ROOT_ID);

        uint64_t app_tx_up_asn = tsch_calculate_current_asn();

        app_seqno = (1 << 28) + ((uint32_t)node_id << 16) + data_count;
        app_magic = (uint16_t)APP_DATA_MAGIC;

        memcpy(app_payload + current_payload_len - sizeof(app_tx_up_asn) - sizeof(app_seqno) - sizeof(app_magic), 
              &app_tx_up_asn, sizeof(app_tx_up_asn));
        memcpy(app_payload + current_payload_len - sizeof(app_seqno) - sizeof(app_magic), &app_seqno, sizeof(app_seqno));
        memcpy(app_payload + current_payload_len - sizeof(app_magic), &app_magic, sizeof(app_magic));

        /* Send to DAG root */
        LOG_INFO("Sending message to ");
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        LOG_HCK("tx_up %u | to %u a_seq %lx len %u at %llx\n", 
                  data_count,
                  HCK_GET_NODE_ID_FROM_IPADDR(&dest_ipaddr),
                  app_seqno,
                  current_payload_len,
                  app_tx_up_asn);

        simple_udp_sendto(&udp_conn, app_payload, current_payload_len, &dest_ipaddr);

        data_count++;
      }
    }
#endif
    else if(data == &print_log_timer) {
      etimer_set(&print_log_timer, APP_PRINT_LOG_PERIOD);
      print_log();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
