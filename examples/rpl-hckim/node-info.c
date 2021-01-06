#include "node-info.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#if TESTBED_SITE == IOT_LAB_LYON_2
uint16_t root_info[3] = {7, 0xa371, 0};
uint16_t non_root_info[NON_ROOT_NUM][3] = {
    {3, 0x8676, 0}
};

#elif TESTBED_SITE == IOT_LAB_LYON_17
uint16_t root_info[3] = {1, 0x9768, 0};
uint16_t non_root_info[NON_ROOT_NUM][3] = {
    // root node {1, 0x9768, 0},
    {2, 0x8867, 0},
    {3, 0x8676, 0},
    {4, 0xb181, 0},
    {5, 0x8968, 0},
    {6, 0xc279, 0},
    {7, 0xa371, 0},
    {8, 0xa683, 0},
    {10, 0x8976, 0},
    {11, 0x8467, 0},
    {12, 0xb682, 0},
    {13, 0xb176, 0},
    {14, 0x2860, 0},
    {15, 0xa377, 0},
    {16, 0xb978, 0},
    {17, 0xa168, 0},
    {18, 0x3261, 0}
};

#elif TESTBED_SITE == IOT_LAB_LILLE_46
uint16_t root_info[3] = {250, 0x2659, 0};
uint16_t non_root_info[NON_ROOT_NUM][3] = {
  {152, 0xa173, 0},
  {153, 0xb572, 0},
  {154, 0xb071, 0},
  {155, 0xb372, 0},
  {156, 0x3759, 0},
  {157, 0x2755, 0},
  {158, 0x2154, 0},
  {169, 0x1459, 0},
  {170, 0xb671, 0},
  {171, 0x2258, 0},
  {172, 0x3554, 0},
  {173, 0xc170, 0},
  {174, 0x9273, 0},
  {175, 0x2459, 0},
  {191, 0x8473, 0},
  {193, 0x3558, 0},
  {194, 0x1159, 0},
  {195, 0xb388, 0},
  {196, 0x2451, 0},
  {205, 0xb173, 0},
  {206, 0x2850, 0},
  {207, 0x3359, 0},
  {208, 0x2350, 0},
  {209, 0x2050, 0},
  {210, 0x1855, 0},
  {225, 0xa573, 0},
  {226, 0x9573, 0},
  {227, 0x2559, 0},
  {228, 0x1455, 0},
  {229, 0x9770, 0},
  {230, 0x2751, 0},
  {231, 0x2052, 0},
  {238, 0x9373, 0},
  {239, 0xc270, 0},
  {240, 0xb288, 0},
  {241, 0xb070, 0},
  {242, 0x2450, 0},
  {243, 0xb073, 0},
  {244, 0x1258, 0},
// root node  {250, 0x2659, 0},
  {251, 0x2454, 0},
  {252, 0x2458, 0},
  {253, 0xb871, 0},
  {254, 0x2358, 0},
  {255, 0xb371, 0},
  {256, 0x2554, 0}
};

#elif TESTBED_SITE == IOT_LAB_LILLE_31
uint16_t root_info[3] = {88, 0xc073, 0};
uint16_t non_root_info[NON_ROOT_NUM][3] = {
  {47, 0x1854, 0},
  {48, 0x0956, 0},
  {51, 0xb973, 0},
  {54, 0x3151, 0},
  {55, 0xb771, 0},
  {56, 0xa273, 0},
  {57, 0x1256, 0},
  {59, 0xb271, 0},
  {60, 0x0761, 0},
  {61, 0x3254, 0},
  {63, 0xc070, 0},
  {67, 0x1957, 0},
  {70, 0xa070, 0},
  {71, 0x1654, 0},
  {80, 0x2750, 0},
  {81, 0x1556, 0},
  {82, 0x2156, 0},
  {84, 0x9370, 0},
  {85, 0x9773, 0},
  // root node {88, 0xc073, 0},
  {91, 0x1161, 0},
  {92, 0x3962, 0},
  {93, 0x3453, 0},
  {97, 0x2055, 0},
  {98, 0x1751, 0},
  {100, 0x9989, 0},
  {101, 0xb870, 0},
  {104, 0x1359, 0},
  {105, 0x3458, 0},
  {108, 0x1658, 0},
  {110, 0x8573, 0}
};

#endif

void print_node_info()
{
  LOG_INFO("HCK-NODE root %u %x\n", root_info[0], root_info[1]);
  uint8_t i = 0;
  for(i = 0; i < NON_ROOT_NUM; i++) {
    LOG_INFO("HCK-NODE non_root %u %x\n", non_root_info[i][0], non_root_info[i][1]);
  }
  LOG_INFO("HCK-NODE end\n");
}
/*---------------------------------------------------------------------------*/
uint16_t
non_root_index_from_addr(const uip_ipaddr_t *sender_addr)
{
  uint16_t sender_uid = (sender_addr->u8[14] << 8) + sender_addr->u8[15];

  uint16_t i = 0;
  for(i = 0; i < NON_ROOT_NUM; i++) {
    if(non_root_info[i][1] == sender_uid) {
      return i;
    }
  }
  return NON_ROOT_NUM;
}