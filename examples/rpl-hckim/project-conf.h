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
#define HCK_MOD_TSCH_DROP_PACKET_FROM_UCSF                  1
#define HCK_MOD_TSCH_OFFLOAD_PACKET_FROM_UCSF_TO_CSSF       0
#define HCK_MOD_TSCH_OFFLOAD_PACKET_FROM_CSSF_TO_UCSF       1
#define HCK_MOD_TSCH_SYNC_COUNT                             1
#define HCK_MOD_TSCH_PACKET_TYPE_INFO                       0
#define HCK_MOD_TSCH_DEACTIVATE_RADIO_INTERRUPT_MODE        1
#define HCK_MOD_TSCH_SWAP_TX_RX_PROCESS_PENDING             1 /* swap order of rx_process_pending and tx_process_pending */
#define HCK_MOD_TSCH_FILTER_PACKETS_WITH_INVALID_RX_TIMING  1
#define HCK_MOD_TSCH_HANDLE_OVERFULL_SLOT_OPERATION         1
//
#define HCK_MOD_ORCHESTRA_NO_PATH_DAO_HANDLING              1
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
#define HCK_LOG_TSCH_SLOT_RX_OPERATION                      1
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
 * HCK's debugging implementation
 ****************************************************************/
#define HCK_DBG_REGULAR_SLOT_DETAIL                0
#define HCK_DBG_REGULAR_SLOT_TIMING                (0 && HCK_DBG_REGULAR_SLOT_DETAIL)
#define HCK_DBG_ALICE_RESCHEDULE_INTERVAL          0


/***************************************************************
 * HCK's topology configuration
 ****************************************************************/
#define WITH_COOJA                                          0
#define WITH_IOTLAB                                         1
//
#if WITH_COOJA
//
#elif WITH_IOTLAB
#define IOTLAB_FIXED_TOPOLOGY                               0
//
#define IOTLAB_GRENOBLE_79_L_CORNER_U                       1 /* 79 nodes */
#define IOTLAB_GRENOBLE_78_R_CORNER_U                       2 /* 78 nodes */
#define IOTLAB_LILLE_79_CORNER                              3 /* 79 nodes */
#define IOTLAB_LILLE_2_CORNER                               4 /* 2 nodes */
//
#define IOTLAB_SITE                                         IOTLAB_LILLE_79_CORNER
//
#if IOTLAB_SITE == IOTLAB_GRENOBLE_79_L_CORNER_U
#define NODE_NUM                                            79
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_78_R_CORNER_U
#define NODE_NUM                                            78
#elif IOTLAB_SITE == IOTLAB_LILLE_79_CORNER
#define NODE_NUM                                            79
#elif IOTLAB_SITE == IOTLAB_LILLE_2_CORNER
#define NODE_NUM                                            2
#endif
#endif


/***************************************************************
 * HCK's configuration for Contiki-NG system
 ****************************************************************/
#define IEEE802154_CONF_PANID                      0x58FA /* 22782 hckim */
#define MAX_NBR_NODE_NUM                           60
#define NBR_TABLE_CONF_MAX_NEIGHBORS               (MAX_NBR_NODE_NUM + 2) /* Add 2 for EB and broadcast neighbors in TSCH layer */


/***************************************************************
 * HCK's configuration for pplication layer (examples/rpl-hckim)
 ****************************************************************/
#if WITH_COOJA

