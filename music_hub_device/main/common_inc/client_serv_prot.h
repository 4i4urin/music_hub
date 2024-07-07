#ifndef CLIENT_SERV_PROT_H
#define CLIENT_SERV_PROT_H

#include "base_types.h"

// cs - client/server
// csp - client/server protocol


/* About protocol
*   MAX_MSG_SIZE - 2048 (head + body + crc) byte
*   HEAD_SIZE - 5 byte
*   MAX_DATA_SIZE - 2041 byte
*   CRC_SIZE - 2 byte
*
*   head 
*   body
*   crc16
*/


#pragma pack(push, 1)


#define ECSP_CONNECT        11      // client send to serv inf about itself (t_csp_connect)
#define ECSP_DISCONNECT     12      // client/serv disconnect from each other
#define ECSP_STATUS         13      // client status content: playing/prev/next track id, volume lvl, type of connection, (connected dev count)
#define ECSP_TRACK_DATA     14      // mp3 data of track

#define ECSP_COM_PAUSE      15      // pause music              ! no data
#define ECSP_COM_RESUME     16      // resume music             ! no data
#define ECSP_COM_NEXT       17      // turn on next track       ! no data
#define ECSP_COM_PREV       18      // turn on previous track   ! no data
#define ECSP_COM_REPEAT     19      // loop current track mode  ! no data
#define ECSP_COM_VOL_INC    20      // increase volume value    ! increase value
#define ECSP_COM_VOL_DEC    21      // decrease volume value    ! decrease value
#define ECSP_COM_SWITCH_LIST    22   // change track list       ! hash of first track in playlist
#define ECSP_COM_GET_TRACK  23     // command to start track transmition of track data

#define ECSP_ACK            24     // just acknowledgment 



// sizeof - 5 byte
typedef struct _t_csp_head
{
    u8_t id;    // id of device selected by server
    u8_t msg_type; // type of message enum e_csp_msg_types
    u16_t body_len; // size of body in message ! ( sizeof(t_csp_head), and msg_crc not included )
    u8_t crc; // checksum of the head 
} t_csp_head;


// ECSP_CONNECT
// send to serv unique devise number
typedef struct _t_csp_connect
{
    u32_t unique_num;
} t_csp_connect;


// ECSP_DISCONECT
// send to serv unique devise number TODO may be expand
typedef struct _t_csp_disconnect
{
    u32_t unique_num;
} t_csp_disconnect;


typedef struct _t_csp_track_list
{
    u16_t prev;
    u16_t current;
    u16_t next;
} t_csp_track_list;


typedef struct _t_csp_connect_dev
{
    u8_t count      : 2; // can be connected only 3 devices
    u8_t            : 3; // N/A
    u8_t usb_status : 1; // connect/disconnect
    u8_t blth_status : 1; // connect/disconnect
    u8_t analog_status : 1; // connect/disconnect
} t_csp_connect_dev;

// ECSP_STATUS
// client status playing/prev/next track id, volume lvl, type of connection, (connected dev count)
typedef struct _t_csp_status
{
    t_csp_track_list track_list_hash;
    u8_t volume_lvl;
    t_csp_connect_dev devices; 
} t_csp_status;


#define    ECSP_TRACK_FORMAT_MP3 1
#define    ECSP_TRACK_FORMAT_SBC 2

#define MAX_TRACK_DATA 25000
// ECSP_TRACK_DATA just data
typedef struct _t_csp_track_pack
{
    u8_t track_format;    // format of track file
    u16_t track_id;             // hash of track name
    u16_t pack_total;           // amount of track packs in the one track
    u16_t pack_num;             // number of track packege in sequence from 0 
    u8_t track[MAX_TRACK_DATA];    // binary track data
} t_csp_track_pack;


// ECSP_COM_GET_TRACK
typedef struct _t_csp_track_req
{
    u16_t track_id;
    union {
        u16_t pack_num;
        u16_t amount_packs;
    };    
} t_csp_track_req;


// ECSP_COM_VOL_INC ECSP_COM_VOL_DEC
typedef struct _t_csp_vol_diff
{
    u8_t difference;
} t_csp_vol_diff;

// ECSP_ACK 
// content last command
typedef struct _t_csp_ack
{
    u8_t last_msg_type;
} t_csp_ack;

#pragma pack(pop)


#endif /* CLIENT_SERV_PROT_H */
