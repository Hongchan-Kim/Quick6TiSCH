#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_


/*---------------------------------------------------------------------------*/
/*
 * Configure testbed site, node num, topology
 */
#define WITH_IOTLAB                               1

#define IOTLAB_LYON_2                             1
#define IOTLAB_LYON_3                             2
#define IOTLAB_LYON_10                            3
#define IOTLAB_LYON_17                            4
#define IOTLAB_LILLE_24                           5
#define IOTLAB_LILLE_40                           6
#define IOTLAB_LILLE_46                           7
#define IOTLAB_LILLE_50                           8
#define IOTLAB_LILLE_79                           9
#define IOTLAB_GRENOBLE_R_43                      10
#define IOTLAB_GRENOBLE_R_63                      11
#define IOTLAB_GRENOBLE_R_83                      12
#define IOTLAB_GRENOBLE_R_2                       13
#define IOTLAB_GRENOBLE_R_3                       14
#define IOTLAB_GRENOBLE_L_43                      15
#define IOTLAB_GRENOBLE_L_63                      16
#define IOTLAB_GRENOBLE_L_79                      17


//#define IOTLAB_SITE                                IOTLAB_LYON_2
//#define IOTLAB_SITE                                IOTLAB_LYON_3
//#define IOTLAB_SITE                                IOTLAB_LYON_10
//#define IOTLAB_SITE                                IOTLAB_LYON_17
//#define IOTLAB_SITE                                IOTLAB_LILLE_24 /* z = 9.6 */
//#define IOTLAB_SITE                                IOTLAB_LILLE_40 /* z = 6.1 */
//#define IOTLAB_SITE                                IOTLAB_LILLE_46 /* z = 9.6 */
//#define IOTLAB_SITE                                IOTLAB_LILLE_50 /* z = 6.1 */
//#define IOTLAB_SITE                                IOTLAB_LILLE_79 /* z = 9.6 or 9.4 */
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_R_43 /* 4~5 hop */
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_R_63
#define IOTLAB_SITE                                IOTLAB_GRENOBLE_R_83 /* 9~10 hop */
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_R_2
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_R_3
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_L_43
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_L_63
//#define IOTLAB_SITE                                IOTLAB_GRENOBLE_L_79

#if IOTLAB_SITE == IOTLAB_LYON_2
#define NODE_NUM                                   2
#elif IOTLAB_SITE == IOTLAB_LYON_3
#define NODE_NUM                                   3
#elif IOTLAB_SITE == IOTLAB_LYON_10
#define NODE_NUM                                   10
#elif IOTLAB_SITE == IOTLAB_LYON_17
#define NODE_NUM                                   17
#elif IOTLAB_SITE == IOTLAB_LILLE_24
#define NODE_NUM                                   24
#elif IOTLAB_SITE == IOTLAB_LILLE_40
#define NODE_NUM                                   40
#elif IOTLAB_SITE == IOTLAB_LILLE_46
#define NODE_NUM                                   46
#elif IOTLAB_SITE == IOTLAB_LILLE_50
#define NODE_NUM                                   50
#elif IOTLAB_SITE == IOTLAB_LILLE_79
#define NODE_NUM                                   79
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_R_43
#define NODE_NUM                                   43
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_R_63
#define NODE_NUM                                   63
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_R_83
#define NODE_NUM                                   83
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_R_2
#define NODE_NUM                                   2
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_R_3
#define NODE_NUM                                   3
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_L_43
#define NODE_NUM                                   43
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_L_63
#define NODE_NUM                                   63
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_L_79
#define NODE_NUM                                   79
#endif

#define NBR_TABLE_CONF_MAX_NEIGHBORS               (NODE_NUM + 2) /* Add 2 for EB and broadcast neighbors in TSCH layer */
#define UIP_CONF_MAX_ROUTES                        (NODE_NUM)
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
#define ENABLE_LOG_TSCH_SLOT_LEVEL_RX_LOG          1
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure App
 */
#define DOWNWARD_TRAFFIC                           1
#define APP_SEND_INTERVAL                          (1 * 60 * CLOCK_SECOND / 2)
#define APP_START_DELAY                            (30 * 60 * CLOCK_SECOND)
#define APP_DATA_PERIOD                            (60 * 60 * CLOCK_SECOND)
#define APP_MAX_TX                                 (APP_DATA_PERIOD / APP_SEND_INTERVAL)
#define APP_PRINT_DELAY                            (1 * 60 * CLOCK_SECOND)
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
 * Configure IP and 6LOWPAN
 */
#define UIP_CONF_BUFFER_SIZE                       160 /* ksh */
#define SICSLOWPAN_CONF_FRAG                       0
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure TSCH
 */
#define QUEUEBUF_CONF_NUM                          16 /* original: 8, 16 in Orchestra, ALICE, and OST */
#define TSCH_CONF_MAX_INCOMING_PACKETS             8 /* original: 4, 8 in OST */
//#define IEEE802154_CONF_PANID                      0x81a5 //ksh
//#define TSCH_CONF_AUTOSTART                        0 //ksh
//#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH          3 //ksh 6TiSCH minimal schedule length.
#ifndef WITH_SECURITY
#define WITH_SECURITY                              0
#endif /* WITH_SECURITY */
#define TSCH_NEXT_PRINT_PERIOD                     (1 * 60 * CLOCK_SECOND)
#define TSCH_LOG_CONF_QUEUE_LEN                    128 // original: 16
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure TSCH scheduler
 */
#define TSCH_SCHEDULER_NB_ORCHESTRA                1 // 1: NB-Orchestra-storing
#define TSCH_SCHEDULER_LB_ORCHESTRA                2 // 2: LB-Orchestra
#define TSCH_SCHEDULER_ALICE                       3 // 3: ALICE
#define TSCH_SCHEDULER_OST                         4 // 4: OST

//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_NB_ORCHESTRA
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_LB_ORCHESTRA
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_ALICE
#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_OST

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
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_NB // neighbor-storing
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 // 0: receiver-based, 1: sender-based
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              11 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71
/* for log messages */
#define ORCHESTRA_EB_SF_ID                         0 //slotframe handle of EB slotframe
#define ORCHESTRA_UNICAST_SF_ID                    1 //slotframe handle of unicast slotframe
#define ORCHESTRA_BROADCAST_SF_ID                  2 //slotframe handle of broadcast/default slotframe

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_LB_ORCHESTRA
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_LB //link-based Orchestra
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              11 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
/* for log messages */
#define ORCHESTRA_EB_SF_ID                         0 //slotframe handle of EB slotframe
#define ORCHESTRA_UNICAST_SF_ID                    1 //slotframe handle of unicast slotframe
#define ORCHESTRA_BROADCAST_SF_ID                  2 //slotframe handle of broadcast/default slotframe


#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_ALICE //ALICE
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_ALICE
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 // broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              23 // unicast, should be longer than (2N-2)/3 to provide contention-free links
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 //1: sender-based, 0:receiver-based
#define TSCH_CONF_BURST_MAX_LEN                    0

#define WITH_ALICE                                 1
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
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_OST
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_UNICAST_PERIOD              47 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        41 //31 broadcast and default slotframe length, original: 31
#define TSCH_CONF_BURST_MAX_LEN                    0 /* turn burst off */

#define WITH_OST                                   1
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
