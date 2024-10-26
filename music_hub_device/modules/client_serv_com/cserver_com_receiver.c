#include "cserver_com_receiver.h"
#include "cserver_com_sender.h"
#include "task_http.h"
#include "task_bt_dev.h"

#include "client_serv_prot.h"
#include "private.h"
#include "crc.h"
#include "status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


static s8_t _send_track_data_user(t_csp_track_pack* ptrack_pack);
static u16_t _read_track_data(t_csp_track_pack* ptrack_pack);
static s8_t _switch_playlist(t_csp_track_req* ptrack_req, u16_t len);
static void _ack_proc(u8_t dev_id_pack, t_csp_ack* pack_body, u16_t len);
static u8_t* _integrity_check(u8_t* buf, u16_t len);

// TODO: move to anover module
static t_track_list _track_list = { 0 };
extern volatile QueueHandle_t QueueHttpBtdev;


s32_t parse_responce(u8_t* buf, u16_t len)
{
    if (buf == NULL || len < sizeof(t_csp_head))
    {
        // return E_HTTP_ERROR_WAIT;
        return E_HTTP_ERROR_REPEAT_SEND;
    }
        

    t_csp_head* phead = (t_csp_head*)buf;    
    u8_t* pbody = _integrity_check(buf, len);

    if (pbody == NULL)
    {
        printf("ERROR: crc\n");
        printf("WARNING: try repaet sending\n");
        return E_HTTP_ERROR_REPEAT_SEND;
    }

    u8_t device_id = http_get_device_id();
    if (phead->id != device_id && device_id)
    {
        printf("Not to me\n");
        return E_HTTP_ERROR_UNEXPECTED;
    }
    
    confirm_receive();
    switch (phead->msg_type)
    {
        case ECSP_CONNECT:
        case ECSP_DISCONNECT:
        case ECSP_STATUS:
        case ECSP_TRACK_DATA:
            printf("PARSE: GET TRACK DATA\n");
            _read_track_data((t_csp_track_pack*)pbody);
            if (_send_track_data_user((t_csp_track_pack*)pbody) < 0)
            {
                printf("DO NOT KNOW WHAT TO DO\n");
                return E_HTTP_ERROR_REPEAT_SEND;
            }
                
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
            printf("PARSE: SWITCH PLAY LIST\n");
            if (_switch_playlist((t_csp_track_req*)pbody, phead->body_len) < 0)
                // http_set_status(E_HTTP_STATUS_IDEL);
                break;
            else
                break;
                // http_set_status(E_HTTP_STATUS_WORK);
            // break;

        case ECSP_COM_GET_TRACK:
        case ECSP_ACK:
            printf("PARSE: ACK\n");
            _ack_proc(phead->id, (t_csp_ack*)pbody, phead->body_len);
        break;

        default:
            return E_HTTP_ERROR_UNEXPECTED;
    }

    return phead->body_len;
}


static s8_t _send_track_data_user(t_csp_track_pack* ptrack_pack)
{
    const u8_t queue_send_timout = 500 / portTICK_PERIOD_MS;
    const u8_t queue_send_try_max = 50;
    u8_t send_try_count = 0;
    while (1)
    {
        if ( xQueueSend( QueueHttpBtdev, (void*)ptrack_pack, queue_send_timout ) == pdPASS )
        {
            printf("HTTP QUEUE: send successe\n");
            printf("track_num = %d\n\n", ptrack_pack->track_id);
            break;
        }
        send_try_count += 1;
        if (send_try_count >= queue_send_try_max)
        {
            printf("HTTP QUEUE: send FAILD\n");
            return -1;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    return 0;
}


static u16_t _read_track_data(t_csp_track_pack* ptrack_pack)
{
    printf("TRACK PACK TOATAL: %d ", ptrack_pack->pack_total);
    printf("TRACK PACK NUMBER: %d\n\n", ptrack_pack->pack_num);
    if (ptrack_pack->pack_num == ptrack_pack->pack_total - 1)
    {
        printf("CONGRATULATIONS\nCONGRATULATIONS\n");
    }
        
    return ptrack_pack->pack_num + 1;
}


static s8_t _switch_playlist(t_csp_track_req* ptrack_req, u16_t len)
{
    printf("track pos = %d track id = %04X\n", ptrack_req->track_pos, ptrack_req->track_id);
    printf("amount packs = %04X\n", ptrack_req->amount_packs);

    t_track* p_track = NULL;
    switch (ptrack_req->track_pos)
    {
        case ECSP_TRACK_POS_PREV: p_track = &_track_list.prev;      break;
        case ECSP_TRACK_POS_CURR: p_track = &_track_list.current;   break;
        case ECSP_TRACK_POS_NEXT: p_track = &_track_list.next;      break;
        default:
            p_track = &_track_list.next;
    }

    p_track->hash_name = ptrack_req->track_id;
    p_track->statys = TRACK_ST_TRANSMITTED;
    p_track->size = (ptrack_req->amount_packs - 1) * MAX_TRACK_DATA;
    
    send_track_req(ptrack_req->track_id, 0);
    return 0;
}



static void _ack_proc(u8_t dev_id_pack, t_csp_ack* pack_body, u16_t len)
{
    if ( !http_get_device_id() && pack_body->last_msg_type == ECSP_CONNECT)
        http_set_device_id(dev_id_pack);
}


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

// t_csp_track_list content only hash names
t_csp_track_list* get_csp_track_list(t_csp_track_list* p_tracklist)
{
    p_tracklist->prev = _track_list.prev.hash_name;
    p_tracklist->current = _track_list.current.hash_name;
    p_tracklist->next = _track_list.next.hash_name;
    return p_tracklist;
}
