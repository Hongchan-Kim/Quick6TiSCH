#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/***************************************************************
 * Prerequisite macros for HCK's implementations
 ****************************************************************/
#define HCK_GET_NODE_ID_FROM_IPADDR(addr)                   ((((addr)->u8[14]) << 8) | (addr)->u8[15])
#define HCK_GET_NODE_ID_FROM_LINKADDR(addr)                 ((((addr)->u8[LINKADDR_SIZE - 2]) << 8) | (addr)->u8[LINKADDR_SIZE - 1]) 


/***************************************************************
 * HCK's modifications of Contiki-NG
 ****************************************************************/
#define HCK_MOD_APP_SEQNO_DUPLICATE_CHECK                   1
#define HCK_MOD_MAC_SEQNO_DUPLICATE_CHECK                   1
//
#define HCK_MOD_RPL_CODE_NO_PATH_DAO                        1
#define HCK_MOD_RPL_IGNORE_REDUNDANCY_IN_BOOTSTRAP          0
#define HCK_MOD_RPL_RELAX_ETX_NOACK_PENALTY                 1
#define HCK_MOD_RPL_DAO_RETX_OPERATION                      1 /* stop dao retransmission when preferred parent changed */
#define HCK_MOD_RPL_DAO_PARENT_NULLIFICATION                1 /* nullify old preferred parent before sending no-path dao, this makes no-path dao sent through common shared slotframe */
//
#define HCK_MOD_TSCH_APPLY_LATEST_CONTIKI                   1
#define HCK_MOD_TSCH_DEACTIVATE_RADIO_INTERRUPT_MODE        1
#define HCK_MOD_TSCH_SWAP_TX_RX_PROCESS_PENDING             1 /* swap order of rx_process_pending and tx_process_pending */
#define HCK_MOD_TSCH_FILTER_PACKETS_WITH_INVALID_RX_TIMING  1
#define HCK_MOD_TSCH_HANDLE_OVERFULL_SLOT_OPERATION         1
//
#define HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR      1
#define HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_NON_RPL_NBR   0
#define HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_RPL_NBR       1
//
#define HCK_MOD_TSCH_SYNC_COUNT                             1
//
#define HCK_MOD_TSCH_PIGGYBACKING_HEADER_IE_32BITS          1 /* piggyback information of 32 bits to header IE of non-EB packets */
#define HCK_MOD_TSCH_PIGGYBACKING_EB_IE_32BITS              1 /* piggyback information of 32 bits to IE of EB packets */
//
#if HCK_MOD_APP_SEQNO_DUPLICATE_CHECK
#define APP_SEQNO_MAX_AGE                                   (20 * CLOCK_SECOND)
#define APP_SEQNO_HISTORY                                   8
#endif
//
#if HCK_MOD_MAC_SEQNO_DUPLICATE_CHECK
#define NETSTACK_CONF_MAC_SEQNO_MAX_AGE                     (20 * CLOCK_SECOND)
#define NETSTACK_CONF_MAC_SEQNO_HISTORY                     8
#endif


/***************************************************************
 * HCK's logging implementation
 ****************************************************************/
#define HCK_LOG                                             1
#define HCK_LOG_LEVEL_LITE                                  1
#define HCK_LOG_EVAL_CONFIG                                 1
#define HCK_LOG_TSCH_LINK_ADD_REMOVE                        1
#define HCK_LOG_TSCH_PACKET_ADD_REMOVE                      1
#define HCK_LOG_TSCH_SLOT                                   1
#define HCK_LOG_TSCH_SLOT_APP_SEQNO                         1
#define HCK_LOG_TSCH_SLOT_RX_OPERATION                      0 /* HCKIM-Eval */
//
#define SIMPLE_ENERGEST_CONF_PERIOD                         (1 * 60 * CLOCK_SECOND)
#define RPL_FIRST_MEASURE_PERIOD                            (1 * 60)
#define RPL_NEXT_MEASURE_PERIOD                             (1 * 60)
#define TSCH_NEXT_PRINT_PERIOD                              (1 * 60 * CLOCK_SECOND)
//
#if HCK_LOG_LEVEL_LITE
#define LOG_LEVEL_APP                                       LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_IPV6                                 LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_RPL                                  LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_6LOWPAN                              LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_TCPIP                                LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_MAC                                  LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER                               LOG_LEVEL_NONE
#define LOG_LEVEL_ENERGEST                                  LOG_LEVEL_NONE
#else
#define LOG_LEVEL_APP                                       LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6                                 LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL                                  LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN                              LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                                LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC                                  LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER                               LOG_LEVEL_INFO
#define LOG_LEVEL_ENERGEST                                  LOG_LEVEL_INFO
#endif


