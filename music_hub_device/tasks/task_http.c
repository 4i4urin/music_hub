#include "task_http.h"

#include "esp_http_client_user.h"

#include "base_types.h"

#include "client_serv_prot.h"
#include "private.h"

#include "cserver_com_sender.h"
#include "cserver_com_receiver.h"


#define MAX_HTTP_OUTPUT_BUFFER 24700

#define SEND_ATTEMPS_MAX 1


#define WAIT_RESP_COUNT_MAX     50
#define HTTP_WAIT_RESP_TIME_GAP      (100 / portTICK_PERIOD_MS)
#define HTTP_WAIT_TO_REPEAT     (500 / portTICK_PERIOD_MS)


#define E_HTTP_STATUS_SEND      ((u8_t)0xA1)
#define E_HTTP_STATUS_RECEIVE   ((u8_t)0xA2)
#define E_HTTP_STATUS_WAIT      ((u8_t)0xA3)


static int read_response(void);
static int send_get_req(void);
static int send_post_req(u8_t* buf, u16_t buf_len);
static void set_req_url(u8_t pack_type);

static esp_http_client_handle_t _client = { 0 };


#define BUF_SIZE_REPEAT_MSG 64
typedef struct _t_msg_to_serv
{
    u8_t repeat_count;
    u8_t buf[BUF_SIZE_REPEAT_MSG];
    u16_t buf_len;
    u8_t pack_type;
} t_msg_to_serv;
static t_msg_to_serv _msg_to_serv = { 0 };

static u8_t _device_id = 0;
static volatile u8_t _http_status = E_HTTP_STATUS_SEND;
static u8_t _client_is_open = 0;


static s32_t _proc_sending(void);
static s32_t _proc_receiving(void);
static void _close_client(void);
static s32_t _http_send_to_serv(t_msg_to_serv msg);


void task_http(void *task_param)
{
    esp_http_client_config_t config = {
        .url = SERVER_ADDRES,
        .buffer_size = MAX_HTTP_OUTPUT_BUFFER,
        .keep_alive_enable = true,
    };
    _client = esp_http_client_init_user(&config);

    u32_t task_delay = HTTP_WAIT_TO_REPEAT * 2;
    s32_t sending_res = E_HTTP_ERROR_UNEXPECTED;
    s32_t receiving_res = E_HTTP_ERROR_UNEXPECTED;
    u8_t wait_count = 0; 
    while(1)
    {
        if ( _http_status == E_HTTP_STATUS_SEND )
        {
            wait_count = 0;
            printf("WRITE\n");
            sending_res = _proc_sending();

            if (sending_res >= 0)
            {
                _http_status = E_HTTP_STATUS_RECEIVE;
                task_delay = HTTP_WAIT_RESP_TIME_GAP;
            } else if (sending_res == E_HTTP_ERROR_UNEXPECTED)
            {
                printf("LOST connections with server\nRESTART\nRESTART\n");
                // in fact do not know what to do
                _device_id = 0;
                task_delay = HTTP_WAIT_TO_REPEAT * 4;
            } else if (sending_res == E_HTTP_ERROR_REPEAT_SEND || sending_res < 0)
                task_delay = HTTP_WAIT_TO_REPEAT;
            
        } else if (_http_status == E_HTTP_STATUS_RECEIVE)
        {
            printf("READ\n");
            receiving_res = _proc_receiving();

            if (receiving_res == E_HTTP_STATUS_WAIT)
            {
                task_delay = HTTP_WAIT_RESP_TIME_GAP;
                wait_count += 1;
                if (wait_count >= WAIT_RESP_COUNT_MAX)
                {
                    printf("HTTP: can't wait anymore\ncan't wait anymore\ncan't wait anymore\n");
                    _close_client();
                    _http_status = E_HTTP_STATUS_SEND;
                }
            } else if (receiving_res == E_HTTP_STATUS_SEND)
            {
                _http_status = E_HTTP_STATUS_SEND;
                continue;
            }
        }
        vTaskDelay(task_delay);
        // vTaskDelay(HTTP_WAIT_RESP_TIME_GAP);
    }
}