#elif WITH_IOTLAB
#define WITH_UPWARD_TRAFFIC                        0
#define APP_UPWARD_SEND_INTERVAL                   (1 * 60 * CLOCK_SECOND / 4)
#define WITH_DOWNWARD_TRAFFIC                      0
#define APP_DOWNWARD_SEND_INTERVAL                 (1 * 60 * CLOCK_SECOND / 4)
#define APP_PRINT_NODE_INFO_DELAY                  (1 * 60 * CLOCK_SECOND / 2)
/* Bootstrap timing configurations */
#if IOTLAB_FIXED_TOPOLOGY
#define APP_RESET_BEFORE_DATA_DELAY                (5 * 60 * CLOCK_SECOND)
#define APP_DATA_START_DELAY                       (6 * 60 * CLOCK_SECOND)
#define APP_DATA_PERIOD                            (30 * 60 * CLOCK_SECOND)
#define APP_PRINT_LOG_DELAY                        (37 * 60 * CLOCK_SECOND)   /* APP_DATA_START_DELAY + APP_DATA_PERIOD + 1 */
#define APP_PRINT_LOG_PERIOD                       (1 * 60 * CLOCK_SECOND / 4)
#else /* IOTLAB_FIXED_TOPOLOGY */
#define APP_RESET_BEFORE_DATA_DELAY                (730 * 60 * CLOCK_SECOND)
#define APP_DATA_START_DELAY                       (731 * 60 * CLOCK_SECOND)
#define APP_DATA_PERIOD                            (60 * 60 * CLOCK_SECOND)
#define APP_PRINT_LOG_DELAY                        (1 * 60 * CLOCK_SECOND)    /* APP_DATA_START_DELAY + APP_DATA_PERIOD + 2 */
#define APP_PRINT_LOG_PERIOD                       (1 * 60 * CLOCK_SECOND)
#endif /* IOTLAB_FIXED_TOPOLOGY */
#endif /* WITH_COOJA or WITH_IOTLAB */
//
#define APP_UPWARD_MAX_TX                          (APP_DATA_PERIOD / APP_UPWARD_SEND_INTERVAL)
#define APP_DOWNWARD_MAX_TX                        (APP_DATA_PERIOD / APP_DOWNWARD_SEND_INTERVAL)
/* 
   App payload size
   - Orchestra or ALICE
      - Wihtout any: Max in single hop: 87, Max in multi hop: 70
      - With UPA: Max in single hop: 81, Max in multi hop: 64 
   OST
      - Without any: Max in single hop: 85, Max in multi hop: 58
      - With UPA: Max in single hop: 79, Max in multi hop: ???
      - With ODP: ???
*/
#define APP_PAYLOAD_LEN                            14
#define APP_DATA_MAGIC                             0x58FA


/***************************************************************
 * HCK's configuration for IPv6, 6LoWPAN, and RPL layers
 ****************************************************************/
/* IPv6 and 6LoWPAN layers */
#define UIP_CONF_BUFFER_SIZE                       160
#define UIP_CONF_MAX_ROUTES                        (NODE_NUM)
#define SICSLOWPAN_CONF_FRAG                       0
/* RPL layer */
#define RPL_CONF_MOP                               RPL_MOP_STORING_NO_MULTICAST
#if IOTLAB_FIXED_TOPOLOGY
#define RPL_CONF_WITH_DAO_ACK                      0
#define RPL_CONF_DEFAULT_LIFETIME                  RPL_INFINITE_LIFETIME
#else /* IOTLAB_FIXED_TOPOLOGY */
#define RPL_CONF_WITH_DAO_ACK                      1
#endif /* IOTLAB_FIXED_TOPOLOGY */
#define RPL_CONF_WITH_PROBING                      1
#define RPL_CONF_PROBING_INTERVAL                  (2 * 60 * CLOCK_SECOND) /* originally 60 seconds */
#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT        (20 * CLOCK_SECOND) /* originally 5 seconds */
#define RPL_CONF_PARENT_SWITCH_THRESHOLD           96 /* 96 (0.75, default), 127 (0.99), 128 (1) */
#define RPL_MRHOF_CONF_SQUARED_ETX                 0
#define LINK_STATS_CONF_INIT_ETX_FROM_RSSI         1 /* originally 1 */


/***************************************************************
 * HCK's configuration for TSCH layer
 ****************************************************************/
#define HCK_TSCH_TIMESLOT_LENGTH                   10000
#define QUEUEBUF_CONF_NUM                          16 /* 16 in Orchestra, ALICE, and OST, originally 8 */
#define TSCH_CONF_MAX_INCOMING_PACKETS             8 /* 8 in OST, originally 4 */
#define TSCH_CONF_CCA_ENABLED                      1
//#define TSCH_CONF_AUTOSTART                        0
//#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH          3
#ifndef WITH_SECURITY
#define WITH_SECURITY                              0
#endif /* WITH_SECURITY */
#define TSCH_LOG_CONF_QUEUE_LEN                    128 // originally 16


/***************************************************************
 * HCK's TSCH scheduler implementation
 ****************************************************************/