/***************************************************************
 * HCK's topology configuration
 ****************************************************************/
#define WITH_COOJA                                          0
#define WITH_IOTLAB                                         1
//
#if WITH_COOJA
//
#elif WITH_IOTLAB
#define IOTLAB_LILLE_83_CORNER                              1 /* 83 nodes */
//
#define IOTLAB_LILLE_2_CORNER                               11 /* 2 nodes */
#define IOTLAB_LILLE_3_CORNER                               12 /* 3 nodes */
#define IOTLAB_GRENOBLE_3_CORNER                            13 /* 3 nodes */

//
#define IOTLAB_SITE                                         IOTLAB_LILLE_83_CORNER /* HCKIM-Eval */
//
#if IOTLAB_SITE == IOTLAB_LILLE_83_CORNER
#define NODE_NUM                                            83
#elif IOTLAB_SITE == IOTLAB_LILLE_2_CORNER
#define NODE_NUM                                            2
#elif IOTLAB_SITE == IOTLAB_LILLE_3_CORNER
#define NODE_NUM                                            3
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_3_CORNER
#define NODE_NUM                                            3
#endif
#endif


/***************************************************************
 * HCK's configuration for Contiki-NG system
 ****************************************************************/
#define IEEE802154_CONF_PANID                               0x58FA /* 22782 hckim */
#define MAX_NBR_NODE_NUM                                    60
#define NBR_TABLE_CONF_MAX_NEIGHBORS                        (MAX_NBR_NODE_NUM + 2) /* Add 2 for EB and broadcast neighbors in TSCH layer */


/***************************************************************
 * HCK's configuration for application layer (examples/rpl-hckim)
 ****************************************************************/
#if WITH_COOJA
#elif WITH_IOTLAB
#define WITH_UPWARD_TRAFFIC                                 0
#define APP_UPWARD_SEND_INTERVAL                            (1 * 60 * CLOCK_SECOND / 4)
#define WITH_DOWNWARD_TRAFFIC                               0
#define APP_DOWNWARD_SEND_INTERVAL                          (1 * 60 * CLOCK_SECOND / 4)
#define APP_PRINT_NODE_INFO_DELAY                           (1 * 60 * CLOCK_SECOND / 2)
/* Bootstrap timing configurations */
#define APP_RESET_BEFORE_DATA_DELAY                         (730 * 60 * CLOCK_SECOND)
#define APP_DATA_START_DELAY                                (731 * 60 * CLOCK_SECOND)
#define APP_DATA_PERIOD                                     (60 * 60 * CLOCK_SECOND)
#define APP_PRINT_LOG_DELAY                                 (1 * 60 * CLOCK_SECOND)    /* APP_DATA_START_DELAY + APP_DATA_PERIOD + 2 */
#define APP_PRINT_LOG_PERIOD                                (1 * 60 * CLOCK_SECOND)
#endif /* WITH_COOJA or WITH_IOTLAB */
//
#define APP_UPWARD_MAX_TX                                   (APP_DATA_PERIOD / APP_UPWARD_SEND_INTERVAL)
#define APP_DOWNWARD_MAX_TX                                 (APP_DATA_PERIOD / APP_DOWNWARD_SEND_INTERVAL)
//
#define APP_PAYLOAD_LEN                                     14
#define APP_DATA_MAGIC                                      0x58FA


