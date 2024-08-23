#ifndef CSERVER_COM_RECEIVER_H
#define CSERVER_COM_RECEIVER_H

#include "base_types.h"
#include "client_serv_prot.h"


s32_t parse_responce(u8_t* buf, u16_t len);

t_csp_track_list* get_csp_track_list(t_csp_track_list* p_tracklist);

#endif /* CSERVER_COM_RECEIVER_H */