#define TSCH_SCHEDULER_NB_ORCHESTRA                1 // 1: NB-Orchestra-storing
#define TSCH_SCHEDULER_LB_ORCHESTRA                2 // 2: LB-Orchestra
#define TSCH_SCHEDULER_ALICE                       3 // 3: ALICE
#define TSCH_SCHEDULER_OST                         4 // 4: OST
//
#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_ALICE
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
/* Neighbor-based Orchestra configuration */
#if CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_NB_ORCHESTRA
#define WITH_ORCHESTRA                             1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_NB // neighbor-storing
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 // 0: receiver-based, 1: sender-based
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              17 //unicast, 7, 11, 13, 17, 19, 23, 31, 43, 47, 59, 67, 71
#define TSCH_SCHED_EB_SF_HANDLE                    0 //slotframe handle of EB slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE               1 //slotframe handle of unicast slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                2 //slotframe handle of broadcast/default slotframe
/* Link-based Orchestra configuration */
#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_LB_ORCHESTRA
#define WITH_ORCHESTRA                             1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_LB //link-based Orchestra
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              11 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71
#define TSCH_SCHED_EB_SF_HANDLE                    0 //slotframe handle of EB slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE               1 //slotframe handle of unicast slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                2 //slotframe handle of broadcast/default slotframe
/* ALICE configuration */
#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_ALICE
#define WITH_ALICE                                 1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_ALICE
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 //1: sender-based, 0:receiver-based
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        31 //19, 31, 41, 53, 61, ... 101 // broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              20 // unicast, should be longer than (2N-2)/3 to provide contention-free links
#define ALICE_PACKET_CELL_MATCHING_ON_THE_FLY      alice_packet_cell_matching_on_the_fly
#define ALICE_TIME_VARYING_SCHEDULING              alice_time_varying_scheduling
#define ALICE_EARLY_PACKET_DROP                    0
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (3 + 2 * MAX_NBR_NODE_NUM + 2) /* EB SF: tx/rx, CS SF: one link, UC SF: tx/rx for each node + 2 for spare */
#define ENABLE_ALICE_PACKET_CELL_MATCHING_LOG      0
#define ENABLE_ALICE_EARLY_PACKET_DROP_LOG         0
#undef HCK_LOG_TSCH_LINK_ADD_REMOVE
#define HCK_LOG_TSCH_LINK_ADD_REMOVE               0
#define ENABLE_LOG_ALICE_LINK_ADD_REMOVE           0
#define TSCH_SCHED_EB_SF_HANDLE                    0 //slotframe handle of EB slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                1 //slotframe handle of broadcast/default slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE               2 //slotframe handle of unicast slotframe
#define ALICE_COMMON_SF_HANDLE                     TSCH_SCHED_COMMON_SF_HANDLE
#define ALICE_UNICAST_SF_HANDLE                    TSCH_SCHED_UNICAST_SF_HANDLE
/* OST configuration */
#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_OST
#define WITH_OST                                   1
#define OST_ON_DEMAND_PROVISION                    1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_OST
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_UNICAST_PERIOD              47 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 // broadcast and default slotframe length, original: 31
//
#define OST_HANDLE_QUEUED_PACKETS                  1
#define WITH_OST_LOG_INFO                          0
#define WITH_OST_LOG_DBG                           0
#define WITH_OST_LOG_NBR                           0
#define WITH_OST_LOG_SCH                           0
#define WITH_OST_TODO                              0 /* check ost_pigg1 of EB later */
//
#define OST_N_SELECTION_PERIOD                     15 // related to OST_N_MAX: Min. traffic load = 1 / (OST_N_SELECTION_PERIOD * 100) pkt/slot (when num_tx = 1). 
#define OST_N_MAX                                  8 // max t_offset 65535-1, 65535 is used for no-allocation
#define OST_MORE_UNDER_PROVISION                   1 // more allocation 2^OST_MORE_UNDER_PROVISION times than under-provision
#define OST_N_OFFSET_NEW_TX_REQUEST                100 // Maybe used for denial message
#define PRR_THRES_TX_CHANGE                        70
#define NUM_TX_MAC_THRES_TX_CHANGE                 20
#define NUM_TX_FAIL_THRES                          5
#define OST_THRES_CONSEQUTIVE_N_INC                3
#define OST_T_OFFSET_ALLOCATION_FAILURE            ((1 << OST_N_MAX) + 1)
#define OST_T_OFFSET_CONSECUTIVE_NEW_TX_REQUEST    ((1 << OST_N_MAX) + 2)
#define OST_THRES_CONSECUTIVE_NEW_TX_SCHEDULE_REQUEST           10
#define TSCH_SCHEDULE_CONF_MAX_SLOTFRAMES          (3 + 4 * MAX_NBR_NODE_NUM + 2) /* EB, CS, RBUC, Periodic, Ondemand, 2 for spare */
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (3 + 1 + MAX_NBR_NODE_NUM + 4 * MAX_NBR_NODE_NUM + 2) /* EB (2), CS (1), RBUC (1 + NODE_NUM), Periodic, Ondemand, 2 for spare */
#define SSQ_SCHEDULE_HANDLE_OFFSET                 (2 * NODE_NUM + 2) /* End of the periodic slotframe (Under-provision uses up to 2*NODE_NUM+2) */
//
#define TSCH_SCHED_EB_SF_HANDLE                    0 //slotframe handle of EB slotframe
#define TSCH_SCHED_UNICAST_SF_HANDLE               1 //slotframe handle of unicast slotframe
#define TSCH_SCHED_COMMON_SF_HANDLE                2 //slotframe handle of broadcast/default slotframe
#define OST_PERIODIC_SF_ID_OFFSET                  2
#define OST_ONDEMAND_SF_ID_OFFSET                  SSQ_SCHEDULE_HANDLE_OFFSET
/* OST only */
#undef TSCH_LOG_CONF_QUEUE_LEN
#define TSCH_LOG_CONF_QUEUE_LEN                    16 // originally 16
#define TSCH_CONF_RX_WAIT                          800 /* ignore too late packets */
#define OST_NODE_ID_FROM_IPADDR(addr)              ((((addr)->u8[14]) << 8) | (addr)->u8[15])
#define OST_NODE_ID_FROM_LINKADDR(addr)            ((((addr)->u8[LINKADDR_SIZE - 2]) << 8) | (addr)->u8[LINKADDR_SIZE - 1]) 
#endif /* CURRENT_TSCH_SCHEDULER */


