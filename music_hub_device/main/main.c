#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "nvs_flash.h" //non volatile storage
#include "ping/ping_sock.h"

#include "task_http.h"
#include "task_test_com.h"
#include "task_sdcard.h"
#include "task_bt_dev.h"
#include "private.h"


#include "client_serv_prot.h"


volatile QueueHandle_t QueueHttpBtdev;


void wifi_connect(void *task_param);

void app_main(void)
{
    
    nvs_flash_init();
    // ???? may be unite WIFI and TASK_HTTP
    // xTaskCreate(task_test_com, "TEST_COM", 1 << 10, NULL, 3, NULL);
    xTaskCreate(wifi_connect, "WIFI", 1 << 12, NULL, 1, NULL);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    QueueHttpBtdev = xQueueCreate(1, sizeof(t_csp_track_pack));
    xTaskCreate(task_http, "TASK_HTTP", 1 << 15, NULL, 2, NULL);
    // xTaskCreate(task_sdcard, "TASK_SDCARD", 1 << 14, NULL, 2, NULL);

    xTaskCreate(task_bt_dev, "TASK_BT_DEV", 1 << 12, NULL, 1, NULL);

}


static void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data)
{
    
    if(event_id == WIFI_EVENT_STA_START)
    {
        printf("WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        printf("WiFi lost connection\n");
        uint8_t retry_num = 0;
        if(retry_num < 5)
        {
            esp_wifi_connect();
            retry_num++;
            printf("Retrying to Connect...\n");
        }
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        printf("Wifi got IP...\n\n");
    }
}


void wifi_connect(void *task_param)
{
    printf("kek\n");
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_conf = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_conf);
    printf("kek\n");
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_conf = {
        .sta = {
            .ssid = WIFI_ID,
            .password = WIFI_PASSWORD
        }
    };
    printf("kek\n");
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_conf);
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_err_t err_key = esp_wifi_connect();
    printf("connect key = %s", esp_err_to_name(err_key));
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



