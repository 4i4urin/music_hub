#include "cserver_com_receiver.h"
#include "cserver_com_sender.h"
#include "task_http.h"

#include "client_serv_prot.h"
#include "private.h"
#include "crc.h"
#include "status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


static u8_t _send_track_data_user(t_csp_track_pack* ptrack_pack);
static u16_t _read_track_data(t_csp_track_pack* ptrack_pack);
static void _switch_playlist(t_csp_track_req* ptrack_req, u16_t len);
static void _ack_proc(u8_t dev_id_pack, t_csp_ack* pack_body, u16_t len);
static u8_t* _integrity_check(u8_t* buf, u16_t len);

// TODO: move to anover module
static t_track_list _track_list = { 0 };
extern volatile QueueHandle_t QueueHttpSD;


// RECIVER FUNC
s32_t parse_responce(u8_t* buf, u16_t len)
{
    if (buf == NULL)
        return -1;

    t_csp_head* phead = (t_csp_head*)buf;    
    u8_t* pbody = _integrity_check(buf, len);

    if (pbody == NULL)
    {
        printf("ERROR: crc\n");
        printf("WARNING: try repaet sending\n");
        if (http_repeat_send() < 0)
            return -2;

        return 0;
    }

    u8_t device_id = http_get_device_id();
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
            _read_track_data((t_csp_track_pack*)pbody);
            _send_track_data_user((t_csp_track_pack*)pbody);
            http_set_status(E_HTTP_STATUS_WORK);

            if (((t_csp_track_pack*)pbody)->pack_num == ((t_csp_track_pack*)pbody)->pack_total - 1)
            {
                // TODO: ASK NEXT TRACK
                http_set_status(E_HTTP_STATUS_IDEL);
                // req_next_track();
                break;
            }
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
            http_set_status(E_HTTP_STATUS_WORK);
            _switch_playlist((t_csp_track_req*)pbody, phead->body_len);
            return phead->body_len;

        case ECSP_COM_GET_TRACK:
        case ECSP_ACK:
            printf("ACK\n");
            _ack_proc(phead->id, (t_csp_ack*)pbody, phead->body_len);
        break;

        default:
            return -3;
    }

    return phead->body_len;
}


// RECIVER FUNC
static u8_t _send_track_data_user(t_csp_track_pack* ptrack_pack)
{
    const u8_t queue_send_timout = 5;
    u8_t send_try_count = 0;
    while (1)
    {
        if ( xQueueSend( QueueHttpSD, (void*)ptrack_pack, queue_send_timout ) == pdPASS )
        {
            printf("HTTP QUEUE: send successe\n");
            printf("track_num = %d\n\n", ptrack_pack->track_id);
            break;
        }
        send_try_count += 1;
        if (send_try_count == 5)
        {
            printf("HTTP QUEUE: send FAILD\n");
            return -1;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    return 0;
}

// RECIVER FUNC
static u16_t _read_track_data(t_csp_track_pack* ptrack_pack)
{
    printf("TRACK PACK TOATAL: %d ", ptrack_pack->pack_total);
    printf("TRACK PACK NUMBER: %d\n\n", ptrack_pack->pack_num);
    if (ptrack_pack->pack_num == ptrack_pack->pack_total - 1)
        printf("CONGRATULATIONS\nCONGRATULATIONS\n");
        
    return ptrack_pack->pack_num + 1;
}

// RECIVER FUNC
static void _switch_playlist(t_csp_track_req* ptrack_req, u16_t len)
{
    printf("track id = %04X\n", ptrack_req->track_id);
    printf("amount packs = %04X\n", ptrack_req->amount_packs);
    _track_list.current.hash_name = ptrack_req->track_id;
    _track_list.current.statys = TRACK_ST_TRANSMITTED;
    _track_list.current.size = (ptrack_req->amount_packs - 1) * MAX_TRACK_DATA;
    
    send_track_req(ptrack_req->track_id, 0);
}


// RECIVER FUNC
static void _ack_proc(u8_t dev_id_pack, t_csp_ack* pack_body, u16_t len)
{
    if ( !http_get_device_id() && pack_body->last_msg_type == ECSP_CONNECT)
        http_set_device_id(dev_id_pack);
}


// RECIVER FUNC
static u8_t* _integrity_check(u8_t* buf, u16_t len)
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
