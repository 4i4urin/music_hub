#include "task_http.h"

#include "esp_http_client_user.h"

#include "base_types.h"

#include "client_serv_prot.h"
#include "private.h"

#include "cserver_com_sender.h"
#include "cserver_com_receiver.h"


#define MAX_HTTP_OUTPUT_BUFFER 25596

#define SEND_ATTEMPS_MAX 5


static int read_response(void);
static int send_get_req(void);
static int send_post_req(u8_t* buf, u16_t buf_len);
static void set_req_url(u8_t pack_type);

static esp_http_client_handle_t _client = { 0 };


#define BUF_SIZE_REPEAT_MSG 64
static struct _repeat_msg
{
    u8_t repeat_count;
    u8_t buf[BUF_SIZE_REPEAT_MSG];
    u16_t buf_len;
    u8_t pack_type;
} repeat_msg;

static u8_t _device_id = 0;
static u8_t _http_status = E_HTTP_STATUS_IDEL;


void task_http(void *task_param)
{
    esp_http_client_config_t config = {
        .url = SERVER_ADDRES,
        .buffer_size = MAX_HTTP_OUTPUT_BUFFER
    };
    _client = esp_http_client_init_user(&config);

    u32_t task_delay = 1000 / portTICK_PERIOD_MS;

    while(1)
    {
        if ( _http_status == E_HTTP_STATUS_IDEL )
        {
            task_delay = 1000 / portTICK_PERIOD_MS;
            if ( !_device_id)
                send_syn();
            else
                send_statys();
        }
        
        esp_http_client_set_timeout_ms_user(_client, 1);
        if (esp_http_client_poll_read_user(_client) > 0)
            read_response();
        
        if ( _http_status == E_HTTP_STATUS_WORK)
            task_delay = 10 / portTICK_PERIOD_MS;

        vTaskDelay(task_delay);
    }
}


u8_t http_get_device_id(void)
{
    return _device_id;
}


void http_set_status(u8_t status)
{
    _http_status = status;
}


u8_t http_get_status(void)
{
    return _http_status;
}


void http_set_device_id(u8_t device_id)
{
    _device_id = device_id;
}


static int read_response(void)
{
    static char buf[MAX_HTTP_OUTPUT_BUFFER] = { 0 };
    
    esp_http_client_set_timeout_ms_user(_client, 200);
    esp_http_client_fetch_headers_user(_client);
    int content_length = esp_http_client_read_user_response_user(_client, buf, MAX_HTTP_OUTPUT_BUFFER);
    // int content_length = esp_http_client_read(_client, buf, MAX_HTTP_OUTPUT_BUFFER);
    esp_http_client_close_user(_client);

    s32_t parse_res = parse_responce((u8_t*)buf, content_length);
    if ( parse_res < 0) 
        printf("ERROR: Cannot parse\n");
    return 0;
}


s8_t http_repeat_send(void)
{
    vTaskDelay(5 / portTICK_PERIOD_MS);
    if ( !repeat_msg.repeat_count )
        return -1;
    
    repeat_msg.repeat_count -= 1;
    http_send_to_serv(repeat_msg.buf, repeat_msg.buf_len, repeat_msg.pack_type);
    return 0;
}


void http_send_to_serv(u8_t* buf, u16_t buf_len, u8_t pack_type)
{
    if (buf_len < BUF_SIZE_REPEAT_MSG)
    {// copy mesage to repeat if transmition fail
        memcpy(repeat_msg.buf, buf, buf_len);
        repeat_msg.buf_len = buf_len;
        repeat_msg.pack_type = pack_type;
        repeat_msg.repeat_count = SEND_ATTEMPS_MAX;
    }
    set_req_url(pack_type);
    esp_http_client_set_timeout_ms_user(_client, 500);
    int result = -1;
    int send_attemps = 0;
    while (send_attemps < SEND_ATTEMPS_MAX && result < 0)
    {
        if (send_attemps)
            printf("WARNING: attemp to send: %d\n", send_attemps);
        
        result = (buf_len == 0) ? send_get_req() : send_post_req(buf, buf_len);
        send_attemps += 1;
        if (result < 0)
            vTaskDelay(2 / portTICK_PERIOD_MS);
    }

    if (result < 0 || result < buf_len)
        printf("ERROR: sending\n");
}

// HTTP FUNC
static void set_req_url(u8_t pack_type)
{
    char link[MAX_LINK_LEN] = { 0 };
    strcpy(link, SERVER_ADDRES);
    switch (pack_type)
    {
        case ECSP_CONNECT:
            strncat(link, URL_CONNECT, MAX_LINK_LEN - strlen(SERVER_ADDRES));
            break;
        case ECSP_DISCONNECT: 
            strncat(link, URL_DISCONNECT, MAX_LINK_LEN - strlen(SERVER_ADDRES));
            break;
        case ECSP_STATUS:
            strncat(link, URL_STATUS, MAX_LINK_LEN - strlen(SERVER_ADDRES));
            break;
        case ECSP_COM_GET_TRACK: 
            strncat(link, URL_TRACK, MAX_LINK_LEN - strlen(SERVER_ADDRES));
            break;
        case ECSP_COM_PAUSE: 
        case ECSP_COM_RESUME: 
        case ECSP_COM_NEXT: 
        case ECSP_COM_PREV: 
        case ECSP_COM_REPEAT: 
        case ECSP_COM_VOL_INC: 
        case ECSP_COM_VOL_DEC: 
        case ECSP_COM_SWITCH_LIST: 
        
        case ECSP_ACK: 
        default:
            break;
    }
    esp_err_t err = esp_http_client_set_url_user(_client, link);
    if (err != ESP_OK) {
        printf("ERROR: cannot set url\n");
    }
}


static int send_get_req(void)
{
    esp_http_client_set_method_user(_client, HTTP_METHOD_GET);
    return esp_http_client_open_user(_client, 0);
}


static int send_post_req(u8_t* buf, u16_t buf_len)
{
    esp_http_client_set_method_user(_client, HTTP_METHOD_POST);
    esp_err_t err = esp_http_client_open_user(_client, buf_len);
    if (err != ESP_OK) {
        printf("ERROR: Send post, val - %d\n", err);
        return ((int)err < 0) ? (int)err : -(int)err;
    }
    return esp_http_client_write_user(_client, (char*)buf, buf_len);
}


void print_binary(u8_t* buf, u16_t len)
{
    for (u16_t i = 0; i < len; i++)
    {
        if (i % 10 == 0)
            printf("\n");
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

