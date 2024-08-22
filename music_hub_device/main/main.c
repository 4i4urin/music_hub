#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "nvs_flash.h" //non volatile storage
#include "ping/ping_sock.h"

#include "task_http.h"
#include "task_test_com.h"
#include "task_sdcard.h"
#include "private.h"


#include "client_serv_prot.h"


volatile QueueHandle_t QueueHttpSD;


void wifi_connect(void *task_param);

void app_main(void)
{
    
    nvs_flash_init();
   
    // xTaskCreate(task_test_com, "TEST_COM", 1 << 10, NULL, 3, NULL);
    xTaskCreate(wifi_connect, "WIFI", 1 << 12, NULL, 1, NULL);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    QueueHttpSD = xQueueCreate(1, sizeof(t_csp_track_pack));
    xTaskCreate(task_http, "TASK_HTTP", 1 << 15, NULL, 2, NULL);
    xTaskCreate(task_sdcard, "TASK_SDCARD", 1 << 14, NULL, 2, NULL);

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
        
        // printf("Wifi settings: ssid = %s password = %s\n", wifi_conf.sta.ssid, wifi_conf.sta.password);
        // uint16_t aid = 0;
        // int rssi = 0;
        // esp_wifi_sta_get_aid(&aid);
        // esp_wifi_sta_get_rssi(&rssi);
        // printf("aid = %d rssi = %d\n", aid, rssi);
        // vTaskDelay(portMAX_DELAY);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    // optionally, get callback arguments
    // const char* str = (const char*) args;
    // printf("%s\r\n", str); // "foo"
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n",
           recv_len, ip_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", ip_ntoa(&target_addr), seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%d packets transmitted, %d received, time %dms\n", transmitted, received, total_time_ms);
}



void initialize_ping()
{
    /* convert URL to IP address */
    ip_addr_t target_addr;
    target_addr.u_addr.ip4.addr = PING_IP; 
    target_addr.type = IPADDR_TYPE_V4;
    

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;     
    ping_config.count = 5;    // ping in infinite mode, esp_ping_stop can stop it
    ping_config.interval_ms = 5000;
    ping_config.timeout_ms = 3000;

    /* set callback functions */
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end = test_on_ping_end;
    // cbs.cb_args = "foo";  // arguments that feeds to all callback functions, can be NULL
    // cbs.cb_args = eth_event_group;

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
}
