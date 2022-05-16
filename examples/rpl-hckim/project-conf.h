#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*---------------------------------------------------------------------------*/
/*
 * Exclusive period implementation
 */
#define WITH_TSCH_TX_CCA                           1
#define WITH_PPSD                                  1

#define ORCHESTRA_PACKET_OFFLOADING                1

#if WITH_PPSD
#define PPSD_APP_PAYLOAD_LEN_CONTROL               64 /* Max in single hop: 81, Max in multi hop: 64 */

#define PPSD_HEADER_IE_IN_DATA_AND_ACK             1 /* Must be 1 if WITH_PPSD is 1*/
#define PPSD_EP_POLICY_CELL_UTIL                   0
#define PPSD_EP_EXTRA_CHANNELS                     0 /* Extra channel hopping */
#define PPSD_TX_SLOT_FORWARD_OFFLOADING            1
#define PPSD_TX_SLOT_BACKWARD_OFFLOADING           1
#define PPSD_RX_SLOT_FORWARD_OFFLOADING            1
#define PPSD_RX_SLOT_BACKWARD_OFFLOADING           1

#define PPSD_USE_BUSYWAIT                          0

#endif /* WITH_PPSD */

#define PPSD_DBG_SLOT_TIMING                       1
#define PPSD_DBG_EP_SLOT_TIMING                    1
#define PPSD_DBG_EP_ESSENTIAL                      1
#define PPSD_DBG_EP_OPERATION                      0

#if PPSD_EP_EXTRA_CHANNELS
#define TSCH_PPSD_HOPPING_SEQUENCE_4_4 (uint8_t[]){ 11, 16, 19, 21 }
#endif

#if WITH_TSCH_TX_CCA
#define TSCH_CONF_CCA_ENABLED                      1
#define TSCH_TX_CCA_DBG_CCA_STATUS                 0
#define TSCH_TX_CCA_EARLY_TX_NODE                  0
#endif

//#define PPSD_CONF_RX_WAIT                    800
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*
 * Configure testbed site, node num, topology
 */
#define WITH_COOJA                                 0
#define WITH_IOTLAB                                1
#if WITH_COOJA && WITH_IOTLAB
#error "Wrong topoligy setting. See project-conf.h"
#endif

#if WITH_COOJA
#define NODE_NUM                                   5

#elif WITH_IOTLAB

#define IOTLAB_GRENOBLE_83_R_CORNER                1 /* 83 nodes */
#define IOTLAB_GRENOBLE_79_R_CORNER                2 /* 79 nodes */
#define IOTLAB_GRENOBLE_79_L_CORNER                3 /* 79 nodes */
#define IOTLAB_LILLE_79_CORNER                     4 /* 79 nodes */
#define IOTLAB_LILLE_79_CENTER                     5 /* 79 nodes */
#define IOTLAB_LYON_2                              6 /* 2 nodes */
#define IOTLAB_LYON_3                              7 /* 3 nodes */
#define IOTLAB_LYON_8                              8 /* 8 nodes */
#define IOTLAB_LYON_17                             9 /* 17 nodes */

//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_83_R_CORNER
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_79_R_CORNER
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_79_L_CORNER
//#define IOTLAB_SITE                                IOTLAB_LILLE_79_CORNER
//#define IOTLAB_SITE                                IOTLAB_LILLE_79_CENTER
#define IOTLAB_SITE                                IOTLAB_LYON_2
//#define IOTLAB_SITE                                IOTLAB_LYON_3
//#define IOTLAB_SITE                                IOTLAB_LYON_8
//#define IOTLAB_SITE                                IOTLAB_LYON_17

#if IOTLAB_SITE == IOTLAB_GRENOBLE_83_R_CORNER
#define NODE_NUM                                   83
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_79_R_CORNER
#define NODE_NUM                                   79
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_79_L_CORNER
#define NODE_NUM                                   79
#elif IOTLAB_SITE == IOTLAB_LILLE_79_CORNER
#define NODE_NUM                                   79
#elif IOTLAB_SITE == IOTLAB_LILLE_79_CENTER
#define NODE_NUM                                   79
#elif IOTLAB_SITE == IOTLAB_LYON_2
#define NODE_NUM                                   2
#elif IOTLAB_SITE == IOTLAB_LYON_3
#define NODE_NUM                                   3
#elif IOTLAB_SITE == IOTLAB_LYON_8
#define NODE_NUM                                   8
#elif IOTLAB_SITE == IOTLAB_LYON_17
#define NODE_NUM                                   17
#endif

