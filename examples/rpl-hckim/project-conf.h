#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_INFO

#define DOWNWARD_TRAFFIC                           1
#define APP_START_DELAY                            (3 * 60 * CLOCK_SECOND)
#define APP_SEND_INTERVAL                          (1 * 60 * CLOCK_SECOND)

#define RPL_FIRST_MEASURE_PERIOD                   (3 * 60)
#define RPL_NEXT_MEASURE_PERIOD                    (1 * 60)

#define SIMPLE_ENERGEST_CONF_PERIOD                (1 * 60 * CLOCK_SECOND)

/* m17dBm, m12dBm, m9dBm, m7dBm, m5dBm, m4dBm, m3dBm, m2dBm, m1dBm, 
   0dBm, 0_7dBm, 1_3dBm, 1_8dBm, 2_3dBm, 2_8dBm, 3dBm, 0dBm */
#define RF2XX_TX_POWER                             PHY_POWER_m17dBm
//#define RF2XX_TX_POWER                             PHY_POWER_3dBm
/* m101dBm, m90dBm, m87dBm, m84dBm, m81dBm, m78dBm, m75dBm, m72dBm, 
   m69dBm, m66dBm, m63dBm, m60dBm, m57dBm, m54dBm, m51dBm, m48dBm */
#define RF2XX_RX_RSSI_THRESHOLD                    RF2XX_PHY_RX_THRESHOLD__m87dBm


#define IOT_LAB_LYON_2                             1
#define IOT_LAB_LYON_17                            2
#define IOT_LAB_LILLE_46                           3
#define IOT_LAB_TEMP                               9

//#define TESTBED_SITE                               IOT_LAB_LYON_2
//#define TESTBED_SITE                               IOT_LAB_LYON_17
#define TESTBED_SITE                               IOT_LAB_LILLE_46

#endif /* PROJECT_CONF_H_ */ 
