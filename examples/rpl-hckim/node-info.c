#include "node-info.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#if TESTBED_SITE == IOT_LAB_LYON
uint16_t root_info[2] = {18, 0x3261};
uint16_t non_root_info[NON_ROOT_NUM][3] = {
    {1, 0x9768, 0},
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
    {17, 0xa168, 0}
};
#elif TESTBED_SITE == IOT_LAB_PARIS
#endif

void print_node_info()
{
  LOG_INFO("HCK-NODE root %u %x\n", root_info[0], root_info[1]);
  uint8_t i = 0;
  for(i = 0; i < NON_ROOT_NUM; i++) {
    LOG_INFO("HCK-NODE non %u %x\n", non_root_info[i][0], non_root_info[i][1]);
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