/***************************************************************
 * RADIO configuration
 ****************************************************************/
#if WITH_ITOLAB
/* m17dBm, m12dBm, m9dBm, m7dBm, m5dBm, m4dBm, m3dBm, m2dBm, m1dBm, 
   0dBm, 0_7dBm, 1_3dBm, 1_8dBm, 2_3dBm, 2_8dBm, 3dBm */
#define RF2XX_TX_POWER                             PHY_POWER_m17dBm
/* m101dBm, m90dBm, m87dBm, m84dBm, m81dBm, m78dBm, m75dBm, m72dBm, 
   m69dBm, m66dBm, m63dBm, m60dBm, m57dBm, m54dBm, m51dBm, m48dBm */ //
#define RF2XX_RX_RSSI_THRESHOLD                    RF2XX_PHY_RX_THRESHOLD__m87dBm
#endif




/***************************************************************
 * Network formation acceleration
 * - Common modifications of Contiki-NG
 * - [TMC'19] Dynamic resource allocation
 * - [TMC'23] TRGB
 * - [Proposed] TOP
 ****************************************************************/
#define NETWORK_FORMATION_ACCELERATION                      0
#if NETWORK_FORMATION_ACCELERATION

/***************************************************************
 * Prerequisite modifications of Contiki-NG for network formation acceleration
 ****************************************************************/
#undef HCK_MOD_TSCH_SYNC_COUNT
#define HCK_MOD_TSCH_SYNC_COUNT                             1
#undef HCK_MOD_TSCH_OFFLOAD_PACKET_FROM_CSSF_TO_UCSF
#define HCK_MOD_TSCH_OFFLOAD_PACKET_FROM_CSSF_TO_UCSF       1
#undef HCK_MOD_TSCH_PACKET_TYPE_INFO
#define HCK_MOD_TSCH_PACKET_TYPE_INFO                       1
#undef HCK_MOD_RPL_CODE_NO_PATH_DAO
#define HCK_MOD_RPL_CODE_NO_PATH_DAO                        1