#endif /* WITH_IOTLAB */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure log
 */
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO //LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_INFO

#define SIMPLE_ENERGEST_CONF_PERIOD                (1 * 60 * CLOCK_SECOND)
#define ENABLE_LOG_TSCH_LINK_ADD_REMOVE            1
#define ENABLE_LOG_TSCH_SLOT_LEVEL_RX_LOG          0
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure App
 */
#if WITH_COOJA
#define APP_UPWARD_SEND_INTERVAL                   (1 * 60 * CLOCK_SECOND / 30)
#define DOWNWARD_TRAFFIC                           0
#define APP_DOWNWARD_SEND_INTERVAL                 (1 * 60 * CLOCK_SECOND / 1)
#define APP_START_DELAY                            (2 * 60 * CLOCK_SECOND)
#define APP_DATA_PERIOD                            (30 * 60 * CLOCK_SECOND)
#define APP_PRINT_DELAY                            (1 * 60 * CLOCK_SECOND / 2)

#elif WITH_IOTLAB
#define APP_UPWARD_SEND_INTERVAL                   (1 * 60 * CLOCK_SECOND / 60)
//#define APP_UPWARD_SEND_INTERVAL                   (1 * 60 * CLOCK_SECOND / 6)
#define DOWNWARD_TRAFFIC                           0
#define APP_DOWNWARD_SEND_INTERVAL                 (1 * 60 * CLOCK_SECOND / 1)
#define APP_START_DELAY                            (2 * 60 * CLOCK_SECOND)
#define APP_DATA_PERIOD                            (8 * 60 * CLOCK_SECOND)
//#define APP_START_DELAY                            (40 * 60 * CLOCK_SECOND)
//#define APP_DATA_PERIOD                            (20 * 60 * CLOCK_SECOND)
#define APP_PRINT_DELAY                            (1 * 60 * CLOCK_SECOND)
#endif

#define APP_UPWARD_MAX_TX                          (APP_DATA_PERIOD / APP_UPWARD_SEND_INTERVAL)
#define APP_DOWNWARD_MAX_TX                        (APP_DATA_PERIOD / APP_DOWNWARD_SEND_INTERVAL)

#define WITH_VARYING_PPM                           0
#if WITH_VARYING_PPM
#define VARY_LENGTH                                8
#endif
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* 
 * Configure Contiki-NG system
 */
#define NBR_TABLE_CONF_MAX_NEIGHBORS               (NODE_NUM + 2) /* Add 2 for EB and broadcast neighbors in TSCH layer */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure IP and 6LOWPAN
 */
#define UIP_CONF_BUFFER_SIZE                       160 /* ksh */
#define UIP_CONF_MAX_ROUTES                        (NODE_NUM)
#define SICSLOWPAN_CONF_FRAG                       0
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure RPL
 */
#define RPL_CONF_MOP                               RPL_MOP_STORING_NO_MULTICAST
#define RPL_CONF_WITH_DAO_ACK                      1
#define RPL_CONF_WITH_PROBING                      1
#define RPL_CONF_PROBING_INTERVAL                  (2 * 60 * CLOCK_SECOND) /* originally 60 seconds */
#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT        (20 * CLOCK_SECOND) /* originally 5 seconds */
#define RPL_FIRST_MEASURE_PERIOD                   (1 * 60)
#define RPL_NEXT_MEASURE_PERIOD                    (1 * 60)
#define LINK_STATS_CONF_INIT_ETX_FROM_RSSI         1 /* originally 1 */
#define RPL_RELAXED_ETX_NOACK_PENALTY              1
#define RPL_MODIFIED_DAO_OPERATION_1               1 /* stop dao retransmission when preferred parent changed */
#define RPL_MODIFIED_DAO_OPERATION_2               1 /* nullify old preferred parent before sending no-path dao, this makes no-path dao sent through common shared slotframe */
//#define RPL_CONF_RPL_REPAIR_ON_DAO_NACK            0 /*  original: 0, set 1 in ALICE to enable local repair, quickly find another parent. */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure TSCH
 */
