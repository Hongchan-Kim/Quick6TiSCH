#include "node-info.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#if WITH_IOTLAB

#if IOTLAB_SITE == IOTLAB_LILLE_83_CORNER
uint16_t iotlab_nodes[NODE_NUM][3] = {
  // {host name, uid, rx count}
  {250, 0x2659, 0}, // root node
  {251, 0x2454, 0},
  {252, 0x2458, 0},
  {253, 0xb871, 0},
  {254, 0x2358, 0},
  {255, 0xb371, 0},
  {256, 0x2554, 0},
  {245, 0x2851, 0},
  {246, 0x2251, 0},
  {247, 0x2551, 0},
  {248, 0x3153, 0},
  {249, 0x1559, 0},
  {238, 0x9373, 0},
  {239, 0xc270, 0},
  {240, 0xb288, 0},
  {241, 0xb070, 0},
  {243, 0xb073, 0},
  {244, 0x1258, 0},
  {232, 0x3659, 0},
  {233, 0x8873, 0},
  {234, 0x1254, 0},
  {235, 0xb270, 0},
  {236, 0x2352, 0},
  {237, 0x0858, 0},
  {225, 0xa573, 0},
  {226, 0x9573, 0},
  {227, 0x2559, 0},
  {228, 0x1455, 0},
  {229, 0x9770, 0},
  {230, 0x2751, 0},
  {231, 0x2052, 0},
  {219, 0x8474, 0},
  {220, 0xa873, 0},
  {221, 0x3059, 0},
  {222, 0x9173, 0},
  {223, 0xa289, 0},
  {205, 0xb173, 0},
  {206, 0x2850, 0},
  {207, 0x3359, 0},
  {208, 0x2350, 0},
  {209, 0x2050, 0},
  {210, 0x1855, 0},
  {198, 0x1856, 0},
  {199, 0x9073, 0},
  {200, 0x9270, 0},
  {201, 0xa189, 0},
  {202, 0x9877, 0},
  {203, 0xc273, 0},
  {191, 0x8473, 0},
  {192, 0x9570, 0},
  {193, 0x3558, 0},
  {194, 0x1159, 0},
  {195, 0xb388, 0},
  {196, 0x2451, 0},
  {178, 0xa473, 0},
  {179, 0x0660, 0},
  {180, 0x3559, 0},
  {181, 0xb172, 0},
  {182, 0x2853, 0},
  {169, 0x1459, 0},
  {170, 0xb671, 0},
  {171, 0x2258, 0},
  {172, 0x3554, 0},
  {173, 0xc170, 0},
  {174, 0x9273, 0},
  {175, 0x2459, 0},
  {161, 0xa390, 0},
  {162, 0x2854, 0},
  {163, 0x8774, 0},
  {164, 0xa077, 0},
  {165, 0x2550, 0},
  {152, 0xa173, 0},
  {153, 0xb572, 0},
  {154, 0xb071, 0},
  {156, 0x3759, 0},
  {157, 0x2755, 0},
  {158, 0x2154, 0},
  {143, 0x3862, 0},
  {144, 0x1759, 0},
  {145, 0xb373, 0},
  {146, 0xb189, 0},
  {147, 0x2151, 0},
  {148, 0xb370, 0}
};
#elif IOTLAB_SITE == IOTLAB_LILLE_2_CORNER
uint16_t iotlab_nodes[NODE_NUM][3] = {
  // {host name, uid, rx count}
  {250, 0x2659, 0}, // root node
  {251, 0x2454, 0}
};
#elif IOTLAB_SITE == IOTLAB_LILLE_3_CORNER
uint16_t iotlab_nodes[NODE_NUM][3] = {
  // {host name, uid, rx count}
  {250, 0x2659, 0}, // root node
  {220, 0xa873, 0},
  {148, 0xb370, 0}
};
#elif IOTLAB_SITE == IOTLAB_GRENOBLE_3_CORNER
uint16_t iotlab_nodes[NODE_NUM][3] = {
  // {host name, uid, rx count}
  {95, 0xa770, 0}, // root node
  {115, 0x8471, 0},
  {151, 0x9576, 0}
};
#endif /* IOTLAB_SITE */
/*---------------------------------------------------------------------------*/
uint16_t
iotlab_node_id_from_uid(uint16_t uid)
{
  uint16_t i = 0;
  for(i = 0; i < NODE_NUM; i++) {
    if(iotlab_nodes[i][1] == uid) {
      return i + 1;
    }
  }
  return 0; /* no matching index */
}
/*---------------------------------------------------------------------------*/
void
print_iotlab_node_info()
{
  LOG_HCK_NODE("root %u %x (%u %x)\n", APP_ROOT_ID, APP_ROOT_ID, iotlab_nodes[0][0], iotlab_nodes[0][1]);
  uint8_t i = 1;
  for(i = 1; i < NODE_NUM; i++) {
    LOG_HCK_NODE("non_root %u %x %u %x\n", i + 1, i + 1, iotlab_nodes[i][0], iotlab_nodes[i][1]);
  }
  LOG_HCK_NODE("end\n");
}
/*---------------------------------------------------------------------------*/
#endif /* WITH_IOTLAB */