/***************************************************************
 * HCK's configuration for IPv6, 6LoWPAN, and RPL layers
 ****************************************************************/
/* IPv6 and 6LoWPAN layers */
#define UIP_CONF_BUFFER_SIZE                                160
#define UIP_CONF_MAX_ROUTES                                 (NODE_NUM)
#define SICSLOWPAN_CONF_FRAG                                0
/* RPL layer */
#define RPL_CONF_MOP                                        RPL_MOP_STORING_NO_MULTICAST
#define RPL_CONF_WITH_DAO_ACK                               1
#define RPL_CONF_WITH_PROBING                               1
#define RPL_CONF_PROBING_INTERVAL                           (2 * 60 * CLOCK_SECOND) /* originally 60 seconds */
#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT                 (20 * CLOCK_SECOND) /* originally 5 seconds */
#define RPL_CONF_PARENT_SWITCH_THRESHOLD                    96 /* 96 (0.75, default), 127 (0.99), 128 (1) */
#define RPL_MRHOF_CONF_SQUARED_ETX                          0
#define LINK_STATS_CONF_INIT_ETX_FROM_RSSI                  1 /* originally 1 */


/***************************************************************
 * HCK's configuration for TSCH layer
 ****************************************************************/
#define QUEUEBUF_CONF_NUM                                   16 /* 16 in Orchestra, ALICE, and OST, originally 8 */
#define TSCH_CONF_MAX_INCOMING_PACKETS                      8 /* 8 in OST, originally 4 */
#define TSCH_CONF_CCA_ENABLED                               1
//#define TSCH_CONF_AUTOSTART                                0
#ifndef WITH_SECURITY
#define WITH_SECURITY                                       0
#endif /* WITH_SECURITY */
#define TSCH_LOG_CONF_QUEUE_LEN                             32 // originally 16


/***************************************************************
 * HCK's TSCH scheduler implementation
 ****************************************************************/
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL              1

#if TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL
#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH                   7 // default: 7

#define HCK_MOD_6TISCH_MINIMAL_CALLBACK                     1

#undef HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR
#define HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR      1 /* HCK_MOD_6TISCH_MINIMAL_CALLBACK should be 1 */
#undef HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_NON_RPL_NBR
#define HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_NON_RPL_NBR   0 /* This should be 0, if HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR is 1 */
#undef HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_RPL_NBR
#define HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_RPL_NBR       0 /* Not applicable to 6TiSCH-MC */

#else /* TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL */

#undef HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR
#define HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR      1
#undef HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_NON_RPL_NBR
#define HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_NON_RPL_NBR   0 /* This should be 0, if HCK_MOD_TSCH_DROP_UCAST_PACKET_FOR_NON_RPL_NBR is 1 */
#undef HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_RPL_NBR
#define HCK_MOD_TSCH_OFFLOAD_UCAST_PACKET_FOR_RPL_NBR       1 /* Applicable to 6TiSCH-MC */

#define TSCH_SCHEDULER_NB_ORCHESTRA                         1 // 1: NB-Orchestra-storing
#define TSCH_SCHEDULER_LB_ORCHESTRA                         2 // 2: LB-Orchestra
#define TSCH_SCHEDULER_ALICE                                3 // 3: ALICE
#define TSCH_SCHEDULER_OST                                  4 // 4: OST
//
#define CURRENT_TSCH_SCHEDULER                              TSCH_SCHEDULER_NB_ORCHESTRA
//
#define ORCHESTRA_RULE_NB { &eb_per_time_source, \
                          &unicast_per_neighbor_rpl_storing, \
                          &default_common }
#define ORCHESTRA_RULE_LB { &eb_per_time_source, \
                          &unicast_per_neighbor_link_based, \
                          &default_common }
#define ORCHESTRA_RULE_ALICE { &eb_per_time_source, \
                          &default_common, \
                          &unicast_per_neighbor_rpl_storing }