#define QUEUEBUF_CONF_NUM                          16 /* 16 in Orchestra, ALICE, and OST, originally 8 */
#define TSCH_CONF_MAX_INCOMING_PACKETS             8 /* 8 in OST, originally 4 */
#define IEEE802154_CONF_PANID                      0x58FE //22782 hckim //0x81a5 //ksh
//#define TSCH_CONF_AUTOSTART                        0 //ksh
//#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH          3 //ksh 6TiSCH minimal schedule length.
#ifndef WITH_SECURITY
#define WITH_SECURITY                              0
#endif /* WITH_SECURITY */
#define TSCH_NEXT_PRINT_PERIOD                     (1 * 60 * CLOCK_SECOND)
#define TSCH_LOG_CONF_QUEUE_LEN                    128 // originally 16
#define TSCH_SWAP_TX_RX_PROCESS_PENDING            1 /* swap order of rx_process_pending and tx_process_pending */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure TSCH scheduler
 */
#define TSCH_SCHEDULER_NB_ORCHESTRA                1 // 1: NB-Orchestra-storing
#define TSCH_SCHEDULER_LB_ORCHESTRA                2 // 2: LB-Orchestra
#define TSCH_SCHEDULER_ALICE                       3 // 3: ALICE
#define TSCH_SCHEDULER_OST                         4 // 4: OST

#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_NB_ORCHESTRA
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_LB_ORCHESTRA
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_ALICE
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_OST

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

#define ORCHESTRA_MODIFIED_CHILD_OPERATION         1
#define ORCHESTRA_COMPARE_LINKADDR_AND_IPADDR(linkaddr, ipaddr) (((((linkaddr)->u8[LINKADDR_SIZE - 2]) << 8) | (linkaddr)->u8[LINKADDR_SIZE - 1]) == ((((ipaddr)->u8[14]) << 8) | (ipaddr)->u8[15]))

#if CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_NB_ORCHESTRA
#define WITH_ORCHESTRA                             1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_NB // neighbor-storing
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 // 0: receiver-based, 1: sender-based
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              307 //unicast, 7, 11, 13, 17, 19, 23, 31, 43, 47, 59, 67, 71
//#define ORCHESTRA_CONF_UNICAST_PERIOD              17 //unicast, 7, 11, 13, 17, 19, 23, 31, 43, 47, 59, 67, 71
#define TSCH_CONF_BURST_MAX_LEN                    0
/* for log messages */
#define ORCHESTRA_EB_SF_ID                         0 //slotframe handle of EB slotframe
#define ORCHESTRA_UNICAST_SF_ID                    1 //slotframe handle of unicast slotframe
#define ORCHESTRA_BROADCAST_SF_ID                  2 //slotframe handle of broadcast/default slotframe

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_LB_ORCHESTRA
#define WITH_ORCHESTRA                             1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_LB //link-based Orchestra
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              11 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
#define TSCH_CONF_BURST_MAX_LEN                    0
/* for log messages */
#define ORCHESTRA_EB_SF_ID                         0 //slotframe handle of EB slotframe
#define ORCHESTRA_UNICAST_SF_ID                    1 //slotframe handle of unicast slotframe
#define ORCHESTRA_BROADCAST_SF_ID                  2 //slotframe handle of broadcast/default slotframe


#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_ALICE //ALICE
#define WITH_ALICE                                 1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_ALICE
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 // broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              23 // unicast, should be longer than (2N-2)/3 to provide contention-free links
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 //1: sender-based, 0:receiver-based
#define TSCH_CONF_BURST_MAX_LEN                    0

