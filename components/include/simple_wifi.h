#ifndef _SIMPLE_WIFI_
#define _SIMPLE_WIFI_

#include <stdint.h>
#include "esp_wifi_types.h"
#define TAG_WIFI "simple_wifi"

#define SIMPLE_STATION WIFI_MODE_STA
#define SIMPLE_AP WIFI_MODE_AP

void simple_wifi_init(wifi_mode_t mode, uint8_t chan);
void simple_wifi_deinit(void);

#endif
