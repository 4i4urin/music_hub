#include "cserver_com_sender.h"
#include "cserver_com_receiver.h"
#include "task_http.h"

#include "client_serv_prot.h"
#include "private.h"
#include "crc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


static t_csp_head _create_pack_head(u8_t e_msg_type, u16_t body_len);
static u16_t _create_pack(t_csp_head* phead, void* pbody, u16_t body_len, u8_t* res_buf, u16_t res_buf_len);
static t_csp_connect_dev* _buid_devices_pack(t_csp_connect_dev* p_con_dev);
// TODO: track list and other things should be in over module
t_csp_track_list* read_tracklist(t_csp_track_list* ptracklist_hash);


extern volatile QueueHandle_t QueueHttpBtStatus;
static t_csp_connect_dev _speakers = { 0 };

void send_syn(void)
{
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = _create_pack_head(ECSP_CONNECT, sizeof(t_csp_connect));

    t_csp_connect body = { UNIQUE_DEVICE_ID };
    u16_t pack_len = _create_pack(
        &head, &body, sizeof(t_csp_connect), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );
    if ( !pack_len )
    {
        printf("Cannot create pack buf to small\n");
        return;
    }
    http_create_msg(pack_buf, pack_len, head.msg_type);
}


// statys is stub just some data
void send_statys(void)
{
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = _create_pack_head(ECSP_STATUS, sizeof(t_csp_status));

    t_csp_track_list tracklist_hash = { 0 };
    get_csp_track_list(&tracklist_hash);
    printf("STATUS: curr = %04X next = %04X prev = %04X\n", 
        tracklist_hash.current, tracklist_hash.next, tracklist_hash.prev);

    t_csp_status body = { 
        .track_list_hash = {
            .prev   = tracklist_hash.prev,
            .current = tracklist_hash.current,
            .next   = tracklist_hash.next,
        },
        .volume_lvl = 0,
        .devices = { 0 }
    };
    _buid_devices_pack(&_speakers);
    body.devices = _speakers;
    printf("Device = %d\n", body.devices.count);

    u16_t pack_len = _create_pack(
        &head, &body, sizeof(t_csp_status), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );

    http_create_msg((u8_t*)&pack_buf, pack_len, head.msg_type);
}


void send_track_req(u16_t hash_track_name, u16_t pack_num)
{
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = _create_pack_head(ECSP_COM_GET_TRACK, sizeof(t_csp_status));
    t_csp_track_req body = {
        .track_id = hash_track_name,
        .pack_num = pack_num
    };

    u16_t pack_len = _create_pack(
        &head, &body, sizeof(t_csp_track_req), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );

    http_create_msg((u8_t*)&pack_buf, pack_len, head.msg_type);
}


static t_csp_head _create_pack_head(u8_t msg_type, u16_t body_len)
{
    t_csp_head head = {
        .id = http_get_device_id(), 
        .msg_type = msg_type, 
        .body_len = body_len, 
        .crc = 0
    };
    head.crc = crc8(0, &head, sizeof(t_csp_head) - 1);
    return head;
}


static u16_t _create_pack(
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


static t_csp_connect_dev* _buid_devices_pack(t_csp_connect_dev* p_con_dev)
{
    if (!uxQueueMessagesWaiting(QueueHttpBtStatus))
        return p_con_dev;

    const u8_t queue_recive_timout = 10;
    u8_t bt_connect = 0;

    portBASE_TYPE xStatus = xQueueReceive( QueueHttpBtStatus, &bt_connect, queue_recive_timout );

    if ( xStatus != pdPASS )
    {
        printf("HTTP QUEUE: can't read status queue\n");
        p_con_dev->count = 0;
        p_con_dev->blth_status = p_con_dev->analog_status = p_con_dev->usb_status = 0;
        return p_con_dev;
    }
    printf("HTTP QUEUE: read status queue bt_connect = %d\n", bt_connect);
    p_con_dev->count = bt_connect;
    p_con_dev->blth_status = 1;
    return p_con_dev;
}
