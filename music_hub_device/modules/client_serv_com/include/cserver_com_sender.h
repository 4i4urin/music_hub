#ifndef CSERVER_COM_SENDER_H
#define CSERVER_COM_SENDER_H


#include "base_types.h"


void send_syn(void);
void send_statys(void);
void send_track_req(u16_t hash_track_name, u16_t pack_num);


#endif /* CSERVER_COM_SENDER_H */
