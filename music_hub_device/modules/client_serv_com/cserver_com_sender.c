#include "cserver_com_sender.h"
#include "cserver_com_receiver.h"
#include "task_http.h"

#include "client_serv_prot.h"
#include "private.h"
#include "crc.h"


static t_csp_head _create_pack_head(u8_t e_msg_type, u16_t body_len);
static u16_t _create_pack(t_csp_head* phead, void* pbody, u16_t body_len, u8_t* res_buf, u16_t res_buf_len);
// TODO: track list and other things should be in over module
t_csp_track_list* read_tracklist(t_csp_track_list* ptracklist_hash);


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
    http_send_to_serv(pack_buf, pack_len, head.msg_type);
}


// statys is stub just some data
void send_statys(void)
{
    u8_t pack_buf[ DEFAULT_PACK_LEN ] = { 0 };
    t_csp_head head = _create_pack_head(ECSP_STATUS, sizeof(t_csp_status));

    t_csp_track_list tracklist_hash = { 0 };
    get_csp_track_list(&tracklist_hash);
    printf("STATUS: curr = %04X next = %04X prev = %04X", 
        tracklist_hash.current, tracklist_hash.next, tracklist_hash.prev);

    t_csp_status body = { 
        .track_list_hash = {
            .prev   = tracklist_hash.prev,
            .current = tracklist_hash.current,
            .next   = tracklist_hash.next,
        },
        .volume_lvl = 0,
        .devices = { .count = 0 }
    };

    u16_t pack_len = _create_pack(
        &head, &body, sizeof(t_csp_status), 
        (u8_t*)&pack_buf, DEFAULT_PACK_LEN
    );

    http_send_to_serv((u8_t*)&pack_buf, pack_len, head.msg_type);
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

    http_send_to_serv((u8_t*)&pack_buf, pack_len, head.msg_type);
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