void confirm_receive(void)
{
    _msg_to_serv.pack_type = 0;
}


static s32_t _proc_sending(void)
{
    if (_msg_to_serv.pack_type)
        return _http_send_to_serv(_msg_to_serv);;

    // There is no msg to send. Make handshake or send status
    printf("HTTP: No ready MSG\n");
    if (!_device_id)
        send_syn();
    else 
        send_statys();
    // send_... function only create msg. They doesn't send anything
    // to send one device need to repeat sending
    return E_HTTP_ERROR_REPEAT_SEND;
}


static s32_t _proc_receiving(void)
{
    s32_t read_res = 0;

    esp_http_client_set_timeout_ms_user(_client, 10);
    if (esp_http_client_poll_read_user(_client) <= 0)
        return E_HTTP_STATUS_WAIT;
    
    read_res = read_response();
    if (read_res == E_HTTP_ERROR_WAIT)
        return E_HTTP_STATUS_WAIT;
    else
        return E_HTTP_STATUS_SEND;
}


u8_t http_get_device_id(void)
{
    return _device_id;
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
    
    esp_http_client_set_timeout_ms_user(_client, 300);
    int pre_length = esp_http_client_fetch_headers_user(_client);
    int content_length = 0;
    u8_t try_recieve_count = 0;
    const u8_t try_recieve_count_max = 7;
    while (content_length < pre_length && try_recieve_count < try_recieve_count_max)
    {
        content_length += esp_http_client_read_user_response_user(_client, buf, pre_length);
        try_recieve_count += 1;
    }
    
    printf("content_length = %d pre_length = %d HTTP: CLOSE USER\n", content_length, pre_length);
    
    s32_t parse_res = parse_responce((u8_t*)buf, content_length);
    _close_client();
    
    if ( parse_res < 0)
        printf("ERROR: Cannot parse\n");
        
    return parse_res;
}


static void _close_client(void)
{
    printf("HTTP: CLOSE USE\n");
    esp_http_client_close_user(_client);
    _client_is_open = 0;
}


void http_create_msg(u8_t* buf, u16_t buf_len, u8_t pack_type)
{
    if (buf_len < BUF_SIZE_REPEAT_MSG)
    {// copy mesage to repeat if transmition fail
        memcpy(_msg_to_serv.buf, buf, buf_len);
        _msg_to_serv.buf_len = buf_len;
        _msg_to_serv.pack_type = pack_type;
        _msg_to_serv.repeat_count = SEND_ATTEMPS_MAX;
    }
}

// ret = 0 - success, ret < 0 error
static s32_t _http_send_to_serv(t_msg_to_serv msg)
{
    if (msg.repeat_count <= 0)
    {
        msg.pack_type = 0;
        printf("HTTP: max try count");
        return E_HTTP_ERROR_UNEXPECTED;
    }
    // DBG
    if (msg.repeat_count != SEND_ATTEMPS_MAX)
        printf("HTTTP: TRY RO REPEAT\n");

    msg.repeat_count -= 1;

    set_req_url(msg.pack_type);
    esp_http_client_set_timeout_ms_user(_client, 500);
    int result = -1;
        
    result = (msg.buf_len == 0) ? send_get_req() : send_post_req(msg.buf, msg.buf_len);

    if (result < 0 || result < msg.buf_len)
    {
        printf("ERROR: sending\n");
        return E_HTTP_ERROR_REPEAT_SEND;
    }
    return 0;
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
    _client_is_open = 1;
    return ESP_OK;
}


static int send_post_req(u8_t* buf, u16_t buf_len)
{
    esp_http_client_set_method_user(_client, HTTP_METHOD_POST);
    // if (_client_is_open)
    //     return esp_http_client_write_user(_client, (char*)buf, buf_len);

    esp_err_t err = esp_http_client_open_user(_client, buf_len);
    if (err != ESP_OK) {
        _client_is_open = 0;
        printf("ERROR: Send post, val - %d\n", err);
        return ((int)err < 0) ? (int)err : -(int)err;
    }
    printf("HTTP: OPEN USER\n");
    _client_is_open = 1;
    return esp_http_client_write_user(_client, (char*)buf, buf_len);
}


