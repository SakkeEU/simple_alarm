#include "simple_wifi.h"
#include "tcpip_adapter.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"

static esp_err_t wifi_handler(void *ctx, system_event_t *event){
	
	switch(event->event_id){
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG_WIFI, " WIFI STARTED");
            break;
		default:
			break;
	}	
	return ESP_OK;
}

void simple_wifi_init(wifi_mode_t mode, uint8_t chan){
	
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_handler, NULL));
	
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    if(chan != 0)
		ESP_ERROR_CHECK(esp_wifi_set_channel(chan, 0));
}

void simple_wifi_deinit(void){
	esp_wifi_stop();
	esp_wifi_deinit();
}
