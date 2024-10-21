#include "task_http.h"

#include "esp_http_client_user.h"

#include "base_types.h"

#include "client_serv_prot.h"
#include "private.h"

#include "cserver_com_sender.h"
#include "cserver_com_receiver.h"


#define MAX_HTTP_OUTPUT_BUFFER 25596

#define SEND_ATTEMPS_MAX 1


#define WAIT_RESP_COUNT_MAX     50
#define WAIT_RESP_TIME_GAP      (100 / portTICK_PERIOD_MS)


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
static volatile u8_t _http_status = E_HTTP_STATUS_IDEL;
static u8_t client_is_open = 0;


void task_http(void *task_param)
{
    esp_http_client_config_t config = {
        .url = SERVER_ADDRES,
        .buffer_size = MAX_HTTP_OUTPUT_BUFFER
    };
    _client = esp_http_client_init_user(&config);

    u32_t task_delay = 1000 / portTICK_PERIOD_MS;
    s32_t read_res = -1;
    while(1)
    {
        if ( _http_status == E_HTTP_STATUS_IDEL )
        {
            task_delay = 1 / portTICK_PERIOD_MS;
            _http_status = E_HTTP_STATUS_WORK;
            read_res = -1;
            printf("WRITE\n");
            if ( !_device_id)
                send_syn();
            else
                send_statys();
        } else if (_http_status == E_HTTP_STATUS_WORK)
        {
            printf("READ\n");
            esp_http_client_set_timeout_ms_user(_client, 1);
            if (esp_http_client_poll_read_user(_client) > 0)
            {
                read_res = read_response();
                if (read_res > 0)
                    _http_status = E_HTTP_STATUS_IDEL;
                else
                    _http_status = http_repeat_send() < 0 ? E_HTTP_STATUS_IDEL : E_HTTP_STATUS_WORK;
                // DBG: remove sending from reading response
                if (repeat_msg.pack_type == ECSP_COM_GET_TRACK)
                {
                    _http_status = E_HTTP_STATUS_WORK;
                    printf("GET TRACK\n");
                }
                    
            }
            
            task_delay = WAIT_RESP_TIME_GAP;
        } else
        {
            printf("HTTP SHISHEL ERROR\n");
            task_delay = 1000 / portTICK_PERIOD_MS;
        }
        // vTaskDelay(task_delay);
        vTaskDelay(WAIT_RESP_TIME_GAP);
        // if ( _http_status == E_HTTP_STATUS_IDEL )
        // {
        //     task_delay = 1000 / portTICK_PERIOD_MS;
        //     if ( !_device_id)
        //         send_syn();
        //     else
        //         send_statys();
        // } else if (_http_status == E_HTTP_STATUS_WAIT_RESP || _http_status == E_HTTP_STATUS_WORK)
        // {
        //     
        // }
        // vTaskDelay(task_delay);
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

/*
if content_length == 0 -> E (23575) HTTP_CLIENT: transport_read: error - 0 
should close connection and repeat sending 
HTTP_TRANSPORT_ERROR
*/
static int read_response(void)
{
    static char buf[MAX_HTTP_OUTPUT_BUFFER] = { 0 };
    
    esp_http_client_set_timeout_ms_user(_client, 200);
    esp_http_client_fetch_headers_user(_client);
    int content_length = esp_http_client_read_user_response_user(_client, buf, MAX_HTTP_OUTPUT_BUFFER);
    esp_http_client_close_user(_client);
    client_is_open = 0;
    printf("content_length = %d HTTP: CLOSE USER\n", content_length);
    // int content_length = esp_http_client_read(_client, buf, MAX_HTTP_OUTPUT_BUFFER);
    
    s32_t parse_res = parse_responce((u8_t*)buf, content_length);
    if ( parse_res < 0) 
        printf("ERROR: Cannot parse\n");
        
    return parse_res;
}


s8_t http_repeat_send(void)
{
    vTaskDelay(5 / portTICK_PERIOD_MS);
    if ( !repeat_msg.repeat_count )
        return -1;
    printf("DBG: In repeat\n");
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
        
    result = (buf_len == 0) ? send_get_req() : send_post_req(buf, buf_len);

    if (result < 0 || result < buf_len)
    {
        printf("ERROR: sending\n");
        // DBG Update status working
        _http_status = E_HTTP_STATUS_IDEL;
    }
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
    esp_err_t err = esp_http_client_open_user(_client, 0);
    if (err != ESP_OK) {
        printf("ERROR: Send GET, val - %d\n", err);
        return err;
    }
    client_is_open = 1;
    return ESP_OK;
}


static int send_post_req(u8_t* buf, u16_t buf_len)
{
    esp_http_client_set_method_user(_client, HTTP_METHOD_POST);
    if (client_is_open)
        return esp_http_client_write_user(_client, (char*)buf, buf_len);

    esp_err_t err = esp_http_client_open_user(_client, buf_len);
    printf("HTTP: OPEN USER\n");
    if (err != ESP_OK) {
        client_is_open = 0;
        printf("ERROR: Send post, val - %d\n", err);
        return ((int)err < 0) ? (int)err : -(int)err;
    }
    client_is_open = 1;
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