#define ORCHESTRA_RULE_OST { &eb_per_time_source, \
                          &unicast_per_neighbor_rpl_storing, \
                          &default_common }
//

#if CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_NB_ORCHESTRA /* Neighbor-based Orchestra configuration */
#define WITH_ORCHESTRA                                      1
#define ORCHESTRA_CONF_RULES                                ORCHESTRA_RULE_NB // neighbor-storing
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED                 1 // 0: receiver-based, 1: sender-based
#define ORCHESTRA_CONF_EBSF_PERIOD                          397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD                 19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD                       17 //unicast, 7, 11, 13, 17, 19, 23, 31, 43, 47, 59, 67, 71
#define TSCH_SCHED_EB_SF_HANDLE                             0 //slotframe handle of EB slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE                        1 //slotframe handle of unicast slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                         2 //slotframe handle of broadcast/default slotframe
//
#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_LB_ORCHESTRA /* Link-based Orchestra configuration */
#define WITH_ORCHESTRA                                      1
#define ORCHESTRA_CONF_RULES                                ORCHESTRA_RULE_LB //link-based Orchestra
#define ORCHESTRA_CONF_EBSF_PERIOD                          397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD                 19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD                       11 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71
#define TSCH_SCHED_EB_SF_HANDLE                             0 //slotframe handle of EB slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE                        1 //slotframe handle of unicast slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                         2 //slotframe handle of broadcast/default slotframe
//
#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_ALICE /* ALICE configuration */
#define WITH_ALICE                                          1
#define ORCHESTRA_CONF_RULES                                ORCHESTRA_RULE_ALICE
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED                 1 //1: sender-based, 0:receiver-based
#define ORCHESTRA_CONF_EBSF_PERIOD                          397 // EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD                 31 //19, 31, 41, 53, 61, ... 101 // broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD                       20 // unicast, should be longer than (2N-2)/3 to provide contention-free links
#define ALICE_PACKET_CELL_MATCHING_ON_THE_FLY               alice_packet_cell_matching_on_the_fly
#define ALICE_TIME_VARYING_SCHEDULING                       alice_time_varying_scheduling
#define ALICE_EARLY_PACKET_DROP                             0
#define TSCH_SCHEDULE_CONF_MAX_LINKS                        (3 + 2 * MAX_NBR_NODE_NUM + 2) /* EB SF: tx/rx, CS SF: one link, UC SF: tx/rx for each node + 2 for spare */
#define ENABLE_ALICE_PACKET_CELL_MATCHING_LOG               0
#define ENABLE_ALICE_EARLY_PACKET_DROP_LOG                  0
#undef HCK_LOG_TSCH_LINK_ADD_REMOVE
#define HCK_LOG_TSCH_LINK_ADD_REMOVE                        0
#define ENABLE_LOG_ALICE_LINK_ADD_REMOVE                    0
#define TSCH_SCHED_EB_SF_HANDLE                             0 //slotframe handle of EB slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                         1 //slotframe handle of broadcast/default slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE                        2 //slotframe handle of unicast slotframe
#define ALICE_COMMON_SF_HANDLE                              TSCH_SCHED_COMMON_SF_HANDLE
#define ALICE_UNICAST_SF_HANDLE                             TSCH_SCHED_UNICAST_SF_HANDLE
//
#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_OST /* OST configuration */
#define WITH_OST                                            1
#define OST_ON_DEMAND_PROVISION                             1
#define ORCHESTRA_CONF_RULES                                ORCHESTRA_RULE_OST
#define ORCHESTRA_CONF_EBSF_PERIOD                          397 // EB, original: 397
#define ORCHESTRA_CONF_UNICAST_PERIOD                       47 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD                 19 // broadcast and default slotframe length, original: 31
//
#define OST_HANDLE_QUEUED_PACKETS                           1
#define WITH_OST_LOG_INFO                                   0
#define WITH_OST_LOG_DBG                                    0
#define WITH_OST_LOG_NBR                                    0
#define WITH_OST_LOG_SCH                                    0
#define WITH_OST_TODO                                       0 /* check ost_pigg1 of EB later */
//
#define OST_N_SELECTION_PERIOD                              15 // related to OST_N_MAX: Min. traffic load = 1 / (OST_N_SELECTION_PERIOD * 100) pkt/slot (when num_tx = 1). 
#define OST_N_MAX                                           8 // max t_offset 65535-1, 65535 is used for no-allocation
#define OST_MORE_UNDER_PROVISION                            1 // more allocation 2^OST_MORE_UNDER_PROVISION times than under-provision
#define OST_N_OFFSET_NEW_TX_REQUEST                         100 // Maybe used for denial message
#define PRR_THRES_TX_CHANGE                                 70
#define NUM_TX_MAC_THRES_TX_CHANGE                          20
#define NUM_TX_FAIL_THRES                                   5
#define OST_THRES_CONSEQUTIVE_N_INC                         3
#define OST_T_OFFSET_ALLOCATION_FAILURE                     ((1 << OST_N_MAX) + 1)
#define OST_T_OFFSET_CONSECUTIVE_NEW_TX_REQUEST             ((1 << OST_N_MAX) + 2)
#define OST_THRES_CONSECUTIVE_NEW_TX_SCHEDULE_REQUEST        10
#define TSCH_SCHEDULE_CONF_MAX_SLOTFRAMES                   (3 + 4 * MAX_NBR_NODE_NUM + 2) /* EB, CS, RBUC, Periodic, Ondemand, 2 for spare */
#define TSCH_SCHEDULE_CONF_MAX_LINKS                        (3 + 1 + MAX_NBR_NODE_NUM + 4 * MAX_NBR_NODE_NUM + 2) /* EB (2), CS (1), RBUC (1 + NODE_NUM), Periodic, Ondemand, 2 for spare */
#define SSQ_SCHEDULE_HANDLE_OFFSET                          (2 * NODE_NUM + 2) /* End of the periodic slotframe (Under-provision uses up to 2*NODE_NUM+2) */
//
#define TSCH_SCHED_EB_SF_HANDLE                             0 //slotframe handle of EB slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE                        1 //slotframe handle of unicast slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                         2 //slotframe handle of broadcast/default slotframe
#define OST_PERIODIC_SF_ID_OFFSET                           2
#define OST_ONDEMAND_SF_ID_OFFSET                           SSQ_SCHEDULE_HANDLE_OFFSET
/* OST only */
#undef TSCH_LOG_CONF_QUEUE_LEN
#define TSCH_LOG_CONF_QUEUE_LEN                             16 // originally 16
#define TSCH_CONF_RX_WAIT                                   800 /* ignore too late packets */
#define OST_NODE_ID_FROM_IPADDR(addr)                       ((((addr)->u8[14]) << 8) | (addr)->u8[15])
#define OST_NODE_ID_FROM_LINKADDR(addr)                     ((((addr)->u8[LINKADDR_SIZE - 2]) << 8) | (addr)->u8[LINKADDR_SIZE - 1]) 
#endif /* CURRENT_TSCH_SCHEDULER */
#endif /* !TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL */

