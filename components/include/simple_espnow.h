#ifndef _SIMPLE_ESPNOW_
#define _SIMPLE_ESPNOW_

#include <stdint.h>
#include "esp_now.h"

#define TAG_ESPNOW "simple_espnow"

extern uint8_t simple_broadcast_mac[ESP_NOW_ETH_ALEN];

void simple_espnow_init(esp_now_send_cb_t send_cb, esp_now_recv_cb_t receive_cb, uint8_t chan);
void simple_espnow_deinit(void);

#endif
