/* 

Test task for the position of embedded developer in WebbyLab:
Expand IoT devices based on ESP32

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

#define MQTT_LOGIN     "8d68402720d57777866dec36ddbb88cdbd4714e5688dd9ea3428ebe31321fecc"
#define DEVICE_ID      "device1661249288868575"

#define DEVICE         MQTT_LOGIN "/sweet-home/" DEVICE_ID
#define DEVICE_ERROR   MQTT_LOGIN "/errors/sweet-home/" DEVICE_ID

#define BLINK_GPIO 2
static uint8_t s_led_state = 0;

static char persent[30];

uint8_t get_rssi(void);


static void load_blink_timer_callback(void *args);
esp_timer_handle_t load_blink_timer;

static void heartbeat_timer_callback(void *args);
esp_timer_handle_t heartbeat_timer;

esp_mqtt_client_handle_t client_info;

static const char *TAG = "MQTT_EXAMPLE";
static const char *TAG2 = "MQTT_RSSI";

SSD1306_t dev;
//int center, top, bottom;
//char lineChar[20];


static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

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


void init_tims(void) {

    const esp_timer_create_args_t load_blink_timer_args = {
            .callback = &load_blink_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    const esp_timer_create_args_t heartbeat_timer_args = {
            .callback = &heartbeat_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&load_blink_timer_args, &load_blink_timer));

    ESP_ERROR_CHECK(esp_timer_create(&heartbeat_timer_args, &heartbeat_timer));
    /* The timer has been created but is not running yet */
}

void oled_init(void){  /* Config i2c bus and init OLED */
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

	ssd1306_display_text(&dev, 2, "The device is", 13, false);
	ssd1306_display_text(&dev, 3, "starting,", 9, false);
	ssd1306_display_text(&dev, 4, "please wait", 11, false);
	ssd1306_display_text(&dev, 5, "...",3, false);              //TODO -- add loadbar in timer  
}