/***************************************************************
 * Default burst transmission (DBT)
 ****************************************************************/
#define WITH_TSCH_DEFAULT_BURST_TRANSMISSION                0
#if WITH_TSCH_DEFAULT_BURST_TRANSMISSION
#define TSCH_CONF_BURST_MAX_LEN                             16 /* turn burst on */
#define MODIFIED_TSCH_DEFAULT_BURST_TRANSMISSION            1
#define ENABLE_MODIFIED_DBT_LOG                             0
#define TSCH_DBT_TEMPORARY_LINK                             1
#define TSCH_DBT_HANDLE_SKIPPED_DBT_SLOT                    1
#define TSCH_DBT_HANDLE_MISSED_DBT_SLOT                     1
#define TSCH_DBT_HOLD_CURRENT_NBR                           1
#define TSCH_DBT_QUEUE_AWARENESS                            1
//
#if WITH_ALICE
#undef TSCH_SCHEDULE_CONF_MAX_LINKS
#define TSCH_SCHEDULE_CONF_MAX_LINKS                        (3 + 4 * MAX_NBR_NODE_NUM + 2) /* EB SF: tx/rx, CS SF: one link, UC SF: tx/rx for each node + 2 for spare */
#define ALICE_AFTER_LASTLY_SCHEDULED_ASFN_SF_HANDLE         3
#define ENABLE_LOG_ALICE_DBT_OPERATION                      0
#endif
//
#else /* WITH_TSCH_DEFAULT_BURST_TRANSMISSION */
#define TSCH_CONF_BURST_MAX_LEN                             0 /* turn burst off */
#endif /* WITH_TSCH_DEFAULT_BURST_TRANSMISSION */


