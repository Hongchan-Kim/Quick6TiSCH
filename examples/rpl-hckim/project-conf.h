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
#define IOTLAB_LILLE_32                           6
#define IOTLAB_LILLE_46                           7

//#define TESTBED_SITE                               IOTLAB_LYON_2
#define TESTBED_SITE                               IOTLAB_LYON_3
//#define TESTBED_SITE                               IOTLAB_LYON_10
//#define TESTBED_SITE                               IOTLAB_LYON_17
//#define TESTBED_SITE                               IOTLAB_LILLE_24
//#define TESTBED_SITE                               IOTLAB_LILLE_32
//#define TESTBED_SITE                               IOTLAB_LILLE_46

#if TESTBED_SITE == IOTLAB_LYON_2
#define NODE_NUM                                   2
#elif TESTBED_SITE == IOTLAB_LYON_3
#define NODE_NUM                                   3
#elif TESTBED_SITE == IOTLAB_LYON_10
#define NODE_NUM                                   10
#elif TESTBED_SITE == IOTLAB_LYON_17
#define NODE_NUM                                   17
#elif TESTBED_SITE == IOTLAB_LILLE_24
#define NODE_NUM                                   24
#elif TESTBED_SITE == IOTLAB_LILLE_32
#define NODE_NUM                                   32
#elif TESTBED_SITE == IOTLAB_LILLE_46
#define NODE_NUM                                   46
#endif

#define NBR_TABLE_CONF_MAX_NEIGHBORS               (NODE_NUM + 2)
#define UIP_CONF_MAX_ROUTES                        (NODE_NUM)
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure App
 */
#define DOWNWARD_TRAFFIC                           1
#define APP_START_DELAY                            (3 * 60 * CLOCK_SECOND) //(10 * 60 * CLOCK_SECOND)
#define APP_PRINT_DELAY                            (1 * 30 * CLOCK_SECOND)
#define APP_SEND_INTERVAL                          (1 * 30 * CLOCK_SECOND)
#define APP_MAX_TX                                 20
/*---------------------------------------------------------------------------*/


/*
 * Configure RPL
 */
#define RPL_CONF_MOP                               RPL_MOP_STORING_NO_MULTICAST  //ksh..
#define RPL_CONF_WITH_DAO_ACK                      1 //ksh..
#define RPL_CONF_WITH_PROBING                      1
#define RPL_FIRST_MEASURE_PERIOD                   (5 * 60)
#define RPL_NEXT_PRINT_PERIOD                      (1 * 60)
/*---------------------------------------------------------------------------*/


/*
 * Configure IP and 6LOWPAN
 */
#define UIP_CONF_BUFFER_SIZE                       160 //ksh
#define SICSLOWPAN_CONF_FRAG                       0
/*---------------------------------------------------------------------------*/


/*
 * Configure TSCH
 */
//#define IEEE802154_CONF_PANID                      0x81a5 //ksh
//#define TSCH_CONF_AUTOSTART                        0 //ksh
//#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH          3 //ksh 6TiSCH minimal schedule length.
#ifndef WITH_SECURITY
#define WITH_SECURITY                              0
#endif /* WITH_SECURITY */
#define TSCH_NEXT_PRINT_PERIOD                     (1 * 60 * CLOCK_SECOND)
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

#if CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_NB_ORCHESTRA
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_NB // neighbor-storing
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        0 // 0: receiver-based, 1: sender-based
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        31 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              17 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_LB_ORCHESTRA
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_LB //link-based Orchestra
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 //EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        31 //broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              17 //unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_ALICE //ALICE
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_ALICE
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        19 //31 broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              17 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    

#define WITH_ALICE                                 1
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 //1: sender-based, 0:receiver-based
#define ALICE_PACKET_CELL_MATCHING_ON_THE_FLY    alice_packet_cell_matching_on_the_fly
#define ALICE_TIME_VARYING_SCHEDULING            alice_time_varying_scheduling
#define ALICE_BROADCAST_SF_ID                      1 //slotframe handle of broadcast/default slotframe
#define ALICE_UNICAST_SF_ID                        2 //slotframe handle of unicast slotframe
#define TSCH_CONF_BURST_MAX_LEN                    0
#define ENABLE_ALICE_PACKET_CELL_MATCHING_LOG      0
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (NODE_NUM * 3) //(70) //alice-implementation

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_OST //OST
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_OST
#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_UNICAST_PERIOD              47 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        41 //31 broadcast and default slotframe length, original: 31
#define TSCH_CONF_BURST_MAX_LEN                    0 /* turn burst off */

#define WITH_OST                                   1
#define WITH_OST_LOG                               0
#define WITH_OST_TEST                              1
#define WITH_OST_TODO                              0 /* check ost_pigg1 of EB later */
#define WITH_OST_OID                               1 /* implement ost_id to the ds6_nbr and tsch_nbr */

#define OST_DEFAULT_COMMON_CHANNEL_OFFSET          1
#define OST_ON_DEMAND_PROVISION                    1

#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM                           16
#define TSCH_CONF_MAX_INCOMING_PACKETS              8
#define TSCH_CONF_RX_WAIT                           800  //guard time
#define TSCH_CONF_MAX_KEEPALIVE_TIMEOUT             (30 * CLOCK_SECOND)
#define TSCH_CONF_MAX_EB_PERIOD                     (16 * CLOCK_SECOND)


#define N_SELECTION_PERIOD                         15 //related to N_MAX: Min. traffic load = 1 / (N_SELECTION_PERIOD * 100) pkt/slot (when num_tx = 1). 
#define N_MAX                                      8 //max t_offset 65535-1, 65535 is used for no-allocation
#define MORE_UNDER_PROVISION                       1 //more allocation 2^MORE_UNDER_PROVISION times than under-provision
#define INC_N_NEW_TX_REQUEST                       100 //Maybe used for denial message
#define MULTI_CHANNEL                              1
#define PRR_THRES_TX_CHANGE                        70
#define NUM_TX_MAC_THRES_TX_CHANGE                 20
#define NUM_TX_FAIL_THRES                          5
#define THRES_CONSEQUTIVE_N_INC                    3
#define T_OFFSET_ALLOCATION_FAIL                   ((1 << N_MAX) + 1)
#define T_OFFSET_CONSECUTIVE_NEW_TX_REQUEST        ((1 << N_MAX) + 2)
#define THRES_CONSECUTIVE_NEW_TX_REQUEST           10
#define TSCH_SCHEDULE_CONF_MAX_SLOTFRAMES          2 * NBR_TABLE_CONF_MAX_NEIGHBORS
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (100) //(70) //alice-implementation
#define SSQ_SCHEDULE_HANDLE_OFFSET                 (2 * NODE_NUM + 2) //Under-provision uses up to 2*NODE_NUM+2
#undef TSCH_CONF_RX_ACK_DELAY
#define TSCH_CONF_RX_ACK_DELAY                     1300
#undef TSCH_CONF_TX_ACK_DELAY
#define TSCH_CONF_TX_ACK_DELAY                     1500



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


/*---------------------------------------------------------------------------*/
/*
 * Configure log
 */
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_DBG //LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_INFO

#define SIMPLE_ENERGEST_CONF_PERIOD                (1 * 60 * CLOCK_SECOND)
#define ENABLE_LOG_TSCH_LINK_ADD_REMOVE            1
#define ENABLE_LOG_TSCH_SLOT_LEVEL_RX_LOG          1
/*---------------------------------------------------------------------------*/



#endif /* PROJECT_CONF_H_ */ 
