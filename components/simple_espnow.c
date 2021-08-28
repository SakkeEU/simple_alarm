#include <string.h>
#include "simple_espnow.h"
//#include "simple_wifi.h"
#include "esp_log.h"

uint8_t simple_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void simple_espnow_init(esp_now_send_cb_t send_cb, esp_now_recv_cb_t receive_cb, uint8_t chan){
	
	ESP_ERROR_CHECK(esp_now_init());
	if(send_cb != NULL)
		ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
	if(receive_cb != NULL)
		ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_cb));
	
	esp_now_peer_info_t peer = {
		.lmk			= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.channel		= chan,
		.ifidx			= ESP_IF_WIFI_STA,
		.encrypt		= 0,
		.priv			= NULL
	};
	memcpy(peer.peer_addr, simple_broadcast_mac, ESP_NOW_ETH_ALEN);
	ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

void simple_espnow_deinit(void){
	esp_now_deinit();
}
