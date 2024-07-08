#include "task_http.h"

#include "task_test_com.h"

#include "esp_http_client_user.h"

#include "base_types.h"

#include "crc.h"

#include "client_serv_prot.h"
#include "status.h"
#include "private.h"

#define MAX_HTTP_OUTPUT_BUFFER 25596

#define SEND_ATTEMPS_MAX 5

static int read_response(void);
static void send_to_serv(u8_t* buf, u16_t buf_len, u8_t pack_type);


static int send_get_req(void);
static int send_post_req(u8_t* buf, u16_t buf_len);

static esp_http_client_handle_t _client = { 0 };

t_track_list _track_list = { 0 };

#define BUF_SIZE_REPEAT_MSG 64
static struct _repeat_msg
{
    u8_t buf[BUF_SIZE_REPEAT_MSG];
    u16_t buf_len;
    u8_t pack_type;
} repeat_msg;

void polling_server(void);
void send_syn(void);
void send_statys(void);
void repeat_send(void);

void send_com_event(char* msg);
void register_com_event(void);
s32_t parse_responce(u8_t* buf, u16_t len);
u16_t create_pack(t_csp_head* phead, void* pbody, u16_t body_len, u8_t* res_buf, u16_t res_buf_len);
t_csp_head create_pack_head(u8_t e_msg_type, u16_t body_len);

void print_binary(u8_t* buf, u16_t len);
void set_req_url(u8_t pack_type);

void ack_proc(u8_t dev_id_pack, t_csp_ack* pack_body, u16_t len);
void switch_playlist(t_csp_track_req* ptrack_req, u16_t len);
void send_track_req(u16_t hash_track_name, u16_t pack_num);
u16_t read_track_data(t_csp_track_pack* ptrack_pack);

static u8_t device_id = 0;
static u8_t _dbg_block = 0;