/***************************************************************
 * Prerequisite/common logging messages for network formation acceleration
 ****************************************************************/
#undef HCK_LOG
#define HCK_LOG                                             1
#undef HCK_LOG_TSCH_SLOT
#define HCK_LOG_TSCH_SLOT                                   1
#undef HCK_LOG_TSCH_SLOT_APP_SEQNO
#define HCK_LOG_TSCH_SLOT_APP_SEQNO                         1
#define FORMATION_COMMON_LOG                                1

/***************************************************************
 * TRGB implementation - WITH_TRGB
 ****************************************************************/
#define WITH_TRGB                                  0
#if WITH_TRGB
#define TRGB_LOG                                   1
#define TRGB_DBG                                   0
#endif /* WITH_TRGB */

/***************************************************************
 * TOP implementation - WITH_HNEXT
 ****************************************************************/
#define WITH_HNEXT                                 0
#if WITH_HNEXT
#define HNEXT_LOG                                  1
#define HNEXT_DBG                                  0
/* HNEXT modules */
#define HNEXT_PACKET_URGENCY_DETERMINATION         1
#define HNEXT_PACKET_OFFSET_ASSIGNMENT             1
#define HNEXT_PACKET_OFFSET_ESCALATION             1
#define HNEXT_PRIORITY_BASED_PACKET_SELECTION      1
/* HNEXT offset configuration - up to six offsets */
#define HNEXT_CCA_OFFSET                           800
#define HNEXT_TX_OFFSET                            1300 /* 1300 ~ 3100 */
#define HNEXT_RX_OFFSET_LEFT                       500  /* 800  ~ 1300, 500 margin */
#define HNEXT_RX_OFFSET_RIGHT                      2900 /* 1300 ~ 4200, 500 margin */
#define HNEXT_OFFSET_GAP                           600
/* Offset assignment policy */
#define HNEXT_OFFSET_ASSIGNMENT_POLICY             HNEXT_OFFSET_ASSIGNMENT_POLICY_1 /* Fix to 2 or 4 */ ////////////////////
#define HNEXT_OFFSET_ASSIGNMENT_POLICY_0           0 /* Baseline */
#define HNEXT_OFFSET_ASSIGNMENT_POLICY_1           1 /* Random */
#define HNEXT_OFFSET_ASSIGNMENT_POLICY_2           2 /* BC / UC / BC / UC */ /* HNEXT TODO */
#define HNEXT_OFFSET_ASSIGNMENT_POLICY_3           3 /* BC / UC / UC / BC */ /* HNEXT TODO */
#define HNEXT_OFFSET_ASSIGNMENT_POLICY_4           4 /* BC / UC / UC / BC / UC */ /* HNEXT TODO */

#define HNEXT_OFFSET_ASSIGNMENT_POLICY_1_OFFSETS   5
#define HNEXT_OFFSET_ASSIGNMENT_FIXED_EB_OFFSET    1
#define HNEXT_OFFSET_ASSIGNMENT_DYNAMIC_DIO_OFFSET 1
#define HNEXT_OFFSET_ASSIGNMENT_FIXED_DIS_OFFSET   1

#define HNEXT_OFFSET_ASSIGNMENT_DIO_INT_THRESH     2

/* Offset escalation policy for postponed packets */
#define HNEXT_OFFSET_ESCALATION_POLICY             HNEXT_OFFSET_ESCALATION_POLICY_2 /* Fix to 2 */ ////////////////////
#define HNEXT_OFFSET_ESCALATION_POLICY_0           0
#define HNEXT_OFFSET_ESCALATION_POLICY_1           1 /* Naive implementation */
#define HNEXT_OFFSET_ESCALATION_POLICY_2           2 /* Bcast packets only */
#define HNEXT_OFFSET_ESCALATION_POLICY_3           3 /* Ucast packets too */
#define HNEXT_OFFSET_ESCALATION_POLICY_4           4 /* All gradual increment */

#if (HNEXT_OFFSET_ASSIGNMENT_POLICY == HNEXT_OFFSET_ASSIGNMENT_POLICY_0) || \
    (HNEXT_OFFSET_ASSIGNMENT_POLICY == HNEXT_OFFSET_ASSIGNMENT_POLICY_1)  
