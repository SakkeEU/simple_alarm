#include <string.h>
#include "simple_wifi.h"
#include "simple_espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_system.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define TAG_MAIN "SIMPLE_MAIN"

#define SIMPLE_GPIO_BUTTON_PIN GPIO_NUM_4
#define SIMPLE_GPIO_BUZZER_PIN GPIO_NUM_5
//trigger alert after ALARM_TIME seconds of no comm with alert device
#define ALARM_TIME 15000/portTICK_RATE_MS
//alert duration
#define ALARM_GOING_TIME 5000/portTICK_RATE_MS

typedef enum{
	ALARM_OFF,
	ALARM_ON,
	ALARM_GOING,
}alarm_status_t;

enum{
	NO_ALARM_OCCURRED,
	ALARM_OCCURRED,
};

static TimerHandle_t alarm_timer = NULL;
static TaskHandle_t base_clock_alarm_handle = NULL;
static TaskHandle_t evaluate_received_data_handle = NULL;
static SemaphoreHandle_t base_alarm_semaphore = NULL;
static alarm_status_t alarm_status = ALARM_ON;
static uint8_t *received_data;

void base_alarm_reset(void);
void base_clock_alarm(void *args);
void evaluate_received_data(void *args);
void buzzer_on(void);
void buzzer_off(void);
void timer_cb(TimerHandle_t xTimer);
void gpio_isr_handler(void *arg);
static void receive_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);

void app_main(void){
	uint8_t chan = 1;
	simple_wifi_init(SIMPLE_STATION, chan);
	simple_espnow_init(NULL ,receive_cb, chan);
	
	gpio_num_t buzzer_pin = SIMPLE_GPIO_BUZZER_PIN;
    gpio_config_t bconf = {
		.pin_bit_mask = 1 << buzzer_pin,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = 0,
		.pull_down_en = 0,
		.intr_type = 0,
		};	
	gpio_config(&bconf);
	gpio_set_level(SIMPLE_GPIO_BUZZER_PIN, 0);
	
    gpio_num_t button_pin = SIMPLE_GPIO_BUTTON_PIN;
    gpio_config_t gconf = {
		.pin_bit_mask = 1 << button_pin,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = 0,
		.pull_down_en = 0,
		.intr_type = GPIO_INTR_NEGEDGE,
		};	
	gpio_config(&gconf);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(button_pin, gpio_isr_handler, NULL);
	
	base_alarm_semaphore = xSemaphoreCreateBinary();
	
	xTaskCreate(base_clock_alarm, "base_clock_alarm", 2048, NULL, 10, &base_clock_alarm_handle);
	xTaskCreate(evaluate_received_data, "evaluate_received_data", 2048, NULL, 10, &evaluate_received_data_handle);
	alarm_timer = xTimerCreate("alarm_timer", ALARM_TIME, pdFALSE, (void *) 1, timer_cb);
	while(xTimerStart(alarm_timer, 1000/portTICK_RATE_MS) == pdFALSE)
		continue;
	ESP_LOGI(TAG_MAIN, "Timer started!\n");
}
void base_alarm_reset(void){
	while(xTimerReset(alarm_timer, 1000/portTICK_RATE_MS) == pdFALSE) 
		continue;
	ESP_LOGI(TAG_MAIN, "Timer restarted!\n");
}

void base_clock_alarm(void *args){
	while(1){
		while(xTaskNotifyWait(0x00, 0xFFFFFFFF, NULL, portMAX_DELAY) == pdFALSE) //wait for notify from timer_cb
			continue;
		if(alarm_status == ALARM_ON){
			alarm_status = ALARM_GOING;
			ESP_LOGI(TAG_MAIN, "ALARM! No comm from alarm device.\n");
			buzzer_on();
			xSemaphoreTake(base_alarm_semaphore, ALARM_GOING_TIME); //press button to silence the alert
			buzzer_off();
			base_alarm_reset();
			alarm_status = ALARM_ON;
		}else
			base_alarm_reset();
	}
}

void evaluate_received_data(void *args){
	//checks what the alarm device sent, triggers an alert if necessary
	while(1){
		while(xTaskNotifyWait(0x00, 0xFFFFFFFF, NULL, portMAX_DELAY) == pdFALSE) //wait for notify from receive_cb
			continue;
		if(alarm_status == ALARM_ON){
			if(*received_data == NO_ALARM_OCCURRED){
				alarm_status = ALARM_OFF;
				ESP_LOGI(TAG_MAIN, "no alarm occurred");
				base_alarm_reset();
				alarm_status = ALARM_ON;
			}else{
				alarm_status = ALARM_GOING;
				ESP_LOGI(TAG_MAIN, "alarm occurred");
				buzzer_on();
				xSemaphoreTake(base_alarm_semaphore, ALARM_GOING_TIME); //press button to silence the alert
				buzzer_off();
				base_alarm_reset();
				alarm_status = ALARM_ON;
			}
		}
		free(received_data);
	}
}

void buzzer_on(void){
	gpio_set_level(SIMPLE_GPIO_BUZZER_PIN, 1);
}
void buzzer_off(void){
	gpio_set_level(SIMPLE_GPIO_BUZZER_PIN, 0);
}

void timer_cb(TimerHandle_t xTimer){
	//callback function for timer
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(base_clock_alarm_handle, &xHigherPriorityTaskWoken);
	if(xHigherPriorityTaskWoken == pdTRUE)
		portYIELD_FROM_ISR();
}

void gpio_isr_handler(void *arg){
	///callback function for gpio interrupt
	if(alarm_status == ALARM_GOING){
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(base_alarm_semaphore, &xHigherPriorityTaskWoken);
		if(xHigherPriorityTaskWoken == pdTRUE)
			portYIELD_FROM_ISR();
	}
}

static void receive_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len){
	//callback function for espnow receive
	if(mac_addr == NULL){
		ESP_LOGE(TAG_ESPNOW, "NULL mac address in receive callback");
		return;
	}
	received_data = malloc(data_len);
	if (received_data == NULL) {
        ESP_LOGE(TAG_ESPNOW, "Malloc receive data fail");
        return;
    }
    memcpy(received_data, data, data_len);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(evaluate_received_data_handle, &xHigherPriorityTaskWoken);
	if(xHigherPriorityTaskWoken == pdTRUE)
		portYIELD_FROM_ISR();
}