static void log_error_if_nonzero(const char *message, int error_code)
{   /* Nonzerro detection of MQTT error in event_handler */
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


uint8_t rssi_to_persent(uint8_t rssi) {  /* Convert RSSI value to percents */
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

uint8_t get_rssi(void)
{   /* Getting RSSI value from ap_records */
    #define DEFAULT_SCAN_LIST_SIZE 1

    uint8_t rssi = 0;
    wifi_scan_config_t scan_config = { 0 };
    esp_wifi_scan_start(&scan_config, true);

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
        rssi = rssi_to_persent((uint8_t)ap_info[i].rssi);
        ESP_LOGI(TAG2, "PERCENTE \t\t%d%%", rssi_to_persent((uint8_t)ap_info[i].rssi));
        ESP_LOGI(TAG2, "Channel \t\t%d\n", ap_info[i].primary);
    }
    return rssi;
}


enum TOPICS
{
    INVALID_TOPIC,
    LED_CONTROL_TOPIC,
    COMANDS_TOPIC
};


static uint8_t get_topic(esp_mqtt_event_handle_t event){
        /*  Checking if a string matches  */
        if(strncmp(event->topic, DEVICE "/signal/get-signal/set", event->topic_len) == 0) return COMANDS_TOPIC;
        if(strncmp(event->topic, DEVICE "/led/led-state/set", event->topic_len) == 0) return LED_CONTROL_TOPIC;
        return INVALID_TOPIC;
}


static void mqtt_data_hendler(esp_mqtt_event_handle_t event){
        int msg_id;
        esp_mqtt_client_handle_t client = event->client;  // init for esp_mqtt_client_publish()

        /*  Ptint curent Topic and Data on OLED  */
        ssd1306_display_text(&dev, 3, "TOPIC: ->", 6, false);
        ssd1306_display_text(&dev, 4, event->topic, event->topic_len, false);
        ssd1306_display_text(&dev, 5, "DATA: ->", 5, false);
        ssd1306_display_text(&dev, 6, event->data, event->data_len+10, false);
        
        /*  Ptint curent Topic and Data in term  */
        printf("\nTOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("\nDATA=%.*s\r\n", event->data_len, event->data);

        /*  Get event topic for more comfort  */
        switch (get_topic(event)) {
        case LED_CONTROL_TOPIC:
            /*  LED control  */
            if(strncmp(event->data, "true", event->data_len) == 0) {
                led_on();
                ESP_LOGI(TAG, "MQTT_led_on()");
                msg_id = esp_mqtt_client_publish(client, DEVICE "/led/led-state", "true", 0, 1, 0);
            	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            } else if(strncmp(event->data, "false", event->data_len) == 0) {
                led_off();
                ESP_LOGI(TAG, "MQTT_led_off()");
                msg_id = esp_mqtt_client_publish(client, DEVICE "/led/led-state", "false", 0, 1, 0);
            	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            } else {
                msg_id = esp_mqtt_client_publish(client, DEVICE_ERROR "/led/led-state", "{\" code \": '404',\" message \": 'Invalid comand. Put \"true\" or \"false\" to switch on or switch off led.'}", 0, 1, 0);
            	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            }
            break;
        case COMANDS_TOPIC:
            /*  Get device info  */
            if(strncmp(event->data, "true", event->data_len) == 0) {
                msg_id = esp_mqtt_client_publish(client, DEVICE "/signal/get-signal", "true", 0, 1, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                sprintf(&persent, "%d", get_rssi());
                msg_id = esp_mqtt_client_publish(client, DEVICE "/signal/set-signal", &persent, 0, 1, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            } else {
                msg_id = esp_mqtt_client_publish(client, DEVICE_ERROR "/led/led-state", "{\" code \": '505',\" message \": 'Invalid comand.'}", 0, 1, 0);
            	ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            }
            break;
        case INVALID_TOPIC:
            /*  You subscribed but this topic is not add in the topic handler get_topic().  */
            ESP_LOGI(TAG, "Error. You subscribed but this topic is not add in the topic handler get_topic().");
            break;
        default:
            break;
        }
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
    int msg_id;

    client_info = (esp_mqtt_client_handle_t)(void*)client;

    esp_timer_handle_t load_blink_timer_even = (esp_timer_handle_t)(void*)load_blink_timer; 
    
    esp_timer_handle_t heartbeat_timer_even = (esp_timer_handle_t)(void*)heartbeat_timer;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_ERROR_CHECK(esp_timer_start_periodic(load_blink_timer_even, 50000));    /*  Start load_blink_timer  */
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_ERROR_CHECK(esp_timer_stop(load_blink_timer_even));   /*  Stop load_blink_timer  */
        gpio_set_level(BLINK_GPIO, 0);   /*  Set led off, if it set on*/

        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");   //   -- ready

        
        /*  Ptint MQTT -- ready status and percent of WI-FI signal on OLED  */
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 0, "MQTT -- ready", 23, false);
        sprintf(&persent, "Signal %d%%", get_rssi());
        ssd1306_display_text(&dev, 1, &persent, strlen(&persent), false);


        /*  Config device init topics  */
        esp_mqtt_client_publish(client, DEVICE "/$name", "MQTT-2SC", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$fw/name", "draft", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$fw/version", "2", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$localip", "192.168.1.228", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$mac", "3C:71:BF:FF:78:40", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$implementation", "ESP32", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$state", "ready", 0, 1, 0);


        /*  Config Wi-Fi init topics  */
        esp_mqtt_client_publish(client, DEVICE "/$options", "signal",  0, 1, 0);
        sprintf(&persent, "%d", get_rssi());
        esp_mqtt_client_publish(client, DEVICE "/$options/signal", &persent,  0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$options/signal/$name", "Wi-Fi",  0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$options/signal/$settable", "false",  0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$options/signal/$retained", "true",  0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$options/signal/$datatype", "integer",  0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/$options/signal/$unit", "%%",  0, 1, 0);

        /*  Init nodes led and signal  */
        esp_mqtt_client_publish(client, DEVICE "/$nodes", "led, signal", 0, 1, 0);

        /*  Config led node  */
        esp_mqtt_client_publish(client, DEVICE "/led/$name", "LED-GPIO", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/led/$state", "ready", 0, 1, 0);

        /*  Add led-state propertti to led node  */
        esp_mqtt_client_publish(client, DEVICE "/led/$propertties", "led-state", 0, 1, 0);

        /*  Config led-state propertti  */
        esp_mqtt_client_publish(client, DEVICE "/led/led-state", "false", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/led/led-state/$name", "ledstate", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/led/led-state/$settable", "true", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/led/led-state/$retained", "true", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/led/led-state/$datatype", "boolean", 0, 1, 0);

        /*  Config signal node  */
        esp_mqtt_client_publish(client, DEVICE "/signal/$name", "Signal", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/$state", "ready", 0, 1, 0);

        /*  Add get-signal and set-signal propertties to signal node  */
        esp_mqtt_client_publish(client, DEVICE "/signal/$propertties", "get-signal, set-signal", 0, 1, 0);

        /*  Config get-signal propertti  */
        esp_mqtt_client_publish(client, DEVICE "/signal/get-signal", "false", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/get-signal/$name", "getsignal", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/get-signal/$settable", "true", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/get-signal/$retained", "false", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/get-signal/$datatype", "boolean", 0, 1, 0);

        /*  Config set-signal propertti  */
        sprintf(&persent, "%d", get_rssi());
        esp_mqtt_client_publish(client, DEVICE "/signal/set-signal", &persent, 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/set-signal/$name", "setsignal", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/set-signal/$settable", "false", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/set-signal/$retained", "false", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/set-signal/$datatype", "integer", 0, 1, 0);
        esp_mqtt_client_publish(client, DEVICE "/signal/set-signal/$unit", "%%",  0, 1, 0);


        /*  Subscribe to "/led/led-state/set" topic for control BLINK_GPIO led */
        msg_id = esp_mqtt_client_subscribe(client, DEVICE "/led/led-state/set", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        /*  Subscribe to "/signal/get-signal/set" topic to get signal val  */
        msg_id = esp_mqtt_client_subscribe(client, DEVICE "/signal/get-signal/set", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        /*  Add subscriber heer, and add the topic handler in get_topic() */
        
        
        ESP_ERROR_CHECK(esp_timer_start_periodic(heartbeat_timer_even, 8000000));    /*  Start heartbeat_timer  */

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");  //  -- lost
        ESP_ERROR_CHECK(esp_timer_stop(heartbeat_timer_even));   /*  Stop heartbeat_timer  */
        /*  Ptint MQTT -- lost status on OLED  */
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 0, "MQTT -- lost", 23, false);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        printf(TAG, "MQTT_EVENT_DATA");
        mqtt_data_hendler(event);  /*  Go to main event data hendler  */
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

static void mqtt_app_start(void)
{
    /* CONFIG_BROKER_URL configuration is specified in sdkconfig */
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
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

    init_tims();
    configure_led();
    oled_init();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    
    mqtt_app_start();
    
}

static void load_blink_timer_callback(void *args)
{   /*  Timer for blink led  */
    static uint8_t status = 0;
    if(status==1) {
        led_off();
        status = 0;
   } else {
        led_on();
    	status = 1;
   }
}

static void heartbeat_timer_callback(void *args)
{   /*  Timer for heartbeat client publish */
    int msg_id;
    esp_mqtt_client_handle_t client_timer = (esp_mqtt_client_handle_t)(void*)client_info;
    msg_id = esp_mqtt_client_publish(client_timer, DEVICE "/$heartbeat", "ping", 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}

