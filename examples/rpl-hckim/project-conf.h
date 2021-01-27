#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_


/*---------------------------------------------------------------------------*/
/*
 * Configure testbed site, node num, topology
 */
#define IOT_LAB_LYON_2                             1
#define IOT_LAB_LYON_17                            2
#define IOT_LAB_LILLE_46                           3
#define IOT_LAB_LILLE_31                           4

//#define TESTBED_SITE                               IOT_LAB_LYON_2
//#define TESTBED_SITE                               IOT_LAB_LYON_17
//#define TESTBED_SITE                               IOT_LAB_LILLE_46
#define TESTBED_SITE                               IOT_LAB_LILLE_31

#if TESTBED_SITE == IOT_LAB_LYON_2
#define NODE_NUM                                   2
#elif TESTBED_SITE == IOT_LAB_LYON_17
#define NODE_NUM                                   17
#elif TESTBED_SITE == IOT_LAB_LILLE_46
#define NODE_NUM                                   46
#elif TESTBED_SITE == IOT_LAB_LILLE_31
#define NODE_NUM                                   31
#endif

#define NBR_TABLE_CONF_MAX_NEIGHBORS               (NODE_NUM + 2)
#define UIP_CONF_MAX_ROUTES                        (NODE_NUM)
#define TSCH_SCHEDULE_CONF_MAX_LINKS               (NODE_NUM * 3) //(70) //alice-implementation
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*
 * Configure App
 */
#define DOWNWARD_TRAFFIC                           1
#define APP_START_DELAY                            (10 * 60 * CLOCK_SECOND) //(10 * 60 * CLOCK_SECOND)
#define APP_SEND_INTERVAL                          (2 * 60 * CLOCK_SECOND)
#define EVALUATION_DURATION                        (60 * 60 * CLOCK_SECOND) //(30 * 60 * CLOCK_SECOND)
#define APP_MAX_TX                                 (EVALUATION_DURATION / APP_SEND_INTERVAL)
/*---------------------------------------------------------------------------*/


/*
 * Configure RPL
 */
#define RPL_CONF_MOP                               RPL_MOP_STORING_NO_MULTICAST  //ksh..
#define RPL_CONF_WITH_DAO_ACK                      1 //ksh..
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

#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_NB_ORCHESTRA
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_LB_ORCHESTRA
//#define CURRENT_TSCH_SCHEDULER                     TSCH_SCHEDULER_ALICE

#define ORCHESTRA_RULE_NB { &eb_per_time_source, \
                          &unicast_per_neighbor_rpl_storing, \
                          &default_common }
#define ORCHESTRA_RULE_LB { &eb_per_time_source, \
                          &unicast_per_neighbor_link_based, \
                          &default_common }
#define ORCHESTRA_RULE_ALICE { &eb_per_time_source, \
                          &default_common, \
                          &unicast_per_neighbor_rpl_storing }

#if CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_NB_ORCHESTRA
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_NB // neighbor-storing
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        0 // 0: receiver-based, 1: sender-based

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_LB_ORCHESTRA
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_LB //link-based Orchestra

#elif CURRENT_TSCH_SCHEDULER == TSCH_SCHEDULER_ALICE //ALICE
#define ORCHESTRA_CONF_RULES                       ORCHESTRA_RULE_ALICE
#define WITH_ALICE                                 1
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED        1 // 1: sender-based, 0:receiver-based
// #define ORCHESTRA_ONE_CHANNEL_OFFSET             0 //mc-orchestra -> 1:single channel offset, 0:multiple channel offsets
#define ALICE_CALLBACK_PACKET_SELECTION            alice_callback_packet_selection //ksh. alice packet selection
#define ALICE_CALLBACK_SLOTFRAME_START             alice_callback_slotframe_start //ksh. alice time varying slotframe schedule
#define ALICE_BROADCAST_SF_ID                      1 //slotframe handle of broadcast/default slotframe
#define ALICE_UNICAST_SF_ID                        2 //slotframe handle of unicast slotframe
#ifndef MULTIPLE_CHANNEL_OFFSETS
#define MULTIPLE_CHANNEL_OFFSETS                   1 //ksh.. allow multiple channel offsets.
#endif

#endif /* CURRENT_TSCH_SCHEDULER */

#define ORCHESTRA_CONF_EBSF_PERIOD                 397 // EB, original: 397
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD        31 // broadcast and default slotframe length, original: 31
#define ORCHESTRA_CONF_UNICAST_PERIOD              17 // unicast, 7, 11, 23, 31, 43, 47, 59, 67, 71    
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
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_INFO

#define SIMPLE_ENERGEST_CONF_PERIOD                (1 * 60 * CLOCK_SECOND)
#define ENABLE_LOG_TSCH_LINK_ADD_REMOVE            0
/*---------------------------------------------------------------------------*/



#endif /* PROJECT_CONF_H_ */ 