#undef HNEXT_OFFSET_ESCALATION_POLICY
#define HNEXT_OFFSET_ESCALATION_POLICY             HNEXT_OFFSET_ESCALATION_POLICY_0
#endif

/* Offset-based packet selection policy */
#define HNEXT_PACKET_SELECTION                     1

#if (HNEXT_OFFSET_ASSIGNMENT_POLICY == HNEXT_OFFSET_ASSIGNMENT_POLICY_0) || \
    (HNEXT_OFFSET_ASSIGNMENT_POLICY == HNEXT_OFFSET_ASSIGNMENT_POLICY_1)  
#undef HNEXT_PACKET_SELECTION
#define HNEXT_PACKET_SELECTION                     0
#endif

/* Backoff policy for postponed packets */
#define HNEXT_TSCH_MAC_BCAST_MAX_BE                5
#define HNEXT_TEMP_RANDOM_RX                       0

#define HNEXT_POSTPONED_BACKOFF_POLICY             HNEXT_POSTPONED_BACKOFF_POLICY_1 /* Fix to 1 */ ////////////////////
#define HNEXT_POSTPONED_BACKOFF_POLICY_0           0
#define HNEXT_POSTPONED_BACKOFF_POLICY_1           1

#if HNEXT_OFFSET_ASSIGNMENT_POLICY == HNEXT_OFFSET_ASSIGNMENT_POLICY_0
#undef HNEXT_POSTPONED_BACKOFF_POLICY
#define HNEXT_POSTPONED_BACKOFF_POLICY             HNEXT_POSTPONED_BACKOFF_POLICY_0
#endif

#if HNEXT_POSTPONED_BACKOFF_POLICY == HNEXT_POSTPONED_BACKOFF_POLICY_0
#define HNEXT_BACKOFF_FOR_BCAST_PACKETS                        0 /* HNEXT TODO: Need to distinguish EB/broadcast nbrs */
#define HNEXT_NO_TRANS_COUNT_INCREASE_FOR_CSSF_POSTPONED_PACKETS  0

#elif HNEXT_POSTPONED_BACKOFF_POLICY == HNEXT_POSTPONED_BACKOFF_POLICY_1
#define HNEXT_BACKOFF_FOR_BCAST_PACKETS                        1 /* HNEXT TODO: Need to distinguish EB/broadcast nbrs */
#define HNEXT_NO_TRANS_COUNT_INCREASE_FOR_CSSF_POSTPONED_PACKETS  1

#endif /* HNEXT_POSTPONED_BACKOFF_POLICY == HNEXT_POSTPONED_BACKOFF_POLICY_X */
#endif /* WITH_HNEXT */

#endif /* NETWORK_FORMATION_ACCELERATION */




/***************************************************************
 * Network time-efficiency enhancement
 * - Common modifications of Contiki-NG
 * - [IEEE 802.15.4e] Default burst transmission
 * - [IPSN'21] A3
 * - [Proposed] Utility-based packet aggregation (UPA)
 * - [Proposed] Slot length adaptation (SLA)
 ****************************************************************/
#define NETWORK_TIME_EFFICIENCY_ENHANCEMENT                 1
#if NETWORK_TIME_EFFICIENCY_ENHANCEMENT

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
 * UPA implementation
 ****************************************************************/
#define WITH_UPA                                   1
#if WITH_UPA
#define UPA_TRIPLE_CCA                             1
#define UPA_RX_SLOT_POLICY                         1 /* 0: no policy, 1: max gain, 2: max pkts w/ gain */
#define UPA_NO_ETX_UPDATE_FROM_PACKETS_IN_BATCH    0

#define UPA_DBG_ESSENTIAL                          1
#define UPA_DBG_OPERATION                          0
#define UPA_DBG_TIMING_TRIPLE_CCA                  (0 && UPA_TRIPLE_CCA)
#define UPA_DBG_SLOT_TIMING                        0
#endif /* WITH_UPA */


/***************************************************************
 * SLA implementation
 ****************************************************************/
