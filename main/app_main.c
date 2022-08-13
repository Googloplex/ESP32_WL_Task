/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_timer.h"

#include "ssd1306.h"
#include "font8x8_basic.h"

#define tag "SSD1306"


#define EXAMPLE_ESP_WIFI_SSID      "TP-Link_5F92"

#define BLINK_GPIO 2
static uint8_t s_led_state = 0;

static uint8_t rssi_val;
static char persent[30];

static void load_blink_timer_callback(void* arg);
esp_timer_handle_t load_blink_timer;
//static void oneshot_timer(void* arg);

static int s_retry_num = 0;

static const char *TAG = "\nMQTT_EXAMPLE";
static const char *TAG2 = "\nMQTT_RSSI";

SSD1306_t dev;
int center, top, bottom;
char lineChar[20];

//uint16_t g_scan_ap_num;
// wifi_ap_record_t *g_ap_list_buffer;

static void led_off(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}
static void led_on(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, !s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

uint8_t rssi_to_persent(int rssi) {
    uint8_t percent = 0;
    uint8_t rssi_arr[100];
    uint8_t rssi_min = 135;
    uint8_t rssi_max = 235;
    if(rssi >= rssi_max) return 100;
    if(rssi <= rssi_min) return 0;
    for(; percent!=100; percent++){
        rssi_arr[percent] = rssi_min + percent;
        if(rssi_arr[percent]==rssi) return percent;
    }
    return 0;
}

void init_tims(void) {

    const esp_timer_create_args_t load_blink_timer_args = {
            .callback = &load_blink_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&load_blink_timer_args, &load_blink_timer));
    /* The timer has been created but is not running yet */

    //ESP_ERROR_CHECK(esp_timer_start_periodic(load_blink_timer, 100000));

    // ESP_ERROR_CHECK(esp_timer_stop(load_blink_timer));
    // gpio_set_level(BLINK_GPIO, 0);
}

void oled_init(void){
    ESP_LOGI(TAG, "INTERFACE is i2c");
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    dev._flip = true;
    ESP_LOGI(TAG, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
    //ssd1306_display_text(&dev, 0, "lol", 5, false);
    //ssd1306_display_text(&dev, 1, "kek", 5, false);
    ssd1306_display_text(&dev, 0, "SSD1306 128x64", 14, false);
	ssd1306_display_text(&dev, 1, "ABCDEFGHIJKLMNOP", 16, false);
	ssd1306_display_text(&dev, 2, "abcdefghijklmnop",16, false);
	ssd1306_display_text(&dev, 3, "Hello World!!", 13, false);
	ssd1306_display_text(&dev, 4, "SSD1306 128x64", 14, true);
	ssd1306_display_text(&dev, 5, "ABCDEFGHIJKLMNOP", 16, true);
	ssd1306_display_text(&dev, 6, "abcdefghijklmnop",16, true);
	ssd1306_display_text(&dev, 7, "Hello World!!", 13, true);
	int pages = 7;
}




/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    esp_timer_handle_t load_blink_timer_even = (esp_timer_handle_t)(void*)load_blink_timer; //  (esp_timer_handle_t*)load_blink_timer;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_ERROR_CHECK(esp_timer_start_periodic(load_blink_timer_even, 100000));
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");   //   -- ready
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 0, "MQTT -- ready", 23, false);
        
        ESP_ERROR_CHECK(esp_timer_stop(load_blink_timer_even));
        gpio_set_level(BLINK_GPIO, 0);

        sprintf(&persent, "Signal %d%%", rssi_val);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", &persent, 0, 1, 0);
        ssd1306_display_text(&dev, 1, &persent, strlen(&persent), false);
        //ssd1306_hardware_scroll(&dev, SCROLL_RIGHT);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/signal", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");  //  -- lost
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 0, "MQTT -- lost", 23, false);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        //printf("MQTT_EVENT_DATA  %s", strstr(event->data, "1"));
        //led_on();
        ssd1306_display_text(&dev, 3, " TOPIC:", 6, false);
        ssd1306_display_text(&dev, 4, event->topic, 13, false);
        ssd1306_display_text(&dev, 5, "DATA:", 5, false);
        ssd1306_display_text(&dev, 6, event->data, 1, false);
        if(strncmp(event->data, "1", 1) == 0) {
            led_on();
             ESP_LOGI(TAG, "MQTT_led_on()");
            }
        if(strncmp(event->data, "0", 1) == 0) {
            led_off();
             ESP_LOGI(TAG, "MQTT_led_off()");
            }
        if(strncmp(event->topic, "/topic/signal", 13) == 0) {
            sprintf(&persent, "Signal quality %d%%", rssi_val);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos1", &persent, 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        }
        printf("\nTOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("\nDATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}



void get_ap_records(void)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) EXAMPLE_ESP_WIFI_SSID;
    uint8_t i;

    esp_wifi_scan_start(&scan_config, true);

    #define DEFAULT_SCAN_LIST_SIZE 1

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_get_ap_records(&number, ap_info);
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG2, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG2, "RSSI \t\t%d", ap_info[i].rssi);
        //rssi = (*rssi_to_persent)((uint8_t)ap_info[i].rssi);
        rssi_val = rssi_to_persent((uint8_t)ap_info[i].rssi);
        ESP_LOGI(TAG2, "PERCENTE \t\t%d%%", rssi_to_persent((uint8_t)ap_info[i].rssi));
        ESP_LOGI(TAG2, "Channel \t\t%d\n", ap_info[i].primary);
    }


}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    configure_led();
    
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    init_tims();
    oled_init();
    mqtt_app_start();
    get_ap_records();
}

static void load_blink_timer_callback(void* arg)
{
    static uint8_t status = 0;
    if(status==1) {
        led_off();
        status = 0;
   } else {
        led_on();
    	status = 1;
   }
    // int64_t time_since_boot = esp_timer_get_time();
    // ESP_LOGI(TAG, "Periodic timer called, time since boot: %lld us", time_since_boot);
}