/***************************************************************
 * HCK's TSCH scheduler add-on implementation: A3
 ****************************************************************/
#define WITH_A3                                             0
#if WITH_A3
#undef ORCHESTRA_CONF_UNICAST_PERIOD
#define ORCHESTRA_CONF_UNICAST_PERIOD                       20 // 20, 40
//
#if WITH_ALICE
#undef TSCH_SCHEDULE_CONF_MAX_LINKS
#define TSCH_SCHEDULE_CONF_MAX_LINKS                        (3 + 2 * MAX_NBR_NODE_NUM) /* EB SF: tx/rx, CS SF: one link, UC SF: tx/rx for each node + 2 for spare */
#endif
//
#define A3_ALICE1_ORB2_OSB3                                 1
#define A3_MAX_ZONE                                         4 // 2, 4, 8

#define A3_INITIAL_NUM_OF_SLOTS                             1
#define A3_INITIAL_NUM_OF_PKTS                              0

#define A3_INITIAL_TX_ATTEMPT_RATE_EWMA                     (0.5)
#define A3_INITIAL_RX_ATTEMPT_RATE_EWMA                     (0.5)
#define A3_INITIAL_TX_SUCCESS_RATE_EWMA                     (0.4)

#define A3_TX_INCREASE_THRESH                               (0.75)
#define A3_TX_DECREASE_THRESH                               (0.34) //0.36 in code, 0.34 in paper
#define A3_RX_INCREASE_THRESH                               (0.65)
#define A3_RX_DECREASE_THRESH                               (0.29)
#define A3_MAX_ERR_PROB                                     (0.5)

#define A3_DBG                                              1
#define A3_DBG_VALUE                                        0
#endif


/***************************************************************
 * RADIO configuration
 ****************************************************************/
#if WITH_ITOLAB
/* m17dBm, m12dBm, m9dBm, m7dBm, m5dBm, m4dBm, m3dBm, m2dBm, m1dBm, 
   0dBm, 0_7dBm, 1_3dBm, 1_8dBm, 2_3dBm, 2_8dBm, 3dBm */
#define RF2XX_TX_POWER                                      PHY_POWER_m17dBm
/* m101dBm, m90dBm, m87dBm, m84dBm, m81dBm, m78dBm, m75dBm, m72dBm, 
   m69dBm, m66dBm, m63dBm, m60dBm, m57dBm, m54dBm, m51dBm, m48dBm */
#define RF2XX_RX_RSSI_THRESHOLD                             RF2XX_PHY_RX_THRESHOLD__m87dBm
#endif


/***************************************************************
 * Study on network formation acceleration
 * - Common modifications of Contiki-NG
 * - [TMC'19] Dynamic resource allocation
 * - [TMC'23] TRGB
 * - [Proposed] TOP
 ****************************************************************/
#define NETWORK_FORMATION_ACCELERATION                      1
#if NETWORK_FORMATION_ACCELERATION

/***************************************************************
 * Prerequisite modifications of Contiki-NG for network formation acceleration
 * - Configurations that must be fixed regardless of definitions above
 ****************************************************************/
