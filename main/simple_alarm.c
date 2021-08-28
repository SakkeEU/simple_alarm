#include "simple_wifi.h"
#include "simple_espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_system.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define TAG_MAIN "simple_main"

#define SIMPLE_GPIO_PIN GPIO_NUM_4
//signal base every ALIVE_SIG_TIME seconds 
#define ALIVE_SIG_TIME 10000/portTICK_RATE_MS
//ignore alarm triggers for ALARM_GOING_TIME after the first alert
#define ALARM_GOING_TIME 5000/portTICK_RATE_MS

typedef enum{
	ALARM_OFF,
	ALARM_ON,
	ALARM_GOING,
}alarm_status_t;

enum{
	IM_ALIVE,
	ALARM_OCCURRED,
};

uint8_t extern simple_broadcast_mac[ESP_NOW_ETH_ALEN];
static SemaphoreHandle_t alarm_semaphore = NULL;
static alarm_status_t alarm_status = ALARM_ON;

void alarm_set(void *args);
void alarm_reset(void);
void base_check(void *args);
void gpio_isr_handler(void *arg);
static void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);

void app_main(void){
    uint8_t chan = 1;
    simple_wifi_init(SIMPLE_STATION, chan);
    simple_espnow_init(send_cb, NULL, chan);
    
    gpio_num_t pin = SIMPLE_GPIO_PIN;
    gpio_config_t gconf = {
		.pin_bit_mask = 1 << pin,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = 0,
		.pull_down_en = 0,
		.intr_type = GPIO_INTR_POSEDGE,
		};	
	gpio_config(&gconf);
	alarm_semaphore = xSemaphoreCreateBinary();
	
	gpio_install_isr_service(0);
	gpio_isr_handler_add(pin, gpio_isr_handler, NULL);
	
	xTaskCreate(alarm_set, "alarm_set", 2048, NULL, 11, NULL);
	xTaskCreate(base_check, "base_check", 2048, NULL, 10, NULL);
}

void alarm_set(void *args){
	//when alert occurs, send notification to the base
	while(1){
		while(xSemaphoreTake(alarm_semaphore, portMAX_DELAY) == pdFALSE)
			continue;
		ESP_LOGI(TAG_MAIN, "ALARM ON!!!");
		const uint8_t data = ALARM_OCCURRED;
		esp_now_send(simple_broadcast_mac, &data, sizeof(data));
		alarm_reset();
	}
}

void alarm_reset(void){
	//alarm reactivation is delayed
	alarm_status = ALARM_GOING;
	vTaskDelay(ALARM_GOING_TIME);
	alarm_status = ALARM_ON;
}

void base_check(void *args){
	//periodic notification to base
	while(1){
		if(alarm_status == ALARM_ON){
			const uint8_t data = IM_ALIVE;
			esp_now_send(simple_broadcast_mac, &data, sizeof(data));
		}
		vTaskDelay(ALIVE_SIG_TIME);
	}
}


void gpio_isr_handler(void *arg){
	///callback function for gpio interrupt
	if(alarm_status == ALARM_ON){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(alarm_semaphore, &xHigherPriorityTaskWoken);
		if(xHigherPriorityTaskWoken == pdTRUE)
			portYIELD_FROM_ISR();
	}
}

static void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status){
	//callback function for espnow send
	if(mac_addr == NULL){
		ESP_LOGE(TAG_ESPNOW, "NULL mac address in send callback");
		return;
	}
	if(status == ESP_NOW_SEND_FAIL)
		ESP_LOGV(TAG_ESPNOW, "send failed");
	else
		ESP_LOGV(TAG_ESPNOW, "send successful");
}