#define ALICE_PACKET_CELL_MATCHING_ON_THE_FLY      alice_packet_cell_matching_on_the_fly
#define ALICE_TIME_VARYING_SCHEDULING              alice_time_varying_scheduling
#define ALICE_EARLY_PACKET_DROP                    1
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (3 + 2 * NODE_NUM + 2) /* EB SF: tx/rx, CS SF: one link, UC SF: tx/rx for each node + 2 for spare */
#define ENABLE_ALICE_PACKET_CELL_MATCHING_LOG      0
#undef ENABLE_LOG_TSCH_LINK_ADD_REMOVE
#define ENABLE_LOG_TSCH_LINK_ADD_REMOVE            0
/* for log messages */
#define ORCHESTRA_EB_SF_ID                         0 //slotframe handle of EB slotframe
#define ORCHESTRA_BROADCAST_SF_ID                  1 //slotframe handle of broadcast/default slotframe
#define ORCHESTRA_UNICAST_SF_ID                    2 //slotframe handle of unicast slotframe
#define ALICE_BROADCAST_SF_ID                      1
#define ALICE_UNICAST_SF_ID                        2

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_OST //OST
#define WITH_OST                                   1
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_OST
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_UNICAST_PERIOD              47 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        41 //31 broadcast and default slotframe length, original: 31
#define TSCH_CONF_BURST_MAX_LEN                    0 /* turn burst off */

#define OST_ON_DEMAND_PROVISION                    1

#define TSCH_DEFAULT_BURST_TRANSMISSION            0
#if TSCH_DEFAULT_BURST_TRANSMISSION
#undef TSCH_CONF_BURST_MAX_LEN
#define TSCH_CONF_BURST_MAX_LEN                    16 /* turn burst off */
#endif

#define OST_HANDLE_QUEUED_PACKETS                  1
#define OST_JSB_ADD                                1
#define WITH_OST_LOG_INFO                          1
#define WITH_OST_LOG_DBG                           0
#define WITH_OST_LOG_NBR                           0
#define WITH_OST_LOG_SCH                           0
#define WITH_OST_TODO                              0 /* check ost_pigg1 of EB later */

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
#define TSCH_SCHEDULE_CONF_MAX_SLOTFRAMES          (3 + 4 * NODE_NUM + 2) /* EB, CS, RBUC, Periodic, Ondemand, 2 for spare */
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (3 + 1 + NODE_NUM + 4 * NODE_NUM + 2) /* EB (2), CS (1), RBUC (1 + NODE_NUM), Periodic, Ondemand, 2 for spare */
#define SSQ_SCHEDULE_HANDLE_OFFSET                 (2 * NODE_NUM + 2) /* End of the periodic slotframe (Under-provision uses up to 2*NODE_NUM+2) */

/* for log messages */
#define ORCHESTRA_EB_SF_ID                         0 //slotframe handle of EB slotframe
#define ORCHESTRA_UNICAST_SF_ID                    1 //slotframe handle of unicast slotframe
#define ORCHESTRA_BROADCAST_SF_ID                  2 //slotframe handle of broadcast/default slotframe
#define OST_PERIODIC_SF_ID_OFFSET                  2
#define OST_ONDEMAND_SF_ID_OFFSET                  SSQ_SCHEDULE_HANDLE_OFFSET

/* OST only */
#define OST_TSCH_TS_RX_ACK_DELAY                   1300
#define OST_TSCH_TS_TX_ACK_DELAY                   1500
#define TSCH_CONF_RX_WAIT                          800 /* ignore too late packets */
#define OST_NODE_ID_FROM_IPADDR(addr)              ((((addr)->u8[14]) << 8) | (addr)->u8[15])
#define OST_NODE_ID_FROM_LINKADDR(addr)            ((((addr)->u8[LINKADDR_SIZE - 2]) << 8) | (addr)->u8[LINKADDR_SIZE - 1]) 

#endif /* CURRENT_TSCH_SCHEDULER */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure radio
 */
/* m17dBm, m12dBm, m9dBm, m7dBm, m5dBm, m4dBm, m3dBm, m2dBm, m1dBm, 
   0dBm, 0_7dBm, 1_3dBm, 1_8dBm, 2_3dBm, 2_8dBm, 3dBm, 0dBm */
#define RF2XX_TX_POWER                             PHY_POWER_m17dBm
//#define RF2XX_TX_POWER                             PHY_POWER_3dBm

/* m101dBm, m90dBm, m87dBm, m84dBm, m81dBm, m78dBm, m75dBm, m72dBm, 
   m69dBm, m66dBm, m63dBm, m60dBm, m57dBm, m54dBm, m51dBm, m48dBm */
#define RF2XX_RX_RSSI_THRESHOLD                    RF2XX_PHY_RX_THRESHOLD__m87dBm
/*---------------------------------------------------------------------------*/

#endif /* PROJECT_CONF_H_ */ 