#define RPL_CONF_DIS_SEND                                   1  /* HCKIM-Eval Turn on/off DIS */
#undef RPL_CONF_WITH_PROBING
#define RPL_CONF_WITH_PROBING                               0 /* Turn on/off RPL probing */
//
#undef HCK_MOD_RPL_CODE_NO_PATH_DAO
#define HCK_MOD_RPL_CODE_NO_PATH_DAO                        1
//
#undef HCK_MOD_TSCH_SYNC_COUNT
#define HCK_MOD_TSCH_SYNC_COUNT                             1
//
#undef HCK_MOD_TSCH_PIGGYBACKING_HEADER_IE_32BITS
#define HCK_MOD_TSCH_PIGGYBACKING_HEADER_IE_32BITS          1
//
#undef HCK_MOD_TSCH_PIGGYBACKING_EB_IE_32BITS
#define HCK_MOD_TSCH_PIGGYBACKING_EB_IE_32BITS              1
//
#undef TSCH_SCHEDULE_CONF_DEFAULT_LENGTH
#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH                   101 /* HCKIM-Eval - 11, 29, 47, 67, 83, 101 */
/* Prime number list
 * 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 
 * 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 
 * 73, 79, 83, 89, 97, 101
*/
#undef TSCH_CONF_MAX_EB_PERIOD
#define TSCH_CONF_MAX_EB_PERIOD                             (16 * CLOCK_SECOND) /* HCKIM-Eval */

/***************************************************************
 * Prerequisite/common logging messages for network formation acceleration
 ****************************************************************/
#undef HCK_LOG
#define HCK_LOG                                             1
#undef HCK_LOG_TSCH_SLOT
#define HCK_LOG_TSCH_SLOT                                   1
#undef HCK_LOG_TSCH_SLOT_APP_SEQNO
#define HCK_LOG_TSCH_SLOT_APP_SEQNO                         1

/***************************************************************
 * Prerequisite macros for network formation acceleration study
 * - HCK_FORMATION_PACKET_TYPE_INFO and 
 * - HCK_FORMATION_BOOTSTRAP_STATE_INFO require
 * - HCK_MOD_TSCH_PIGGYBACKING_HEADER_IE_32BITS and 
 * - HCK_MOD_TSCH_PIGGYBACKING_EB_IE_32BITS to be 1
 ****************************************************************/
#define HCK_FORMATION_PACKET_TYPE_INFO                      1
#define HCK_FORMATION_BOOTSTRAP_STATE_INFO                  1

/***************************************************************
 * Dynamic resource allocation implementation - WITH_DRA
 ****************************************************************/
#define WITH_DRA                                            0 /* HCKIM-Eval */
#if WITH_DRA
#define DRA_LOG                                             1
#define DRA_DBG                                             0

#define DRA_SLOTFRAME_HANDLE                                0
#define DRA_SLOTFRAME_LENGTH                                TSCH_SCHEDULE_CONF_DEFAULT_LENGTH

#define DRA_NBR_NUM                                         MAX_NBR_NODE_NUM
#define TSCH_SCHEDULE_CONF_MAX_LINKS                        64
#define DRA_MINIMUM_INTER_SLOT_INTERVAL                     2
#endif /* WITH_DRA */

/***************************************************************
 * TRGB implementation - WITH_TRGB
 ****************************************************************/
#define WITH_TRGB                                           0 /* HCKIM-Eval */
#if WITH_TRGB
#define TRGB_LOG                                            1
#define TRGB_DBG                                            0

#define TRGB_SLOTFRAME_HANDLE                               0
#define TRGB_SLOTFRAME_LENGTH                               TSCH_SCHEDULE_CONF_DEFAULT_LENGTH