void task_http(void *task_param)
{
    esp_http_client_config_t config = {
        .url = SERVER_ADDRES,
        .buffer_size = MAX_HTTP_OUTPUT_BUFFER
    };
    _client = esp_http_client_init_user(&config);

    register_com_event();

    while(1)
    {
        if ( !device_id)
            send_syn();
        else if ( !_dbg_block )
            send_statys();

        esp_http_client_set_timeout_ms_user(_client, 1);
        if (esp_http_client_poll_read_user(_client) > 0)
            read_response();

        if (_dbg_block)
            vTaskDelay(25 / portTICK_PERIOD_MS);
        else
            vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void send_syn(void)
{
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = create_pack_head(ECSP_CONNECT, sizeof(t_csp_connect));

    t_csp_connect body = { UNIQUE_DEVICE_ID };
    u16_t pack_len = create_pack(
        &head, &body, sizeof(t_csp_connect), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );
    if ( !pack_len )
    {
        printf("Cannot create pack buf to small\n");
        return;
    }
    send_to_serv(pack_buf, pack_len, head.msg_type);
}

// statys is stub just some data
void send_statys(void)
{
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = create_pack_head(ECSP_STATUS, sizeof(t_csp_status));
    t_csp_status body = { 
        .track_list_hash = {
            .prev   = 0xFFFF,
            .current = 0xFF00,
            .next   = 0x00FF,
        },
        .volume_lvl = 0,
        .devices = { .count = 0 }
    };

    u16_t pack_len = create_pack(
        &head, &body, sizeof(t_csp_status), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );

    send_to_serv((u8_t*)&pack_buf, pack_len, head.msg_type);
}


t_csp_head create_pack_head(u8_t msg_type, u16_t body_len)
{
    t_csp_head head = {
        .id = device_id, 
        .msg_type = msg_type, 
        .body_len = body_len, 
        .crc = 0
    };
    head.crc = crc8(0, &head, sizeof(t_csp_head) - 1);
    return head;
}


u16_t create_pack(
    t_csp_head* phead, void* pbody, u16_t body_len, 
    u8_t* res_buf, u16_t res_buf_len)
{
    if ( !phead || !pbody || !res_buf )
        return 0;
    if ( sizeof(t_csp_head) + body_len + sizeof(u16_t) > res_buf_len )
        return 0;

    memcpy(res_buf, phead, sizeof(t_csp_head));
    memcpy(res_buf + sizeof(t_csp_head), pbody, body_len);

    u16_t crc16_pack = crc16(0, res_buf, body_len + sizeof(t_csp_head));
    memcpy(
        res_buf + sizeof(t_csp_head) + body_len, 
        &crc16_pack, sizeof(u16_t)
    );

    return sizeof(t_csp_head) + body_len + sizeof(u16_t);
}


static int read_response(void)
{
    static char buf[MAX_HTTP_OUTPUT_BUFFER] = { 0 };
    
    esp_http_client_set_timeout_ms_user(_client, 200);
    printf("try to read\n");
    esp_http_client_fetch_headers_user(_client);
    int content_length = esp_http_client_read_user_response_user(_client, buf, MAX_HTTP_OUTPUT_BUFFER);
    // int content_length = esp_http_client_read(_client, buf, MAX_HTTP_OUTPUT_BUFFER);
    esp_http_client_close_user(_client);

    s32_t parse_res = parse_responce((u8_t*)buf, content_length);
    if ( parse_res < 0) 
        printf("ERROR: Cannot parse\n");
    return 0;
}


u8_t* integrity_check(u8_t* buf, u16_t len)
{
    if (buf == NULL)
        return NULL;

    t_csp_head* phead = (t_csp_head*)buf;
    
    if (phead->crc != crc8(0, phead, sizeof(t_csp_head) - 1))
        return NULL; // HEAD crc error

    u16_t crc16_pack = *((u16_t*)(buf + len - 2));
    if (crc16_pack != crc16(0, buf, len - 2))
        return NULL; // pack crc error

    // return pointer to the body
    return buf + sizeof(t_csp_head);
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

s32_t parse_responce(u8_t* buf, u16_t len)
{
    if (buf == NULL)
        return -1;

    static u8_t repeat_count = 0;
    t_csp_head* phead = (t_csp_head*)buf;
    // printf("id = %02X type = %02X size = %04X crc = %02X\n", 
    // phead->id, phead->msg_type, phead->body_len, phead->crc);
    u8_t* pbody = integrity_check(buf, len);

    if (pbody == NULL)
    {
        printf("ERROR: crc\n");
        if (repeat_count >= SEND_ATTEMPS_MAX)
            return -2;

        printf("WARNING: try repaet sending\n");
        repeat_send();
        repeat_count += 1;
        return 0;
    }
    repeat_count = 0;
    if (phead->id != device_id && device_id)
    {
        printf("Not to me\n");
        return -3;
    }
    
    switch (phead->msg_type)
    {
        case ECSP_CONNECT:
        case ECSP_DISCONNECT:
        case ECSP_STATUS:
        case ECSP_TRACK_DATA:
            printf("GET TRACK DATA\n");
            read_track_data((t_csp_track_pack*)pbody);
            if (((t_csp_track_pack*)pbody)->pack_num == ((t_csp_track_pack*)pbody)->pack_total - 1)
                break;
            send_track_req(
                ((t_csp_track_pack*)pbody)->track_id,
                ((t_csp_track_pack*)pbody)->pack_num + 1
            );
            break;
        case ECSP_COM_PAUSE:
        case ECSP_COM_RESUME:
        case ECSP_COM_NEXT:
        case ECSP_COM_PREV:
        case ECSP_COM_REPEAT:
        case ECSP_COM_VOL_INC:
        case ECSP_COM_VOL_DEC:
        case ECSP_COM_SWITCH_LIST:
            printf("SWITCH PLAY LIST\n");
            switch_playlist((t_csp_track_req*)pbody, phead->body_len);
            return phead->body_len;
        case ECSP_COM_GET_TRACK:
        case ECSP_ACK:
            printf("ACK\n");
            ack_proc(phead->id, (t_csp_ack*)pbody, phead->body_len);
        break;

        default:
            return -3;
    }

    // send_statys();
    return phead->body_len;
}


u16_t read_track_data(t_csp_track_pack* ptrack_pack)
{
    // if (ptrack_pack->track_format == ECSP_TRACK_FORMAT_MP3)
    //     printf("TRACK FROMAT: mp3\n");
    // else 
    //     printf("TRACK FROMAT: sbc\n");

    // printf("TRACK HASH: %04X\n", ptrack_pack->track_id);
    printf("TRACK PACK TOATAL: %d ", ptrack_pack->pack_total);
    printf("TRACK PACK NUMBER: %d\n\n", ptrack_pack->pack_num);
    if (ptrack_pack->pack_num == ptrack_pack->pack_total - 1)
    {
        printf("CONGRATULATIONS\nCONGRATULATIONS\n");
        _dbg_block = 0;
    }
    return ptrack_pack->pack_num + 1;
}


void switch_playlist(t_csp_track_req* ptrack_req, u16_t len)
{
    printf("track id = %04X\n", ptrack_req->track_id);
    printf("amount packs = %04X\n", ptrack_req->amount_packs);
    _track_list.next.hash_name = ptrack_req->track_id;
    _track_list.next.statys = TRACK_ST_TRANSMITTED;
    _track_list.next.size = (ptrack_req->amount_packs - 1) * MAX_TRACK_DATA;
    
    _dbg_block = 1;
    send_track_req(ptrack_req->track_id, 0);
}


void send_track_req(u16_t hash_track_name, u16_t pack_num)
{
    // printf("TRACK REQUEST: %d\n", pack_num);
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = create_pack_head(ECSP_COM_GET_TRACK, sizeof(t_csp_status));
    t_csp_track_req body = {
        .track_id = hash_track_name,
        .pack_num = pack_num
    };

    u16_t pack_len = create_pack(
        &head, &body, sizeof(t_csp_track_req), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );

    send_to_serv((u8_t*)&pack_buf, pack_len, head.msg_type);
}

void repeat_send(void)
{
    send_to_serv(repeat_msg.buf, repeat_msg.buf_len, repeat_msg.pack_type);
}


void ack_proc(u8_t dev_id_pack, t_csp_ack* pack_body, u16_t len)
{
    if ( !device_id && pack_body->last_msg_type == ECSP_CONNECT)
        device_id = dev_id_pack;
}


static void send_to_serv(u8_t* buf, u16_t buf_len, u8_t pack_type)
{
    if (buf_len < BUF_SIZE_REPEAT_MSG)
    {// copy mesage to repeat if transmition fail
        memcpy(repeat_msg.buf, buf, buf_len);
        repeat_msg.buf_len = buf_len;
        repeat_msg.pack_type = pack_type;
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


void set_req_url(u8_t pack_type)
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


void send_com_event(char* msg)
{
    t_event_com_data event_data = { {0}, 0 };
    if (strlen(msg) >= EVENT_MSG_SIZE_LIM)
        event_data.size = EVENT_MSG_SIZE_LIM;
    else
        event_data.size = strlen(msg);
    memcpy(event_data.str, msg, event_data.size);

    esp_err_t err = esp_event_post_to(
        event_com_get_handle(), EVENT_COM_BASE, E_EVENT_COM_PRINT, 
        &event_data,  sizeof(event_data), 50 / portTICK_PERIOD_MS
    );
    if (err != ESP_OK) {
        printf("Failed to post event to \"%s\" #%d: %d (%s)", EVENT_COM_BASE, EVENT_COM_ID, err, esp_err_to_name(err));
    }
}


void register_com_event(void)
{
    for (uint8_t event_id = E_EVENT_COM_PRINT; event_id < E_EVENT_COM_WAIT; event_id++)
    {
        esp_err_t err =  esp_event_handler_register_with(
            event_com_get_handle(), EVENT_COM_BASE, event_id, event_com, NULL
        );
        if (err != ESP_OK) {
            printf("Failed to register event handler for %s #%d", EVENT_COM_BASE, event_id);
        };
    }
}