#define WITH_SLA                                   1
#if WITH_SLA
#define SLA_K_TH_PERCENTILE                        90
#define SLA_DBG_ESSENTIAL                          1
#define SLA_DBG_OPERATION                          0
#define SLA_START_DELAY                            (5 * 60 * CLOCK_SECOND)
#define SLA_DETERMINATION_PERIOD                   (5 * 60 * CLOCK_SECOND)
#define SLA_RAPID_EB_PERIOD                        (3 * CLOCK_SECOND)
#define SLA_MAX_FRAME_LEN                          128 /* Including RADIO_PHY_OVERHEAD (3 bytes) */
#define SLA_CALCULATE_DURATION(len)                (32 * (5 + len)) /* len includes RADIO_PHY_OVERHEAD (3 bytes) */
#define HCK_TSCH_MAX_ACK                           SLA_CALCULATE_DURATION(SLA_MAX_ACK_LEN)
#define SLA_SHIFT_BITS                             3


#if WITH_UPA
#define SLA_GUARD_TIME_TIMESLOTS                   6
#else
#define SLA_GUARD_TIME_TIMESLOTS                   2
#endif

#if WITH_UPA
#if UPA_TRIPLE_CCA
#define SLA_MAX_ACK_LEN                            34  /* 2950, 1300, Including RADIO_PHY_OVERHEAD (3 bytes) */
#else
#define SLA_MAX_ACK_LEN                            60  /* 2120, 1300, Including RADIO_PHY_OVERHEAD (3 bytes) */
#endif
#else
#define SLA_MAX_ACK_LEN                            70  /* 2120, 1000, Including RADIO_PHY_OVERHEAD (3 bytes) */
#endif

#define SLA_OBSERVATION_WINDOWS                    1
#define SLA_FRAME_LEN_QUANTIZED_LEVELS             ((((SLA_MAX_FRAME_LEN - 1) >> SLA_SHIFT_BITS) + 1) + 1)
#define SLA_ACK_LEN_QUANTIZED_LEVELS               ((((SLA_MAX_ACK_LEN - 1) >> SLA_SHIFT_BITS) + 1) + 1)

#define SLA_INITIAL_HOP_DISTANCE                   10

#define SLA_ZERO_HOP_DISTANCE_OFFSET               1
#define SLA_TRIGGERING_ASN_INCREMENT               1
#define SLA_TRIGGERING_ASN_MULTIPLIER              1
#endif
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*
 * ASAP
 */
#define ASAP_DBG_SLOT_END                          (0 || UPA_DBG_SLOT_TIMING)



/*---------------------------------------------------------------------------*/
/*
 * Evaluation orientd configurations
 */
#define HCK_ASAP_EVAL_01_SLA_REAL_TIME             0
#if HCK_ASAP_EVAL_01_SLA_REAL_TIME
#define APP_PAYLOAD_MAX_LEN                        63
#define APP_PAYLOAD_MIN_LEN                        14
#define APP_PAYLOAD_LEN_FIRST                      APP_PAYLOAD_MAX_LEN
#define APP_PAYLOAD_LEN_SECOND                     APP_PAYLOAD_MIN_LEN
#define APP_PAYLOAD_LEN_THIRD                      (APP_PAYLOAD_MIN_LEN + 32)
#define APP_PAYLOAD_LEN_FOURTH                     (APP_PAYLOAD_MIN_LEN + 16)

#undef SLA_K_TH_PERCENTILE
#define SLA_K_TH_PERCENTILE                        90

#undef SLA_START_DELAY
#define SLA_START_DELAY                            (46 * 60 * CLOCK_SECOND)
#endif /* HCK_ASAP_EVAL_01_SLA_REAL_TIME */

#define HCK_ASAP_EVAL_02_UPA_SINGLE_HOP            0
#if HCK_ASAP_EVAL_02_UPA_SINGLE_HOP

#undef ORCHESTRA_CONF_UNICAST_PERIOD
#define ORCHESTRA_CONF_UNICAST_PERIOD              10 //unicast, 7, 11, 13, 17, 19, 23, 31, 43, 47, 59, 67, 71