#define TRGB_ROOT_ID                                        1
#define TRGB_NUM_OF_CHANNEL                                 16 /* HCKIM-Eval - 4, 16*/
#undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
#if TRGB_NUM_OF_CHANNEL == 4
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE                  TSCH_HOPPING_SEQUENCE_4_4
#elif TRGB_NUM_OF_CHANNEL == 16
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE                  TSCH_HOPPING_SEQUENCE_16_16
#endif
#undef TSCH_CONF_MAX_EB_PERIOD
#define TSCH_CONF_MAX_EB_PERIOD                             (4 * CLOCK_SECOND) /* HCKIM-Eval (16 * CLOCK_SECOND) */
#endif /* WITH_TRGB */

/***************************************************************
 * Quick6TiSCH implementation - WITH_QUICK6
 ****************************************************************/
#define WITH_QUICK6                                         1 /* HCKIM-Eval */
#if WITH_QUICK6
#define QUICK6_LOG                                          1
#define QUICK6_DBG                                          0 /* TODO: further optimization */

#define QUICK6_SLOTFRAME_LENGTH                             TSCH_SCHEDULE_CONF_DEFAULT_LENGTH
#define QUICK6_SLOTFRAME_HANDLE                             0

/* Quick6TiSCH offset configuration - up to five offsets (the sixth offset is available only for broadcast packets) */
#define QUICK6_NUM_OF_OFFSETS                               5    /* 0, 1, 2, 3, 4 */
#define QUICK6_TIMING_CCA_OFFSET                            800  /* */
#define QUICK6_TIMING_TX_OFFSET                             1300 /* 1300 ~ 3100 */
#define QUICK6_TIMING_RX_OFFSET_LEFT                        500  /* 800  ~ 1300, 500 left margin from the tx timing */
#define QUICK6_TIMING_RX_OFFSET_RIGHT                       2900 /* 1300 ~ 4200, 500 right margin from the tx timing */
#define QUICK6_TIMING_INTER_OFFSET_INTERVAL                 600

/* Packet criticality-based prioritization */
#define QUICK6_PRIORITIZATION_CRITICALITY_BASED             1 /* HCKIM-Eval */
#define QUICK6_PRIORITIZATION_EB_DIO_CRITICAL_THRESH        2 /* Up to two packets */ /* HCKIM-Eval */
//
#define QUICK6_PRIORITIZATION_CRITICALITY_BASED_TWO_TIER    0 /* HCKIM-Eval */
#define QUICK6_PRIORITIZATION_CRITICALITY_BASED_RANDOM      1 /* HCKIM-Eval */
//
#define QUICK6_CRITICALITY_BASED_PACKET_SELECTION           1 /* HCKIM-Eval */

/* Packet postponement-based prioritization */
#define QUICK6_PRIORITIZATION_POSTPONEMENT_BASED            1
#define QUICK6_POSTPONEMENT_BASED_THRESH                    (TSCH_MAC_MAX_FRAME_RETRIES + 1)
#define QUICK6_POSTPONEMENT_BASED_SCALING_FACTOR            1 /* HCKIM-Eval */

/* Quick6TiSCH supplementary features - HCKIM-Eval */
#define QUICK6_NO_TX_COUNT_INCREASE_FOR_POSTPONED_PACKETS   1
#define QUICK6_BACKOFF_FOR_BCAST_PACKETS                    1 /* Backoff policy for postponed packets, QUICK-TODO: Need to distinguish EB/broadcast nbrs */
#define QUICK6_TSCH_MAC_BCAST_MAX_BE                        5
#define QUICK6_PER_SLOTFRAME_BACKOFF                        1 /* Slotframe level backoff for contention mitigation */
#define QUICK6_PER_SLOTFRAME_BACKOFF_MIN_BE                 1 /* Slotframe level backoff configuration */
#define QUICK6_PER_SLOTFRAME_BACKOFF_MAX_BE                 5 /* Slotframe level backoff configuration */
#define QUICK6_DUPLICATE_PACKET_MANAGEMENT                  1 /* Replace duplicate packet within neighbor queue, while maintaining the position within the ringbuf.*/

#endif /* WITH_QUICK6 */

#endif /* NETWORK_FORMATION_ACCELERATION */

#endif /* PROJECT_CONF_H_ */ 