#define APP_PAYLOAD_LEN_MIN                        14 // 14, 36, 58
#define APP_PAYLOAD_LEN_MAX                        80 // 35, 57, 80
#define FIXED_NUM_OF_AGGREGATED_PKTS               0 /* If zero, payload len varies from MIN to MAX */
#define NUM_OF_MAX_AGGREGATED_PKTS                 16
#define NUM_OF_APP_PAYLOAD_LENS                    (APP_PAYLOAD_LEN_MAX - APP_PAYLOAD_LEN_MIN + 1)
#if FIXED_NUM_OF_AGGREGATED_PKTS == 0
#define NUM_OF_PACKETS_PER_EACH_APP_PAYLOAD_LEN    600
#else
#define NUM_OF_PACKETS_PER_EACH_APP_PAYLOAD_LEN    (20 * FIXED_NUM_OF_AGGREGATED_PKTS)
#endif
#define NUM_OF_PACKETS_PER_SECOND                  20
#define DATA_PERIOD_LEN_IN_SECONDS                 (NUM_OF_APP_PAYLOAD_LENS * NUM_OF_PACKETS_PER_EACH_APP_PAYLOAD_LEN / NUM_OF_PACKETS_PER_SECOND)

#undef WITH_UPA
#define WITH_UPA                                   1

#undef UPA_TRIPLE_CCA
#define UPA_TRIPLE_CCA                             1

#undef UPA_RX_SLOT_POLICY
#define UPA_RX_SLOT_POLICY                         0 /* 0: no policy, 1: max gain, 2: max pkts w/ gain */

#undef UPA_DBG_ESSENTIAL
#define UPA_DBG_ESSENTIAL                          1
#undef UPA_DBG_OPERATION
#define UPA_DBG_OPERATION                          0
#undef UPA_DBG_TIMING_TRIPLE_CCA
#define UPA_DBG_TIMING_TRIPLE_CCA                  0
#undef UPA_DBG_SLOT_TIMING
#define UPA_DBG_SLOT_TIMING                        0

#undef HCK_DBG_REGULAR_SLOT_DETAIL
#define HCK_DBG_REGULAR_SLOT_DETAIL                0
#undef HCK_DBG_REGULAR_SLOT_TIMING
#define HCK_DBG_REGULAR_SLOT_TIMING                0

#undef WITH_ASAP
#define WITH_ASAP                                  1
#undef ASAP_DBG_SLOT_END
#define ASAP_DBG_SLOT_END                          0

#undef HCK_MOD_RPL_IGNORE_REDUNDANCY_IN_BOOTSTRAP
#define HCK_MOD_RPL_IGNORE_REDUNDANCY_IN_BOOTSTRAP 0
#undef IOTLAB_FIXED_TOPOLOGY
#define IOTLAB_FIXED_TOPOLOGY                     0

#undef IOTLAB_SITE
#define IOTLAB_SITE                                IOTLAB_GRENOBLE_2
//#define IOTLAB_SITE                                IOTLAB_SACLAY_2
#undef NODE_NUM
#define NODE_NUM                                   2
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES                        (NODE_NUM)

#undef APP_UPWARD_SEND_INTERVAL
#define APP_UPWARD_SEND_INTERVAL                   (1 * 60 * CLOCK_SECOND / 60 / NUM_OF_PACKETS_PER_SECOND)

#undef APP_RESET_BEFORE_DATA_DELAY
#define APP_RESET_BEFORE_DATA_DELAY                (80 * 60 * CLOCK_SECOND)
#undef APP_DATA_START_DELAY
#define APP_DATA_START_DELAY                       (5 * 60 * CLOCK_SECOND)
#undef APP_DATA_PERIOD
#define APP_DATA_PERIOD                            (DATA_PERIOD_LEN_IN_SECONDS * CLOCK_SECOND)

#undef APP_UPWARD_MAX_TX
#define APP_UPWARD_MAX_TX                          (APP_DATA_PERIOD / APP_UPWARD_SEND_INTERVAL)

#undef MAX_NBR_NODE_NUM
#define MAX_NBR_NODE_NUM                           4
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS               (MAX_NBR_NODE_NUM + 2) /* Add 2 for EB and broadcast neighbors in TSCH layer */

#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM                          32 /* 16 in Orchestra, ALICE, and OST, originally 8 */
#undef TSCH_CONF_MAX_INCOMING_PACKETS
#define TSCH_CONF_MAX_INCOMING_PACKETS             32 /* 8 in OST, originally 4 */

#endif /* HCK_ASAP_EVAL_02_UPA_SINGLE_HOP */

#endif /* NETWORK_TIME_EFFICIENCY_ENHANCEMENT */

#endif /* PROJECT_CONF_H_ */ 
